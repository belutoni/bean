#pragma once

#include <unordered_map>
#include <string>
#include <filesystem>
#include <optional>
#include <format>
#include <print>
#include <string_view>

// Windows-related headers
#include <shlobj_core.h>
#include <rpc.h>
#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "Shell32.lib")

// ??????????????????????????????????????????????????????
template <>
struct std::formatter<std::filesystem::path> : std::formatter<std::string_view> {
    auto format(const std::filesystem::path& p, std::format_context& ctx) const {
        auto u8{ p.u8string() };
        std::string_view sv(reinterpret_cast<const char*>(u8.data()), u8.size());
        return std::formatter<std::string_view>::format(sv, ctx);
    }
};

struct KnownFolderIdHash {
    size_t operator()(const KNOWNFOLDERID& guid) const noexcept {
        RPC_STATUS status;
        unsigned short hash_value{ UuidHash(const_cast<UUID*>(&guid), &status) };
        return static_cast<size_t>(hash_value);
    }
};

struct KnownFolderIdEqual {
    bool operator()(const KNOWNFOLDERID& lhs, const KNOWNFOLDERID& rhs) const noexcept {
        return InlineIsEqualGUID(lhs, rhs) != 0;
    }
};

struct QueuedFile {
    std::filesystem::path source{};
    std::filesystem::path destination{};
    static constexpr int max_retries{ 5 };
    int retries_left{ max_retries };
};

class Organizer {
public:
    /// <summary>
    /// Thread-safe access to the singleton instance.
    /// </summary>
    [[nodiscard]] static Organizer& instance() {
        static Organizer instance;
        return instance;
    }

    ~Organizer();

    // Delete copy/move operations
    Organizer(const Organizer&) = delete;
    Organizer& operator=(const Organizer&) = delete;
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
    /// 
    /// </summary>
    HANDLE m_stop_event{ nullptr };

    [[nodiscard]] static std::string print_guid_modern(REFKNOWNFOLDERID rfid) {
        std::string guid_str{ std::format(
            "{{{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}}}",
            rfid.Data1, rfid.Data2, rfid.Data3,
            rfid.Data4[0], rfid.Data4[1], rfid.Data4[2], rfid.Data4[3],
            rfid.Data4[4], rfid.Data4[5], rfid.Data4[6], rfid.Data4[7]
        ) };

        return guid_str;
    }
    
    /// <summary>
    /// Generates a unique destination path by appending an incremental counter
    /// to the filename if a file already exists at the target location.
    /// </summary>
    /// <param name="dest_path">The desired target path for the file.</param>
    /// <returns>A collision-free filesystem path.</returns>
    [[nodiscard]] static std::filesystem::path get_unique_destination(const std::filesystem::path& dest_path);

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
    void process_retry_queue();

    /// <summary>
    /// Holds files that failed to move due to sharing violations or active locks,
    /// awaiting deferred retry attempts.
    /// </summary>
    std::vector<QueuedFile> m_retry_queue{};

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
    static inline const std::unordered_map<std::string, KNOWNFOLDERID> m_extension_to_folder_id{
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