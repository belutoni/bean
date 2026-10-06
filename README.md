# bean

**bean** is an ultra-lightweight, native Windows background file organizer for your Downloads folder written in modern C++23. 

Instead of polling your disk on an aggressive interval, `bean` leverages the Win32 Directory Change Notification API to react instantly when files arrive. It automatically routes files into their appropriate Windows Known Folders (Pictures, Documents, Videos, Music, etc.) while handling in-progress downloads, file locks, and name collisions gracefully.

***

## Features

* **Native Windows Known Folders Integration:** Resolves system folder paths dynamically via `SHGetKnownFolderPath` (respecting customized or relocated folder locations).
* **Event-Driven File Watching:** Uses Win32 `FindFirstChangeNotificationW` and `WaitForMultipleObjects` rather than CPU-heavy polling loops.
* **Smart Lock & Download Detection:** Actively catches files still being written to by browsers or installers (`ERROR_SHARING_VIOLATION`, `ERROR_LOCK_VIOLATION`, `ERROR_ACCESS_DENIED`) and defers them to a bounded retry queue.
* **Automatic Collision Resolution:** Appends incremental counters (e.g., `report (1).pdf`, `report (2).pdf`) to prevent accidental overwrites.
* **Minimal Memory Footprint:** Trims its working set (`SetProcessWorkingSetSize`) during idle wait states to stay out of the way.
* **Graceful Termination:** Thread-safe cancellation support powered by Win32 Event handles.

***

## How It Works

```
Downloads Folder ──> Win32 Change Notification ──> Extension Normalized
                                                          │
   ┌──────────────────────────────────────────────────────┴─────────┐
   ▼                                                                ▼
Known Extension?                                             Unknown Extension?
   │                                                                │
   ▼                                                                ▼
Resolve Known Folder Destination                                  Ignored
   │
   ├── [Destination Exists?] ──> Append " (N)" unique suffix
   │
   └── [File Locked / Downloading?] ──> Enqueue to Retry Queue (1s tick)
   │
   └── [Available] ───────────────────> Atomically move file
```

***

## Prerequisites

* **Operating System:** Windows 10 (Build 19041+) or Windows 11
* **Compiler:** C++23 compliant compiler supporting `<print>`:
  * MSVC v143 (Visual Studio 2022 17.7+) or newer
  * Clang 17+ / GCC 13+ with MinGW-w64 targeting Windows
* **Build System:** CMake 3.25+

***

## Building

### Using Visual Studio (MSVC)

```cmd
git clone [https://github.com/your-username/bean.git](https://github.com/your-username/bean.git)
cd bean
cmake -B build -S . -DCMAKE_CXX_STANDARD=23
cmake --build build --config Release
```

The resulting binary will be output to `build/Release/bean.exe`.

### Minimal `CMakeLists.txt` Example

```cmake
cmake_minimum_required(VERSION 3.25)
project(bean LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(bean
    src/main.cpp
    src/organizer.cpp
    src/organizer.h
)

target_link_libraries(bean PRIVATE Shell32 Ole32)
```

***

## Usage

Run `bean` directly from the terminal or register it as a user startup task:

```powershell
.\bean.exe
```

### Log Output

```text
[X] C:\Users\Username\Downloads\screenshot.png -> C:\Users\Username\Pictures\screenshot.png.
File locked: C:\Users\Username\Downloads\installer.exe.crdownload. Added to retry queue.
[X] (Retry) C:\Users\Username\Downloads\installer.exe -> C:\Users\Username\Documents\installer.exe.
```

***

## Configuration

Default extension routing maps file types to standard Windows Known Folder IDs (`KNOWNFOLDERID`):

| Extension Type | Example Extensions | Target Known Folder |
| :--- | :--- | :--- |
| **Documents** | `.pdf`, `.docx`, `.xlsx`, `.txt` | `FOLDERID_Documents` |
| **Pictures** | `.png`, `.jpg`, `.jpeg`, `.gif`, `.webp` | `FOLDERID_Pictures` |
| **Audio** | `.mp3`, `.wav`, `.flac`, `.aac` | `FOLDERID_Music` |
| **Video** | `.mp4`, `.mkv`, `.mov`, `.avi` | `FOLDERID_Videos` |
| **Archives** | `.zip`, `.tar.gz`, `.7z`, `.rar` | Custom subfolder / Documents |

***

## License

This project is licensed under the [MIT License](LICENSE).
