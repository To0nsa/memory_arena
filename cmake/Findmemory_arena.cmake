# Try to find the memory_arena header (arena.h) in common include paths
find_path(MEMORY_ARENA_INCLUDE_DIR
    NAMES arena.h									# The public header file to search for
    PATHS											# Custom and fallback locations
        ${CMAKE_CURRENT_LIST_DIR}/../../../include	# Relative to this Find module's location (useful for vendoring)
        /usr/local/include							# Common install location on Unix systems
)

# Try to find the memory_arena library file (libmemory_arena.a or .so)
find_library(MEMORY_ARENA_LIBRARY
    NAMES memory_arena                             # The library filename (without lib prefix or extension)
    PATHS
        ${CMAKE_CURRENT_LIST_DIR}/../../../lib     # Relative search path
        /usr/local/lib                             # Fallback to common install location
)

# Use built-in helper to validate if both the include path and library were found
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(memory_arena     # Package name
    DEFAULT_MSG                                    # Let CMake generate a default error message if not found
    MEMORY_ARENA_LIBRARY MEMORY_ARENA_INCLUDE_DIR  # Required variables for success
)

# If the package was found, export useful variables
if(MEMORY_ARENA_FOUND)
    set(MEMORY_ARENA_LIBRARIES ${MEMORY_ARENA_LIBRARY})			# Exported library path
    set(MEMORY_ARENA_INCLUDE_DIRS ${MEMORY_ARENA_INCLUDE_DIR})	# Exported include path
endif()
