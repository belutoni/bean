# bean

bean is a small Windows program that keeps your Downloads folder from turning into a junk drawer. It sits in the background, and whenever a file lands in Downloads it gets sent to Pictures, Documents or Videos, depending on what it is. What goes where is up to you: drop an `extensions.cfg` next to `bean.exe` and list the extensions you want sent to each folder. No rebuild needed. See [Make your own rules](#make-your-own-rules).

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

When bean starts, it loads its extension mappings, begins watching Downloads, and then sorts whatever is already there, so files that arrive during that first pass aren't missed. After that it goes to sleep. Windows wakes it up when a file in Downloads is created, renamed or deleted. It waits half a second for the change to settle, sorts again, and goes back to sleep. It doesn't poll, so while nothing is happening it uses basically no CPU.

Without a configuration file, this is where things end up:

| Folder    | Extensions                                          |
|-----------|-----------------------------------------------------|
| Pictures  | `.png` `.jpg` `.jpeg` `.gif` `.bmp` `.webp`         |
| Documents | `.pdf` `.doc` `.docx` `.xls` `.xlsx` `.pptx` `.txt` |
| Videos    | `.mp4` `.mov` `.mkv` `.avi`                         |

Don't like these? You can change all of it with a plain text file. It takes a minute, see [Make your own rules](#make-your-own-rules).

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
[INFO] Loaded 17 extension mappings from configuration file 'C:\Tools\bean\extensions.cfg'.
[INFO] Moved file(C:\Users\you\Downloads\holiday.jpg) to destination(C:\Users\you\Pictures\holiday.jpg).
[INFO] Moved file(C:\Users\you\Downloads\invoice.pdf) to destination(C:\Users\you\Documents\invoice.pdf).
[INFO] Moved file(C:\Users\you\Downloads\lecture.mp4) to destination(C:\Users\you\Videos\lecture.mp4).
```

A while later, you download another `invoice.pdf`. The first one is already in Documents, so the new one gets a number:

```
[INFO] Moved file(C:\Users\you\Downloads\invoice.pdf) to destination(C:\Users\you\Documents\invoice (1).pdf).
```

If a file is locked, you get a warning (these go to stderr) on every attempt, and a final one when bean gives up:

```
[WARN] File(C:\Users\you\Downloads\movie.mkv) is locked or busy. Will retry later.
[WARN] File(C:\Users\you\Downloads\movie.mkv) is locked or busy. Will retry later.
...
[WARN] File(C:\Users\you\Downloads\movie.mkv) could not be moved after 5 retries. Removing from retry queue.
```

In a debug build you also see the retry countdown, and every line shows the function that logged it:

```
[WARN]  [org::Organizer::move_file] File(C:\Users\you\Downloads\movie.mkv) is locked or busy. Will retry later.
[WARN]  [org::Organizer::move_file] File(C:\Users\you\Downloads\movie.mkv) is locked or busy. Will retry later.
[DEBUG] [org::Organizer::process_move_queue] File(C:\Users\you\Downloads\movie.mkv) is still locked. 4 retries left.
...
[WARN]  [org::Organizer::process_move_queue] File(C:\Users\you\Downloads\movie.mkv) could not be moved after 5 retries. Removing from retry queue.
```

If there is no configuration file, bean says so and carries on with the defaults:

```
[WARN] Failed to open configuration file(C:\Tools\bean\extensions.cfg). Using default extension mappings.
```

Info messages go to stdout, warnings and errors go to stderr. Debug messages are off by default. Debug builds also print debug messages and add the name of the function that logged each line (see [Building](#building)).

## Building

You need Windows, CMake, and MSVC with C++23 support. The code uses `<print>`, so that means Visual Studio 2022 17.7 or newer.

```
cmake -B build
cmake --build build --config Release
```

The executable will be somewhere under `build`, depending on your generator.

To get a debug build, define `ENABLE_DEBUG_MODE` when you compile. It turns on the debug messages and adds the function name to every log line, which is handy when tracking down a problem. In CMake that looks like this:

```cmake
target_compile_definitions(bean PRIVATE ENABLE_DEBUG_MODE)
```

## Running

Start `bean.exe`. It opens a console window and keeps running until you close it (or press Ctrl+C).

If you want it running all the time:

1. Press `Win + R`, type `shell:startup` and hit Enter.
2. Put a shortcut to `bean.exe` in the folder that opens.
3. Right-click the shortcut, open Properties, and set **Run** to **Minimized** so the console stays out of the way.

## Make your own rules

bean sorts by the table at the top unless you tell it otherwise. To tell it otherwise, create a plain text file called **`extensions.cfg`** and put it **in the same folder as `bean.exe`**. It doesn't matter where you launch bean from, so a Startup shortcut finds it just fine.

### Quick start

1. Create a new text file next to `bean.exe` and name it `extensions.cfg` (make sure it isn't `extensions.cfg.txt`).
2. Paste in the starter below and edit it.
3. Restart bean. The file is read once at startup.

```
# bean configuration
# Each line: <folder> <extension> <extension> ...
# Anything after a # is a comment.

pictures   .png .jpg .jpeg .gif .bmp .webp
documents  .pdf .doc .docx .xls .xlsx .pptx .txt
videos     .mp4 .mov .mkv .avi

# Add your own:
music      .mp3 .flac .wav
```

This starter is exactly bean's built-in default plus one line for music, so it's a safe base to build on.

### Syntax

Every line has the same shape: a **folder name**, then one or more **extensions**, separated by spaces or tabs.

```
folder  .ext  .ext  .ext   # optional comment
```

- Folder names and extensions are **not case-sensitive**. `Pictures .PNG` works the same as `pictures .png`.
- The leading dot on an extension is **optional**. `png` is the same as `.png`.
- Lines starting with `#` are comments, and so is anything after a `#` at the end of a line. Blank lines are ignored.
- A folder can appear on **more than one line**, so you can split a long list however you like.
- If an extension is listed twice (even under different folders), **the last one wins**.
- An extension can only have **one dot**. `.tar.gz` is skipped, because Windows only considers the last part of a file name. Use `.gz` instead.

### Valid folder names

These are the only folder names bean understands. They map to the matching Windows folders, so if you've moved one of them to another drive or into OneDrive, bean follows it.

| Name        | Where files go                                                        |
|-------------|-----------------------------------------------------------------------|
| `pictures`  | Your Pictures folder                                                  |
| `documents` | Your Documents folder                                                 |
| `music`     | Your Music folder                                                     |
| `playlists` | Your Playlists folder (normally inside Music)                         |
| `videos`    | Your Videos folder                                                    |
| `desktop`   | Your Desktop                                                          |
| `games`     | Legacy Games folder. Modern Windows often doesn't have it.            |
| `profile`   | The root of your user folder (`C:\Users\you`). Probably not what you want. |

Any other name is reported as a warning and that line is skipped. The rest of the file still loads.

### Things to know

- **Your file replaces the defaults, it doesn't add to them.** If your file only says `pictures .png`, then PDFs and videos are no longer sorted. Keep the lines you still want.
- **If the file is missing**, or none of its lines were valid, bean falls back to the built-in defaults and logs a warning.
- **Check the output.** bean prints how many mappings it loaded, and a warning for every line or extension it couldn't use (unknown folder, bad extension, a folder with no extensions).
- **Edits need a restart.** bean only reads the file when it starts.
- **Only ASCII extensions** are matched.

### Common mistakes

| You wrote                        | What happens                                                    |
|----------------------------------|-----------------------------------------------------------------|
| `picture .png`                   | Unknown folder name (it's `pictures`). Line skipped.            |
| `video .mp4`                     | Unknown folder name (it's `videos`). Line skipped.              |
| `pictures`                       | No extensions given. Line skipped.                              |
| `pictures .tar.gz`               | Extension has two dots. Skipped.                                |
| `pictures .png, .jpg`            | The comma becomes part of `.png,`, which never matches (`.jpg` still works). Separate with spaces only. |
| `extensions.cfg.txt`             | bean doesn't find the file and uses the defaults.               |

## Project layout

- `organizer.h` / `organizer.cpp`: the `org::Organizer` class that loads the configuration, watches Downloads and moves files. The built-in default mappings live in `load_default_extension_mappings` in `organizer.cpp`.
- `winpp.h`: small RAII wrappers for Win32 handles, so the stop event and the folder watcher are always closed.
- `logger.h`: the logging used for all output.

## Planned

- Custom folder paths as targets, so files can go somewhere other than the standard Windows folders.