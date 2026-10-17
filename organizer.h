#pragma once

#include <unordered_map>
#include <string>
#include <filesystem>
#include <optional>
#include <format>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <system_error>
#include <shlobj_core.h>

#include "winpp.h"

/// <summary>
/// Formatter that allows std::filesystem::path to be used with std::format.
/// The path is converted to UTF-8 before being formatted as a string_view,
/// so standard string format specs are supported.
/// </summary>
template <>
struct std::formatter<std::filesystem::path> : std::formatter<std::string_view> {
    /// <summary>
    /// Formats the given path as a UTF-8 string.
    /// </summary>
    /// <param name="p">The path to format.</param>
    /// <param name="ctx">The format context to write the output to.</param>
    /// <returns>An iterator to the end of the formatted output.</returns>
    auto format(std::filesystem::path const& p, std::format_context& ctx) const {
        auto u8{ p.u8string() };
        std::string_view sv(reinterpret_cast<char const*>(u8.data()), u8.size());
        return std::formatter<std::string_view>::format(sv, ctx);
    }
};


namespace org {
    /// <summary>
    /// Hash functor for KNOWNFOLDERID keys, used by unordered containers.
    /// </summary>
    struct KnownFolderIdHash {
        /// <summary>
        /// Computes the hash of a KNOWNFOLDERID using its first data member,
        /// which is already well distributed.
        /// </summary>
        /// <param name="guid">The known folder ID to hash.</param>
        /// <returns>The hash value.</returns>
        size_t operator()(KNOWNFOLDERID const& guid) const noexcept {
            return guid.Data1;
        }
    };

    /// <summary>
    /// Equality functor for KNOWNFOLDERID keys, used by unordered containers.
    /// </summary>
    struct KnownFolderIdEqual {
        /// <summary>
        /// Compares two KNOWNFOLDERIDs for equality.
        /// </summary>
        /// <param name="lhs">The first known folder ID.</param>
        /// <param name="rhs">The second known folder ID.</param>
        /// <returns>True if both IDs are identical; false otherwise.</returns>
        bool operator()(KNOWNFOLDERID const& lhs, KNOWNFOLDERID const& rhs) const noexcept {
            return InlineIsEqualGUID(lhs, rhs) != 0;
        }
    };

    /// <summary>
    /// Describes a file whose move was postponed because it was locked or busy,
    /// along with the number of attempts that remain.
    /// </summary>
    struct QueuedFile {
        /// <summary>
        /// The current path of the file.
        /// </summary>
        std::filesystem::path source{};

        /// <summary>
        /// The desired target path of the file (before any unique suffix is applied).
        /// </summary>
        std::filesystem::path destination{};

        /// <summary>
        /// The maximum number of retry attempts for a queued file.
        /// </summary>
        static int constexpr max_retries{ 5 };

        /// <summary>
        /// The number of retry attempts left before the file is dropped from the queue.
        /// </summary>
        int retries_left{ max_retries };
    };

    /// <summary>
    /// The outcome of an attempt to move a file.
    /// </summary>
    enum class MoveResult {
        /// <summary>
        /// The file was moved successfully.
        /// </summary>
        Success,

        /// <summary>
        /// The move failed with an error that retrying will not fix.
        /// </summary>
        Failed,

        /// <summary>
        /// The move failed because the file is locked or in use; it may succeed later.
        /// </summary>
        FileLocked
    };

    /// <summary>
    /// Singleton that organizes the Downloads folder by moving files into the
    /// matching known folders (Pictures, Documents, Videos) based on file extension.
    /// </summary>
    class Organizer {
    public:
        /// <summary>
        /// Thread-safe access to the singleton instance.
        /// </summary>
        [[nodiscard]] static Organizer& instance() {
            static Organizer instance;
            return instance;
        }

        // Delete copy/move operations
        Organizer(Organizer const&) = delete;
        Organizer& operator=(Organizer const&) = delete;
        Organizer(Organizer&&) = delete;
        Organizer& operator=(Organizer&&) = delete;

        /// <summary>
        /// Iterates through the Downloads folder and moves the files
        /// to their corresponding folder by matching the file extension.
        /// </summary>
        void organize();

        /// <summary>
        /// Blocks the current thread and monitors the Downloads folder,
        /// triggering the organize action whenever changes occur.
        /// </summary>
        void watch();

        /// <summary>
        /// Signals the watcher loop to terminate gracefully.
        /// </summary>
        void stop() const;
    private:
        /// <summary>
        /// Private constructor to prevent direct instantiation.
        /// Resolves the Downloads folder path upon first access.
        /// </summary>
        Organizer();

        /// <summary>
        /// Moves the file to the given destination by using the Win32 API.
        /// </summary>
        /// <param name="source">The current path for the file.</param>
        /// <param name="destination">The desired target path for the file.</param>
        /// <returns>MoveResult indicating the outcome of the operation.</returns>
        [[nodiscard]] MoveResult move_file(std::filesystem::path const& source, std::filesystem::path const& destination);

        /// <summary>
        /// A handle to the stop event used to signal the termination of the watcher loop.
        /// </summary>
        winpp::Handle m_stop_event{ nullptr };

        /// <summary>
        /// Converts a KNOWNFOLDERID into its canonical string form,
        /// e.g. {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}. Used for logging.
        /// </summary>
        /// <param name="rfid">The known folder ID to convert.</param>
        /// <returns>The GUID formatted as a string.</returns>
        [[nodiscard]] static std::string get_string_guid(REFKNOWNFOLDERID rfid);

        /// <summary>
        /// Resolves the folder path for the specified known folder ID and caches
        /// it for faster access on subsequent requests.
        /// </summary>
        /// <param name="folder_id">The KNOWNFOLDERID of the requested folder.</param>
        /// <returns>True if the path was successfully resolved and cached; false otherwise.</returns>
        [[nodiscard]] bool cache_folder_id_to_path(REFKNOWNFOLDERID folder_id);

        /// <summary>
        /// Re-attempts to move previously locked or busy files, removing items
        /// that succeed, exceed their retry limit, or no longer exist.
        /// </summary>
        void process_move_queue();

        /// <summary>
        /// Holds files that failed to move due to sharing violations or active locks,
        /// awaiting deferred retry attempts.
        /// </summary>
        std::vector<QueuedFile> m_retry_move_queue{};

        /// <summary>
        /// Stores the path to the downloads folder if the constructor succeeds in getting it.
        /// </summary>
        std::optional<std::filesystem::path> m_downloads_folder_path{};

        /// <summary>
        /// Caches resolved filesystem paths associated with their known folder IDs
        /// to avoid redundant shell lookups.
        /// </summary>
        std::unordered_map<KNOWNFOLDERID,
            std::filesystem::path,
            KnownFolderIdHash,
            KnownFolderIdEqual> m_folder_id_to_path{};

        /// <summary>
        /// Maps supported file extensions to their corresponding target known folder IDs.
        /// </summary>
        static inline std::unordered_map<std::string, KNOWNFOLDERID> const m_extension_to_folder_id{
            // Pictures
            { ".png",   FOLDERID_Pictures },
            { ".jpeg",  FOLDERID_Pictures },
            { ".jpg",   FOLDERID_Pictures },
            { ".gif",   FOLDERID_Pictures },
            { ".bmp",   FOLDERID_Pictures },
            { ".webp",  FOLDERID_Pictures },
            // Documents
            { ".pdf",   FOLDERID_Documents },
            { ".docx",  FOLDERID_Documents },
            { ".doc",   FOLDERID_Documents },
            { ".xlsx",  FOLDERID_Documents },
            { ".xls",   FOLDERID_Documents },
            { ".pptx",  FOLDERID_Documents },
            { ".txt",   FOLDERID_Documents },
            // Videos
            { ".mp4",   FOLDERID_Videos },
            { ".mov",   FOLDERID_Videos },
            { ".mkv",   FOLDERID_Videos },
            { ".avi",   FOLDERID_Videos }
        };
    };
}