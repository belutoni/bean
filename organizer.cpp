#include "organizer.h"

#include <chrono>
#include <thread>
#include <algorithm>
#include <ranges>
#include <vector>

#include "logger.h"
#include "winpp.h"

org::Organizer::Organizer() {
    m_stop_event = winpp::Handle(CreateEventW(nullptr, true, false, nullptr));
    if (!m_stop_event) {
        logger::error("CreateEventW failed for the stop event. ({:#X})", GetLastError());
        throw std::runtime_error("CreateEventW failed for the stop event.");
    }

    wchar_t* known_folder_path{ nullptr };
    if (auto const status{ SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &known_folder_path) };
        status != S_OK) {
        logger::error("SHGetKnownFolderPath was unable to locate the Downloads folder. ({:#X})", static_cast<DWORD>(status));

        CoTaskMemFree(known_folder_path);
        throw std::runtime_error("SHGetKnownFolderPath was unable to locate the Downloads folder.");
    }

    m_downloads_folder_path.emplace(known_folder_path);
    CoTaskMemFree(known_folder_path);
}

void org::Organizer::stop() const {
    if (m_stop_event) {
        SetEvent(m_stop_event.get());
    }
}

org::MoveResult org::Organizer::move_file(std::filesystem::path const& source, std::filesystem::path const& destination) {
    int constexpr max_name_attempts{ 1000 };

    auto const parent{ destination.parent_path() };
    auto const stem{ destination.stem().native() };
    auto const ext{ destination.extension().native() };

    // MoveFileExW without MOVEFILE_REPLACE_EXISTING fails if the destination exists,
    // which makes the unique-name probing atomic (no check-then-move race).
    // MOVEFILE_COPY_ALLOWED lets Windows copy and delete when crossing volumes.
    std::filesystem::path candidate{ destination };
    for (int counter = 1; counter <= max_name_attempts; ++counter) {
        if (MoveFileExW(source.c_str(), candidate.c_str(), MOVEFILE_COPY_ALLOWED)) {
            logger::info("Moved file({}) to destination({}).", source, candidate);
            return MoveResult::Success;
        }

        DWORD const last_error{ GetLastError() };
        if (last_error == ERROR_FILE_EXISTS || last_error == ERROR_ALREADY_EXISTS) {
            candidate = parent / std::format(L"{} ({}){}", stem, counter, ext);
            continue;
        }

        // ACCESS_DENIED is ambiguous: it can mean a lock or a real permission problem.
        // It is treated as retryable, so a genuine permission error is only dropped
        // after QueuedFile::max_retries attempts.
        if (last_error == ERROR_SHARING_VIOLATION || last_error == ERROR_LOCK_VIOLATION || last_error == ERROR_ACCESS_DENIED) {
            logger::warn("File({}) is locked or busy. Will retry later.", source);
            return MoveResult::FileLocked;
        }

        logger::warn("Failed to move file({}) to destination({}). {:#X}", source, candidate, last_error);
        return MoveResult::Failed;
    }

    return MoveResult::Failed;
}

bool org::Organizer::cache_folder_id_to_path(REFKNOWNFOLDERID folder_id) {
    DWORD constexpr flags{ 0 };

    wchar_t* known_folder_path{ nullptr };
    if (auto const status{ SHGetKnownFolderPath(folder_id, flags, nullptr, &known_folder_path) };
        status != S_OK) {
        logger::warn("SHGetKnownFolderPath was unable to locate the folder specified by KNOWNFOLDERID({}). {:#X}",
            get_string_guid(folder_id), static_cast<DWORD>(status));
        CoTaskMemFree(known_folder_path);
        return false;
    }

    m_folder_id_to_path[folder_id] = std::filesystem::path(known_folder_path);
    CoTaskMemFree(known_folder_path);

    return true;
}

void org::Organizer::organize() {
    if (!m_downloads_folder_path.has_value()) {
        logger::error("Constructor was unable to provide a Downloads folder.");
        return;
    }

    std::error_code iter_ec;
    std::filesystem::directory_iterator dir_it{ m_downloads_folder_path.value(), iter_ec };
    std::filesystem::directory_iterator const end_it{};

    if (iter_ec) {
        logger::warn("Failed to iterate through the Downloads folder({}) due to an unhandled error code. "
            "Maybe the message provided by the std library can help: {}", m_downloads_folder_path.value(), iter_ec.message());
        return;
    }

    for (; dir_it != end_it; dir_it.increment(iter_ec)) {
        if (iter_ec) {
            iter_ec.clear();
            continue;
        }

        std::error_code ec;
        if (!dir_it->is_regular_file(ec) || ec) {
            continue;
        }

        auto const& source{ dir_it->path() };

        // Ignore items already present in the queue.
        bool const already_queued{ std::ranges::any_of(m_retry_move_queue, [&](QueuedFile const& item) {
            return item.source == source;
            })
        };

        if (already_queued)
            continue;

        // Get the lowercase version of the extension.
        std::wstring const wide_extension{ source.extension().native() };
        std::string extension;
        extension.reserve(wide_extension.size());
        bool is_ascii{ true };
        for (wchar_t const wc : wide_extension) {
            if (wc > 127) {
                is_ascii = false;
                break;
            }

            char c{ static_cast<char>(wc) };
            if (c >= 'A' && c <= 'Z')
                c += ('a' - 'A');

            extension.push_back(c);
        }

        // Only accept ASCII extensions.
        if (!is_ascii)
            continue;

        auto folder_id_iterator{ m_extension_to_folder_id.find(extension) };
        if (folder_id_iterator == m_extension_to_folder_id.end())
            continue;

        auto const& folder_id{ folder_id_iterator->second };
        if (!m_folder_id_to_path.contains(folder_id) && !cache_folder_id_to_path(folder_id_iterator->second))
            continue;

        // The path is already cacheded here
        auto const& new_path{ m_folder_id_to_path[folder_id] };
        auto const base_dest{ new_path / source.filename() };

        if (move_file(source, base_dest) == MoveResult::FileLocked) {
            m_retry_move_queue.emplace_back(source, base_dest);
        }
    }
}

void org::Organizer::process_move_queue() {
	// Take ownership of the queue to prevent iterator invalidation during the loop. 
    // Items that need to be retried are pushed back into the queue.
    auto pending_items{ std::move(m_retry_move_queue) };
    m_retry_move_queue.clear();

    for (auto& item : pending_items) {
        std::error_code ec;
        if (!std::filesystem::exists(item.source, ec) || ec) {
            logger::debug("File({}) no longer exists. Removing from retry queue.", item.source);
            continue;
        }

        switch (move_file(item.source, item.destination)) {
        case MoveResult::Success:
            logger::debug("File({}) moved successfully. Removing from retry queue.", item.source);
            break;

        case MoveResult::FileLocked:
            if (--item.retries_left <= 0) {
                logger::warn("File({}) could not be moved after {} retries. Removing from retry queue.",
                    item.source, QueuedFile::max_retries);
                break;
            }

            logger::debug("File({}) is still locked. {} retries left.", item.source, item.retries_left);
            m_retry_move_queue.push_back(std::move(item));
            break;

        case MoveResult::Failed:
            logger::warn("File({}) failed to move with a non-retryable error. Removing from retry queue.", item.source);
            break;
        }
    }
}

void org::Organizer::watch() {
    if (!m_downloads_folder_path.has_value()) {
        logger::error("Constructor was unable to provide a Downloads folder.");
        return;
    }

    // Create the notification handle before the first organize() so that files
    // arriving during the initial pass are not missed.
    winpp::ChangeNotificationHandle change_notification_handle{ FindFirstChangeNotificationW(m_downloads_folder_path->c_str(), false, FILE_NOTIFY_CHANGE_FILE_NAME) };
    if (!change_notification_handle) {
        logger::error("FindFirstChangeNotificationW failed to create the file_notify_handle. {:#X}", GetLastError());
        return;
    }

    organize();

    HANDLE wait_handles[2] = { m_stop_event.get(), change_notification_handle.get() };

    // Trim the working set after start-up to reduce the idle memory footprint.
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<size_t>(-1), static_cast<size_t>(-1));
    while (true) {
        // 'Sleep' until a notification is given. Wake up every 1000ms if there
        // are files waiting in the queue.
        DWORD const wait_time{ m_retry_move_queue.empty() ? INFINITE : 1000 };
        DWORD const wait_status{ WaitForMultipleObjects(2, wait_handles, false, wait_time) };

        switch (wait_status) {
            // m_stop_event
        case WAIT_OBJECT_0:
            return;

            // change_notification_handle
        case WAIT_OBJECT_0 + 1:
            if (!FindNextChangeNotification(change_notification_handle.get())) {
                logger::error("FindNextChangeNotification failed to rearm the file_notify_handle. {:#X}", GetLastError());
                return;
            }

            // Short debounce so a burst of changes (or a file just finishing its
            // rename) settles before we scan. Waiting on the stop event instead of
            // sleeping keeps shutdown responsive.
            if (WaitForSingleObject(m_stop_event.get(), 500) == WAIT_OBJECT_0)
                return;

            process_move_queue();
            organize();

            break;

        case WAIT_TIMEOUT:
            process_move_queue();
            break;

        default:
            return;
        }
    }
}