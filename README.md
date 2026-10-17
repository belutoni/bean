# bean

bean is a small Windows program that keeps your Downloads folder from turning into a junk drawer. It sits in the background, and whenever a file lands in Downloads it gets sent to Pictures, Documents or Videos, depending on what it is.

Windows only, written in C++23.

```
Before                          After

Downloads/                      Downloads/
  holiday.jpg                     setup.exe
  invoice.pdf                     backup.zip
  setup.exe
  lecture.mp4                   Pictures/   holiday.jpg
  backup.zip                    Documents/  invoice.pdf
                                Videos/     lecture.mp4
```

Anything bean doesn't recognize (like `setup.exe` and `backup.zip` above) stays where it is.

## How it works

When bean starts, it begins watching Downloads and then sorts whatever is already there, so files that arrive during that first pass aren't missed. After that it goes to sleep. Windows wakes it up when a file in Downloads is created, renamed or deleted. It waits half a second for the change to settle, sorts again, and goes back to sleep. It doesn't poll, so while nothing is happening it uses basically no CPU.

Here is where things end up:

| Folder    | Extensions                                          |
|-----------|-----------------------------------------------------|
| Pictures  | `.png` `.jpg` `.jpeg` `.gif` `.bmp` `.webp`         |
| Documents | `.pdf` `.doc` `.docx` `.xls` `.xlsx` `.pptx` `.txt` |
| Videos    | `.mp4` `.mov` `.mkv` `.avi`                         |

## The details

- Extensions are matched ignoring case, so `.JPG` works too. Files with non-ASCII extensions are skipped.
- Only files directly inside Downloads are moved. Subfolders and what's in them are left alone.
- Unfinished browser downloads like `.crdownload` are ignored because of their extension. Once the browser renames the file to its real name, bean notices the change and picks it up.
- The destination folders come from Windows itself, so if you've moved Documents to another drive or into OneDrive, bean follows it there.
- bean never overwrites anything. If a file with the same name already exists at the destination, the new one becomes `name (1).ext`, then `name (2).ext`, and so on (up to 1000 tries). The name is claimed as part of the move itself, so there is no gap in which another program could create the same file and get overwritten.
- If the destination is on a different drive, Windows copies the file over and removes the original as part of the same move. Large files can take a while.
- If a file is in use by another program, bean puts it in a retry queue and tries again about once a second, up to 5 times. After that it logs a warning and moves on. It will try that file again the next time something changes in Downloads.
- Only "file is in use" errors are retried. Any other failure (for example a missing destination) is logged once and the file is dropped from the queue. "Access denied" can mean either a lock or a real permissions problem, so it is treated as a lock and retried.
- bean moves files without asking and has no undo. If something disappears from Downloads, check the output to see where it went.

## What it looks like

bean prints a line for every file it handles. This is roughly what you'll see:

```
[INFO] [org::Organizer::move_file] Moved file(C:\Users\you\Downloads\holiday.jpg) to destination(C:\Users\you\Pictures\holiday.jpg).
[INFO] [org::Organizer::move_file] Moved file(C:\Users\you\Downloads\invoice.pdf) to destination(C:\Users\you\Documents\invoice.pdf).
[INFO] [org::Organizer::move_file] Moved file(C:\Users\you\Downloads\lecture.mp4) to destination(C:\Users\you\Videos\lecture.mp4).
```

A while later, you download another `invoice.pdf`. The first one is already in Documents, so the new one gets a number:

```
[INFO] [org::Organizer::move_file] Moved file(C:\Users\you\Downloads\invoice.pdf) to destination(C:\Users\you\Documents\invoice (1).pdf).
```

If a file is locked, you get a warning (these go to stderr) on every attempt. With debug logging on you also see the retry countdown:

```
[WARN]  [org::Organizer::move_file] File(C:\Users\you\Downloads\movie.mkv) is locked or busy. Will retry later.
[WARN]  [org::Organizer::move_file] File(C:\Users\you\Downloads\movie.mkv) is locked or busy. Will retry later.
[DEBUG] [org::Organizer::process_move_queue] File(C:\Users\you\Downloads\movie.mkv) is still locked. 4 retries left.
...
[WARN]  [org::Organizer::process_move_queue] File(C:\Users\you\Downloads\movie.mkv) could not be moved after 5 retries. Removing from retry queue.
```

Info and debug messages go to stdout, warnings and errors go to stderr. Debug messages are off by default.

## Building

You need Windows, CMake, and MSVC with C++23 support. The code uses `<print>`, so that means Visual Studio 2022 17.7 or newer.

```
cmake -B build
cmake --build build --config Release
```

The executable will be somewhere under `build`, depending on your generator.

To get debug messages, define `ENABLE_DEBUG_MODE` when you compile. In CMake that looks like this:

```cmake
target_compile_definitions(bean PRIVATE ENABLE_DEBUG_MODE)
```

## Running

Start `bean.exe`. It opens a console window and keeps running until you close it (or press Ctrl+C).

If you want it running all the time:

1. Press `Win + R`, type `shell:startup` and hit Enter.
2. Put a shortcut to `bean.exe` in the folder that opens.
3. Right-click the shortcut, open Properties, and set **Run** to **Minimized** so the console stays out of the way.

## Changing what gets sorted

For now the extension list is hardcoded. It lives in `m_extension_to_folder_id` at the bottom of `organizer.h`. To add an extension, add a line:

```cpp
{ ".mp3", FOLDERID_Music },
```

Any Windows `KNOWNFOLDERID` works as a target. Rebuild afterwards.

## Project layout

- `organizer.h` / `organizer.cpp`: the `org::Organizer` class that watches Downloads and moves files.
- `winpp.h`: small RAII wrappers for Win32 handles, so the stop event and the folder watcher are always closed.
- `logger.h`: the logging used for all output.

## Planned

- A config file, so you can change extensions and target folders without touching the code or rebuilding.