#include "organizer.h"

#include <chrono>
#include <thread>
#include <algorithm>
#include <ranges>

Organizer::Organizer() {
    m_stop_event = CreateEventW(nullptr, true, false, nullptr);
    if (!m_stop_event) {
        std::println(stderr, "Failed to create stop event: {:#X}.", GetLastError());
        return;
    }

    wchar_t* known_folder_path{ nullptr };
    if (auto const status{ SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &known_folder_path) };
        status != S_OK) {
        std::println(stderr, "Failed to locate Downloads folder: {:#x}.", static_cast<DWORD>(status));
        CoTaskMemFree(known_folder_path);
        return;
    }

    m_downloads_folder_path.emplace(known_folder_path);
    CoTaskMemFree(known_folder_path);
}

Organizer::~Organizer() {
    if (m_stop_event) {
        CloseHandle(m_stop_event);
        m_stop_event = nullptr;
    }
}

void Organizer::stop() const {
    if (m_stop_event) {
        SetEvent(m_stop_event);
    }
}

bool Organizer::move_file(std::filesystem::path const& current_path, std::filesystem::path const& dest_path) {
    std::error_code ec;
    std::filesystem::rename(current_path, dest_path, ec);
    if (!ec)
        return false;

    // Move failed due to the file being locked by another process.
    if (ec.value() == ERROR_SHARING_VIOLATION
        || ec.value() == ERROR_LOCK_VIOLATION
        || ec.value() == ERROR_ACCESS_DENIED) {
        std::println("File locked: {}. Added to retry queue.", current_path);

        m_retry_queue.emplace_back(current_path, dest_path);
        return false;
        }

    // Move failed due to destination being on a different disk
    // Copy and delete file works across disks
    if (ec.value() == ERROR_NOT_SAME_DEVICE) {
        if (!copy_and_delete_file_to(current_path, dest_path)) {
            std::println(stderr, "Could not copy and delete {} to {}: {}.",
                current_path,
                dest_path,
                GetLastError());

            return false;
        }
    }

    // Otherwise just ignore it
    std::println(stderr, "Could not move {} to {}: {}.",
        current_path,
        dest_path,
        ec.message());

    return true;
}

bool Organizer::copy_and_delete_file(std::filesystem::path const& current_path, std::filesystem::path const& dest_path) {
    std::error_code ec;
    std::filesystem::copy(current_path, dest_path, ec);
    if (!ec) {
        std::filesystem::remove(current_path, ec);
        if (ec) {
            std::println(stderr, "Could not delete file {}: {}", current_path, ec.value());
        }

        // todo()
        return true;
    }

    // Copy failed due to the file being locked by another process.
    if (ec.value() == ERROR_SHARING_VIOLATION
        || ec.value() == ERROR_LOCK_VIOLATION
        || ec.value() == ERROR_ACCESS_DENIED) {
        std::println("Copy failed: File locked: {}. Added to retry queue.", current_path);

        m_retry_queue.emplace_back(current_path, dest_path);
        return false;
    }

    return true;
}

bool Organizer::cache_folder_id_to_path(REFKNOWNFOLDERID folder_id) {
    wchar_t* known_folder_path{ nullptr };
    if (auto const status{ SHGetKnownFolderPath(folder_id, 0, nullptr, &known_folder_path) };
        status != S_OK) {
        std::println(stderr, "Failed to locate folder specified by {}: {:#x}.",
            print_guid_modern(folder_id),
            static_cast<DWORD>(status));
        CoTaskMemFree(known_folder_path);
        return false;
    }

    m_folder_id_to_path[folder_id] = std::filesystem::path(known_folder_path);
    CoTaskMemFree(known_folder_path);
    return true;
}

std::filesystem::path Organizer::get_unique_destination(std::filesystem::path const& dest_path) {
    if (!std::filesystem::exists(dest_path)) {
        return dest_path;
    }

    auto const parent = dest_path.parent_path();
    auto const stem = dest_path.stem().string();
    auto const ext = dest_path.extension().string();

    int counter = 1;
    std::filesystem::path candidate;
    do {
        candidate = parent / std::format("{} ({}){}", stem, counter++, ext);
    } while (std::filesystem::exists(candidate));

    return candidate;
}

void Organizer::organize() {
    if (!m_downloads_folder_path.has_value()) {
        std::println(stderr, "Downloads folder path is missing, cannot organize.");
        return;
    }

    std::error_code iter_ec;
    for (auto const& dir_entry : std::filesystem::directory_iterator{ m_downloads_folder_path.value(), iter_ec }) {
        if (iter_ec || !dir_entry.is_regular_file()) 
            continue;

        auto const& current_path_for_file{ dir_entry.path() };

        // Ignore items already present in the queue.
        bool const already_queued = std::ranges::any_of(m_retry_queue, [&](QueuedFile const& item) {
            return item.source == current_path_for_file;
        });
        if (already_queued) 
            continue;

        // Get the lowercase version of the extension.
        std::string extension{ current_path_for_file.extension().string()};
        for (char& c : extension) {
            if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
        }

        // Ignore unknown extensions.
        auto folder_id_iterator{ m_extension_to_folder_id.find(extension) };
        if (folder_id_iterator == m_extension_to_folder_id.end()) continue;

        auto new_path_iterator{ m_folder_id_to_path.find(folder_id_iterator->second) };
        if (new_path_iterator == m_folder_id_to_path.end()) {
            if (!cache_folder_id_to_path(folder_id_iterator->second)) {
                std::println(stderr, "GUID {} was not able to get cached!", print_guid_modern(folder_id_iterator->second));
                continue;
            }
        }

        // Guaranteed to have new_path here.
        auto const& new_path{ m_folder_id_to_path[folder_id_iterator->second] };
        auto const dest_file{ get_unique_destination(new_path / current_path_for_file.filename()) };

        if (!move_file(current_path_for_file, dest_file))
            continue;

        std::println("[X] {} -> {}.", current_path_for_file, dest_file);
    }
}

void Organizer::process_retry_queue() {
    std::error_code ec;
    std::erase_if(m_retry_queue, [&](QueuedFile& item) {
        // File no longer exists.
        if (!std::filesystem::exists(item.source))
            return true;

        auto const final_dest = get_unique_destination(item.destination);

        // File moved succesfully.
        if (move_file(item.source, final_dest)) {
            std::println("[X] (Retry) {} -> {}.", item.source, final_dest);
            return true;
        }

        --item.retries_left;

        // No more attempts left
        if (item.retries_left <= 0) {
            std::println(stderr, "Could not move {} to {} (file lock): {}.",
                item.source,
                final_dest,
                ec.message());
            return true;
        }

        // Try again later
        return false;
    });
}

void Organizer::watch() {
    if (!m_downloads_folder_path.has_value()) {
        std::println(stderr, "Cannot watch: Downloads folder path is missing.");
        return;
    }

    organize();

    HANDLE change_notification_handle{ FindFirstChangeNotificationW(m_downloads_folder_path->c_str(), false, FILE_NOTIFY_CHANGE_FILE_NAME) };

    if (change_notification_handle == INVALID_HANDLE_VALUE) {
        std::println(stderr, "Failed to initialize directory watcher: {:#x}.", GetLastError());
        return;
    }

    HANDLE wait_handles[2] = { m_stop_event, change_notification_handle };

    while (true) {
        // Get rid of useless start-up code.
        SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));

        // 'Sleep' until a notification is given. Wake up every 1000ms if there
        // are files waiting in the queue.
        DWORD wait_time{ m_retry_queue.empty() ? INFINITE : 1000 };
        DWORD wait_status{ WaitForMultipleObjects(2, wait_handles, false, wait_time) };

        switch (wait_status) {
            // m_stop_event
            case WAIT_OBJECT_0:
                FindCloseChangeNotification(change_notification_handle);
                return;

            // change_notification_handle
            case WAIT_OBJECT_0 + 1: 
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                organize();
                process_retry_queue();   

                if (!FindNextChangeNotification(change_notification_handle)) {
                    std::println(stderr, "Failed to rearm change notification.");
                    FindCloseChangeNotification(change_notification_handle);
                    return;
                }

                break;

            case WAIT_TIMEOUT:
                process_retry_queue();
                break;

            default:
                FindCloseChangeNotification(change_notification_handle);
                return;
        }
    }
}