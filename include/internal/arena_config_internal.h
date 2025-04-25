#ifndef ARENA_CONFIG_INTERNAL_H
#define ARENA_CONFIG_INTERNAL_H

/// Max characters for arena ID strings
#ifndef ARENA_ID_LEN
#define ARENA_ID_LEN 8
#endif

/// Max nested frame depth for arena_mark/pop
#ifndef ARENA_MAX_STACK_DEPTH
#define ARENA_MAX_STACK_DEPTH 16
#endif

/// Shrink threshold: offset < ratio * size
#ifndef ARENA_MIN_SHRINK_RATIO
#define ARENA_MIN_SHRINK_RATIO 0.95
#endif

/// Extra padding when computing shrink sizes
#ifndef ARENA_SHRINK_PADDING
#define ARENA_SHRINK_PADDING 64
#endif

/// Upper bound for arena buffer size
#ifndef ARENA_MAX_ALLOWED_SIZE
#define ARENA_MAX_ALLOWED_SIZE (1ULL << 32)
#endif

/// Max alignment value supported
#ifndef ARENA_MAX_ALIGNMENT
#define ARENA_MAX_ALIGNMENT 4096
#endif

#endif // ARENA_CONFIG_INTERNAL_H
