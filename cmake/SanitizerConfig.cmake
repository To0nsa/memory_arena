
# Optional Build-Time Feature Flags
# These options can be toggled manually or by CMake presets.

# Enable AddressSanitizer (for detecting memory corruption bugs)
option(USE_ADDRESS_SANITIZER "Enable ASAN" OFF)

# Enable ThreadSanitizer (for detecting data races)
option(USE_THREAD_SANITIZER  "Enable TSAN" OFF)

# Enable runtime integrity checks inside the arena system
option(ARENA_DEBUG_CHECKS    "Enable internal arena checks" OFF)

# Enable memory poisoning with 0xDEADBEEF to detect use-after-free bugs
option(ARENA_POISON_MEMORY   "Enable poisoning" OFF)

# Enable thread-safety across all arenas using internal mutexes
option(ARENA_ENABLE_THREAD_SAFE "Enable thread-safe arena" OFF)


# Sanity Check: ASAN and TSAN are not compatible together
if (USE_ADDRESS_SANITIZER AND USE_THREAD_SANITIZER)
    message(FATAL_ERROR "Cannot enable both ASAN and TSAN simultaneously.")
endif()

# AddressSanitizer Configuration
if (USE_ADDRESS_SANITIZER)
    message(STATUS "🧪 ASAN enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -O1 -g)
    add_link_options(-fsanitize=address)
endif()

# ThreadSanitizer Configuration
if (USE_THREAD_SANITIZER)
    message(STATUS "🔒 TSAN enabled")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=thread)
endif()

# Thread Safety: enable internal locking
if (ARENA_ENABLE_THREAD_SAFE)
    add_compile_definitions(ARENA_ENABLE_THREAD_SAFE)
    message(STATUS "🔐 Thread-safe mode enabled")
endif()

# Memory Poisoning: fill deallocated memory for diagnostics
if (ARENA_POISON_MEMORY)
    add_compile_definitions(ARENA_POISON_MEMORY)
    message(STATUS "☣️  Memory poisoning enabled")
endif()

# Debug Checks: enable internal consistency verification
if (ARENA_DEBUG_CHECKS)
    add_compile_definitions(ARENA_DEBUG_CHECKS ARENA_DEBUG_LOG)
    message(STATUS "🛠️ Arena debug checks enabled")
endif()
