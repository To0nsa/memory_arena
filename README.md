# Memory_Arena

![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)
![Language: C](https://img.shields.io/badge/language-C-blue.svg)
![CMake](https://img.shields.io/badge/build%20system-CMake-informational)
![Platform: Linux](https://img.shields.io/badge/platform-Linux-informational)
![Pkg-config](https://img.shields.io/badge/pkg--config-supported-brightgreen)
![Library: Static + Shared](https://img.shields.io/badge/library-static%20%2B%20shared-blue)
![Thread-safe](https://img.shields.io/badge/thread--safe-optional-yellow)
![Tested with CTest](https://img.shields.io/badge/tested%20with-CTest-green)

> `Memory_Arena` is a lightweight, high-performance memory allocator written in C, fully compatible with C++.

[🔗 View on GitHub](https://github.com/to0nsa/Memory_Arena)
**[📚 View full documentation](https://to0nsa.github.io/memory_arena/)**

___

## 🎯 Introduction

`Memory_Arena` is a lightweight, high-performance memory allocator written in C, fully compatible with C++. Designed around the arena allocation model, it organizes memory allocations into contiguous blocks for rapid allocation and bulk deallocation, significantly improving speed, memory efficiency, and reducing fragmentation. Library that is especially useful for systems programming, game engines, real-time applications, and scenarios where performance and memory efficiency are critical.

Instead of individually managing numerous memory operations (like `malloc` and `free`), it allows you to reset or reuse entire memory regions at once, perfect for scenarios involving temporary data, real-time rendering, or modular systems.

`Memory_Arena` is a lightweight, high-performance memory allocator written in C, fully compatible with C++. It leverages the arena allocation model to organize memory allocations into contiguous blocks, providing rapid allocation and bulk deallocation—ideal for systems programming, game engines, real-time applications, and scenarios where performance and memory efficiency are critical.

___

## 📚 About the Project

I created this project after completing the C programming curriculum at Hive Helsinki (part of the 42 Network). My main goal was to deepen my understanding of **low-level memory management**, **system programming** and improve my skills by tackling practical challenges related to memory, performance, and **library architecture**.

Throughout the development of `Memory_Arena`, I've:

- Explored and implemented advanced memory management techniques such as arena allocators, scratch arenas, and stack-like memory handling.
- Integrated thorough testing methods, leveraging AddressSanitizer (ASAN) and ThreadSanitizer (TSAN) to ensure robust memory and thread safety.
- Handled multi-threaded scenarios carefully, designing thread-safe mechanisms to enable concurrent memory operations without significant performance hits.
- Focused on usability, providing clear documentation, straightforward API design, and robust build configurations with CMake and pkg-config support.
- Gained experience in debugging and optimizing for performance, balancing speed, and reliability.

This project is part of my learning journey with C, demonstrating my ability to handle complex technical tasks, organize large-scale codebases effectively, and adhere to modern software development practices.

Ultimately, my goal is to deliver a library that is both educational for peers exploring systems programming, and genuinely useful as an efficient tool for developers tackling memory-intensive tasks.

___

## ✨ `Memory_Arena` Features

<details>
<summary><b> See `Memory_Arena` Features </b></summary>

⚡ **Fast Bump Allocation**
Allocations are performed linearly via pointer bumping, which offers near-zero overhead and avoids costly malloc bookkeeping. Ideal for temporary or scoped allocations.

🔧 **Flexible Allocation**
Allocate raw or zeroed memory, reallocate in place, or tag with debug labels. Supports alignment, hooks, stats, and thread safety. Designed for speed, clarity, and control—without manual frees or fragmentation.

🔄 **Dynamic Growth & Shrinkage**
Arenas can automatically grow and optionally shrink when memory usage changes. This provides flexibility while maintaining predictable performance characteristics.

🔁 **Sub-Arenas & Markers**
Supports nested memory scopes via sub-arenas and arena_mark/arena_pop. Useful for implementing undo/rollback systems or scoped temporary memory.

🧵 **Thread-Safety (Opt-In)**
Enable safe multithreaded access to arena structures using `ARENA_ENABLE_THREAD_SAFE`. Internally guarded by recursive mutexes for safe reentry.

🌀 **Temporary Scratch Arenas**
Memory_Arena provides two systems for fast, reusable temporary memory:
🔢 **Scratch Arena Pool (arena_scratch)**
Fast, reusable memory slots ideal for temporary workloads. Acquire/reset arenas on demand with thread-safe access, atomic tracking, and minimal overhead. Perfect for per-frame or per-task use.
🧵 **Thread-Local Scratch Arenas (arena_tlscratch)**
Each thread gets its own fast, auto-resetting arena. Zero locks, zero setup after init, and perfect for throwaway allocations in tight loops or parallel workloads.

🗂️ **Scoped Stack Frames**
Use `arena_mark()` and `arena_pop()` to create scoped memory lifetimes within an arena. Perfect for recursive algorithms, temporary parse buffers, or structured rollback. Fast, deterministic, and zero heap allocations—stack frames live inside the arena itself.

💾 **Arena Snapshots**
Save and load arena memory to .bin files. Includes magic header/versioning, offset tracking, and buffer content. Great for debugging, state persistence, or fast startup by restoring memory from disk. Only works with arenas that own their buffer.

🖼️ **Interactive Memory Visualizer**
A curses-based terminal visualizer is provided (using Notcurses) to observe arena activity live, allocations, resets, growth, and more. Helpful for profiling, education, or debugging.

🧩 **Allocation Labels & Hooks**
Attach labels to allocations for better diagnostics, or use custom hooks to monitor memory usage and capture metadata in real time.

📊 **Debug Stats & Growth Tracking**
Internal statistics provide detailed insight into memory usage, peak allocations, growth events, and frame stack depth.

🛠️ **Debug-Ready**
Built-in support for AddressSanitizer, ThreadSanitizer, and debug stats through CMake presets.

☣️ **Memory Poisoning for Debugging**
Enable `ARENA_POISON_MEMORY` to fill freed memory with a poison pattern, helping you catch use-after-free bugs during development.

🧪 **Fully Tested**
Includes over 30 unit and multithreaded tests. Supports ASAN, TSAN, and custom debug checks with arena_report_error() on critical paths.

🪶 **Minimal Dependencies**
Written in pure C11 with no external dependencies outside optional visualizer.

📚 **Well Documented**
Each function and internal module is documented using Doxygen with usage examples, groupings, and full cross-references. **[View full documentation](https://to0nsa.github.io/memory_arena/)**

</details>

___

## 🚀Usage guide

<details>
<summary><b> See Usage Guide </b></summary>

### 🧱 Static and Shared Library Builds

This project builds the memory arena as both:

- **`Memory_Arena_static`** — a static library (`.a`), linked directly into your program.
- **`Memory_Arena_shared`** — a shared library (`.so`), loaded at runtime by the system (linked with `pthread`).

___

### 🔰 Prerequisites

<details>
<summary><b> See Prerequisites </b></summary>

Make sure the following tools are installed on your system:

- **CMake ≥ 3.16**
- **Ninja** (for fast builds via `--preset`)
- **GCC / Clang** (any modern C compiler)
- **pkg-config** (for non-CMake users)
- **Notcurses** (if you plan to build and run the `visualizer`)

</details>

___

### 🧪 Debugging & Sanitizer Options

<details>
<summary><b> See Debugging & Sanitizer Options </b></summary>

To help catch memory leaks, bugs, race conditions, deadlocks early, this project includes optional sanitizer support and debug features built into the CMake configuration.

You can enable the following features by setting CMake options (using presets or `-D` flags):

- **`USE_ADDRESS_SANITIZER`** — Detects memory errors like use-after-free, buffer overflows, etc.
- **`USE_THREAD_SANITIZER`** — Detects data races in multi-threaded programs.
- **`ARENA_DEBUG_CHECKS`** — Adds runtime checks to verify internal consistency.
- **`ARENA_POISON_MEMORY`** — Overwrites memory with a known pattern when freed (e.g. `0xDEADBEEF`) to detect use-after-free.
- **`ARENA_ENABLE_THREAD_SAFE`** — Enables mutex locking inside the allocator for safe multi-threaded usage.

> ⚠️ You **cannot** enable both ASAN and TSAN at the same time — the build will fail with a clear error if you try.

These options are defined in:

- `SanitizerConfig.cmake` — applied conditionally based on flags

- The top-level `CMakeLists.txt` — integrates them into the build and install process

By using these, you can easily switch between production-ready builds and debug-friendly builds to help test your project more safely.

</details>

___

### 🔧 Build Presets

<details>
<summary><b> See Build Presets </b></summary>

The project uses [CMake presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) to manage different build configurations.

Use one of the following commands to configure a build:

```bash
cmake --preset debug      # Debug + extra checks
cmake --preset asan       # AddressSanitizer (memory issues)
cmake --preset tsan       # ThreadSanitizer (thread safety issues)
cmake --preset release    # Optimized build for distribution
```

Each preset enables or disables certain safety and debug features:

| Preset   | Type    | Thread-Safe | Poison Memory | Debug Checks | AddressSanitizer | ThreadSanitizer | Tests Enabled |
|----------|---------|-------------|----------------|---------------|------------------|------------------|----------------|
| `debug`  | Debug   | ✅           | ✅              | ✅             | ❌                | ❌                | ✅              |
| `asan`   | Debug   | ✅           | ✅              | ✅             | ✅                | ❌                | ✅              |
| `tsan`   | Debug   | ✅           | ❌              | ✅             | ❌                | ✅                | ✅              |
| `release`| Release | ✅           | ❌              | ❌             | ❌                | ❌                | ❌              |

> 🔹 **Thread-Safe**: Enables mutex locking internally.  
> 🔹 **Poison Memory**: Fills freed memory with patterns (e.g. `0xDEADBEEF`) to catch bugs.  
> 🔹 **Debug Checks**: Adds runtime checks for allocation errors, bounds, and double frees.  
> 🔹 **ASAN/TSAN**: Compiler-level tools to detect bugs at runtime.

</details>

___

### 🛠️ Build Targets

<details>
<summary><b> See Build Targets </b></summary>

Once you've configured a preset (e.g. cmake --preset debug), you can build using these commands:

#### 🔧 Common Build Commands

| Command                          | Description                                          |
|----------------------------------|------------------------------------------------------|
| `cmake --build --preset release` | Build an optimized release version                   |
| `cmake --build --preset asan`    | Build with AddressSanitizer (memory error detection) |
| `cmake --build --preset tsan`    | Build with ThreadSanitizer (race condition checker)  |
| `cmake --build --preset debug`   | Build to check debugs                                |

#### 🧪 Example Workflow

```bash
# Configure with Debug preset
cmake --preset debug

# Build the library, tests, and tools
cmake --build --preset debug

# Run unit Memory_Arena tests
ctest --preset debug
```

You can replace `debug` with `asan`, `tsan`, or `release` for different builds.

#### 🐛 Debug Builds

While these builds were originally designed to test the allocator itself, you can also use them while integrating the arena into your own program.

For example:

- ASAN can detect out-of-bounds or use-after-free bugs in *your* code.
- TSAN will catch race conditions across threads using the arena or other data.

</details>

___

### 📚 Using The Libraries

<details>
<summary><b> See Using The Libraries </b></summary>

#### 🔗 Using the Static Library

The static version (`libMemory_Arena.a`) is linked directly into your binary. It requires no special setup at runtime — ideal for portable or standalone builds.

```cmake
target_link_libraries(my_app PRIVATE Memory_Arena::static)
```

This will include the memory arena directly into your binary, making it easy to distribute without worrying about runtime dependencies.

___

#### 📦 Using the Shared Library

The shared version (`libMemory_Arena.so`) is loaded by the system at runtime. You **must ensure the dynamic linker can locate it**.

##### ✅ Option 1: Install Globally

```bash
sudo cmake --install build/release
```

This installs the `.so` to `/usr/local/lib`, which is typically already searched by the system loader.

- ✅ **Pros**: Works out of the box once installed, system-wide availability.
- ⚠️ **Cons**: Requires `sudo`, may conflict with system packages if multiple versions are installed.

##### ✅ Option 2: Set `LD_LIBRARY_PATH`

```bash
export LD_LIBRARY_PATH=/path/to/build/release:$LD_LIBRARY_PATH
./my_app
```

Use this if you want to test without global installation. The environment variable tells the loader where to find your `.so`.

- ✅ **Pros**: Great for local testing, sandboxed environments, CI setups.
- ⚠️ **Cons**: Needs to be set **each time**, or exported in `.bashrc`/`.zshrc`.

> 💡 Tip: You can also wrap it in a one-liner:
> `LD_LIBRARY_PATH=build/release ./my_app`

##### ✅ Option 3: Embed an rpath in Your Binary

```cmake
set_target_properties(my_app PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
)
```

This embeds a path inside your binary, instructing it to look for the shared library relative to its install location.

- ✅ **Pros**: Self-contained, portable, no need for global installation or env variables.
- 💡 `$ORIGIN` refers to the location of the executable at runtime — useful when installing to a subfolder layout like:

```bash
my_app/
├── bin/
│   └── my_app
└── lib/
    └── libMemory_Arena.so
```

</details>

___

### 📂 Consuming the Library

<details>
<summary><b> See Consuming the Library </b></summary>

This project installs both CMake and pkg-config integration to simplify usage.

___

#### 🔧 From CMake

If you've installed the library globally (via `sudo cmake --install`), you can use `find_package`:

```cmake
find_package(Memory_Arena REQUIRED)

# Link to either the static or shared version
target_link_libraries(my_app PRIVATE Memory_Arena::static) # or Memory_Arena::shared
```

You get access to:

- Include paths via `target_include_directories`
- Compile definitions like `ARENA_ENABLE_THREAD_SAFE`

💡 CMake will locate `Memory_ArenaTargets.cmake` and automatically configure everything.

___

#### 📦 From pkg-config

If you're using a Makefile or a non-CMake build system:

```sh
pkg-config --cflags Memory_Arena   # Get compiler flags
pkg-config --libs Memory_Arena     # Get linker flags
```

Make sure `PKG_CONFIG_PATH` includes the install location (`/usr/local/lib/pkgconfig`).

```sh
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

This allows any build system to integrate with `Memory_Arena` using the standard pkg-config interface.

</details>

___

### 🧹 Uninstall / Clean the Build

<details>
<summary><b> See Uninstall / Clean the Build </b></summary>

If you need to fully clean the Memory_Arena project — including all build files, installed artifacts, and pkg-config data — follow these steps:

#### 🗑️ 1. Clean the Build Directory

If you used CMake presets (debug, release, etc.), simply delete the build folders:

```bash
rm -rf build/
```

#### 🧽 2. Uninstall System-Wide Files

If you've installed the library with:

```bash
sudo cmake --install build/release
```

You can manually remove the installed files:

```bash
# Remove the static and shared libraries
sudo rm -f /usr/local/lib/libMemory_Arena.a
sudo rm -f /usr/local/lib/libMemory_Arena.so

# Remove the installed headers
sudo rm -rf /usr/local/include/arena_*.h
sudo rm -f /usr/local/include/arena.h

# Remove CMake configuration
sudo rm -rf /usr/local/lib/cmake/Memory_Arena

# Remove pkg-config file
sudo rm -f /usr/local/lib/pkgconfig/Memory_Arena.pc
```

Make sure to check `/usr/local/lib`, `/usr/local/include`, and `/usr/local/lib/pkgconfig` if you customized the install path.

#### 🧼 3. Reset pkg-config Environment

If you had added this to your session:

```bash
export PKG_CONFIG_PATH=/path/to/build/release:$PKG_CONFIG_PATH
```

It will reset on reboot, but you can manually unset it with:

```bash
unset PKG_CONFIG_PATH
```

#### 🧹 4. Remove Compilation Artifacts (Optional)

You can also remove compile commands and CMake cache:

```bash
rm -f compile_commands.json CMakeCache.txt
```

</details>

___

## 📁 Project Structure Overview

<details>
<summary><b> See Project Structure Overview </b></summary>

```bash
Memory_Arena/
├── benchmark/                     # Benchmarks for performance testing
├── cmake/                         # Custom CMake modules and templates
│   ├── Memory_Arena.pc.in         # Template for pkg-config .pc file
│   ├── Memory_ArenaConfig.cmake.in # Template for find_package() config
│   ├── FindMemory_Arena.cmake     # Manual fallback if find_package fails
│   └── SanitizerConfig.cmake      # Enables ASAN / TSAN / debug macros via options
├── include/                       # Public header files (installed)
│   ├── internal/                  # Internal headers (not installed, private use only)
│   │   ├── arena_internal.h       # Shared internal declarations and helpers
│   │   └── ...                    # Other private headers
│   ├── arena.h                    # Main API header
│   └── ...                        # All other public headers (group, stack, stats, etc.)
├── srcs/                          # Implementation source files
│   ├── core/                      # Core arena logic
│   │   ├── allocation/            # Low-level memory allocators (alloc, calloc, etc.)
│   │   │   ├── arena_alloc.c
│   │   │   └── ...
│   │   ├── arena_cleanup.c        # Arena destruction and cleanup logic
│   │   └── ...
│   └── internal/                  # Internal modules
│       ├── arena_internal.c
│       └── ...
├── tests/                         # Unit tests for every module
│   ├── test_arena_alloc.c         # Test: arena_alloc behavior
│   └── ...                        # Other tests: realloc, scratch, stack, etc.
├── .clang-format                  # Code formatting configuration
├── .gitignore                     # Files and folders to ignore in Git
├── CMakeLists.txt                 # Main CMake build configuration
├── CMakePresets.json              # Presets for common builds (debug, release, asan...)
├── DOXYGEN_STYLE_GUIDE.md         # Documentation style guide (Doxygen-based)
├── format_all.sh                  # Format all source files (via clang-format)
├── LICENCE                        # License of the project (MIT)
├── README.md                      # Main project overview and instructions
└── test_all.sh                    # Helper script to build & run all tests
```

</details>
___

## 📝 License

This project is licensed under the [MIT License](LICENSE).

You are free to use, modify, and distribute this code for academic, personal, or professional purposes. Attribution is appreciated but not required.

___

If you have any questions, suggestions, or feedback, feel free to reach out:

- **📧 Email:** nicolas.lovis@hotmail.fr
- **💻 GitHub:** [github.com/to0nsa](https://github.com/to0nsa)

You're also welcome to open an issue or leave a comment on the repository.
