# Doxygen Style Guide for `memoryArena`

This guide defines how to write consistent, clean, and useful Doxygen documentation for all `.h` and `.c` files in the `memoryArena` project.

Following this guide helps:

- Keep documentation readable and structured
- Generate clean HTML/PDF output with Doxygen
- Improve code discoverability and collaboration

---

## 1. File Header Template (`@file`)

Each `.h` and `.c` file should start with a Doxygen `@file` block, **before** anything else.

```c
/**
 * @file ft_array.h
 * @author Toonsa
 * @date YYYY/MM/DD
 * @brief Brief description of this file's purpose.
 *
 * @details
 * Optional longer description of what the file does,
 * what it implements, and any special notes.
 * 
 * @note Optional notes about assumptions, guarantees,
 * or structure rules.
 *
 * @ingroup group_name
 */
```

---

## 2. Grouping Functions with `@defgroup`

Use groups to categorize related functions under a label (e.g., array_utils, math_helpers).

```c
/**
 * @defgroup array_utils Array Utilities
 * @brief Functions for working with pointer and integer arrays.
 *
 * @details
 * Useful for manipulating dynamic and fixed-size arrays.
 *
 * @note Assumes arrays are zero-terminated unless stated otherwise.
 *
 * @{
 */

/** @} */ // end of array_utils
```

### Guidelines

- One group per .h file
- Use `@{ ... @}` to wrap declarations
- Use `@ingroup` in .c to associate implementations
- Avoid redefining the same group in multiple places
- Use snake_case for group identifiers
- Place this in the header file that introduces those functions. Then, inside .c files, reference it with `@ingroup` array_utils.

---

## 3. Function Documentation Template

### In Headers (.h)

#### Public Function Template (Headers .h only)

```c
/**
 * @brief One-liner summary of what this function does.
 *
 * @note
 * Any caveats, NULL-safety guarantees, thread-safety,
 * or usage contexts worth highlighting.
 *
 * @param [name] Description of each parameter.
 * @return Description of the return value.
 *
 * @see Related functions (optional)
 */
```

#### Inline Function Template (static inline, usually in .h)

```c
/**
 * @brief One-liner summary of what this inline function does.
 *
 * @details
 * - Why this is inline
 * - Expected usage and limitations
 * - Side effects (if any)
 *
 * @note Marked `static inline` to avoid linkage issues.
 *
 * @param [name] Description of each parameter.
 * @return Description of the return value.
 *
 * @see Related inline or public functions
 * @ingroup inline_group_name
 */
```

#### Macro Documentation Template (#define, in headers)

```c
/**
 * @brief One-liner summary of what this macro does.
 *
 * @details
 * - Expansion behavior
 * - Use cases and operator precedence issues
 * - Macro pitfalls like double-evaluation
 *
 * @note Wrap arguments in parentheses for safety.
 *
 * @param [param] Description of macro parameters.
 * @return Result or behavior of the macro.
 *
 * @see Related macros or inline functions
 * @ingroup macro_group_name
 */
#define MACRO_NAME(x, y) ((x) > (y) ? (x) : (y))
```

### In Source Files (.c)

#### Internal Function Template (.c files, static/private functions)

```c
/**
 * @brief Summary of what this internal function does.
 *
 * @details
 * - Internal logic and rationale
 * - Implementation quirks or edge cases
 *
 * @note Internal use only. Do not call outside this module.
 *
 * @param [name] Description of each parameter.
 * @return Description of return value.
 *
 * @see Related helpers
 * @ingroup internal_group_name
 */
```

#### Public Function Template (.c files, exposed via headers)

``` c
/**
 * @brief Summary of what this public function does.
 *
 * @details
 * - Purpose, expected inputs, and outputs
 * - Algorithm insights and performance notes
 * - Side effects and edge cases
 *
 * @note Portability or usage caveats.
 *
 * @param [name] Description of each parameter.
 * @return Description of return value.
 *
 * @see Related functions
 *
 * @example
 * @code
 * arena_t* a = arena_create(1024);
 * void* ptr = arena_alloc(a, 32);
 * assert(ptr != NULL);
 * @endcode
 *
 * @ingroup public_group_name
 */
```

---

## 4. Struct and Typedef Documentation

```c
/**
 * @typedef t_point
 * @brief A 2D point with integer coordinates.
 *
 * @details
 * - `x`: horizontal position
 * - `y`: vertical position
 */
typedef struct s_point {
    int x; ///< X coordinate
    int y; ///< Y coordinate
} t_point;
```

> Use ///< for inline field comments. Group with @ingroup as needed.

---

## 5. Internal/Static Functions

Use @internal to hide from public Doxygen output:

```c
/**
 * @internal
 * @brief Internal helper to clamp a value.
 */
static int clamp_internal(int value, int min, int max);
```

> Skip trivial helpers. Use this only for non-trivial internals worth documenting.

---

## 6. Doxygen Tag Reference Table

| Tag           | Description                                | Use In                        |
|---------------|--------------------------------------------|-------------------------------|
| `@file`       | Describes the file                         | Top of `.c` / `.h`            |
| `@brief`      | Short summary                              | All blocks                    |
| `@details`    | Longer explanation                         | Optional                      |
| `@param`      | Function parameter description             | Functions                     |
| `@return`     | Return value description                   | Functions                     |
| `@retval`     | Specific return values (e.g., error codes) | Functions                     |
| `@note`       | Additional usage info or caveats           | Optional                      |
| `@see`        | Cross-references to related symbols        | Optional                      |
| `@ingroup`    | Associates function with a group           | Implementation (`.c`) files   |
| `@defgroup`   | Defines a documentation group              | Header (`.h`) files           |
| `@internal`   | Hides internal code from public docs       | `.c` only                     |
| `@example`    | Usage example snippet                      | Public `.c` only              |
| `@code`       | Starts a code block                        | Inside `@details`, `@example` |
| `@endcode`    | Ends a code block                          | Matches `@code`               |
| `@todo`       | Flags an incomplete feature                | Optional                      |
| `@warning`    | Warns about unsafe or risky usage          | Optional                      |
| `@since`      | Indicates when a feature was added         | Optional                      |
| `@deprecated` | Marks an API as obsolete                   | Optional                      |

---

## 7. General Writing Style

- Use full sentences with punctuation.
- Prefer present tense: "Allocates" not "Allocated".
- Avoid repeating parameter names in the `@brief`.
- Keep headers short, and put explanations in `@details`.
- Align `@param` order with function signature.
- Add spacing between `@brief`, `@details`, and `@note` sections.

---

## 8. Doxyfile Tips for GitHub Pages

### Setup

```bash
doxygen -g Doxyfile
```

### Edit Doxyfile

```ini
#----------------------------------
# Project Info
#----------------------------------
PROJECT_NAME           = "memoryArena"
PROJECT_BRIEF          = "..."
PROJECT_LOGO           =
OUTPUT_DIRECTORY       = docs
INPUT                  = inc/ srcs/
RECURSIVE              = YES
FILE_PATTERNS          = *.h *.c
EXCLUDE_PATTERNS       = */.git/*

#----------------------------------
# Build Configuration
#----------------------------------
EXTRACT_ALL            = YES
EXTRACT_PRIVATE        = YES
EXTRACT_STATIC         = YES
EXTRACT_LOCAL_CLASSES  = YES
OPTIMIZE_OUTPUT_FOR_C  = YES
EXTENSION_MAPPING      = c=C

#----------------------------------
# Output Options
#----------------------------------
GENERATE_HTML          = YES
HTML_OUTPUT            = html
HTML_DYNAMIC_SECTIONS  = YES

#----------------------------------
# Warnings and Cleanliness
#----------------------------------
WARN_IF_UNDOCUMENTED   = YES
WARN_IF_DOC_ERROR      = YES
WARN_NO_PARAMDOC       = YES
QUIET                  = NO
WARNINGS               = YES

#----------------------------------
# GitHub Pages Friendly Output
#----------------------------------
GENERATE_LATEX         = NO
GENERATE_MAN           = NO
GENERATE_RTF           = NO
GENERATE_XML           = NO
USE_MDFILE_AS_MAINPAGE = README.md
MARKDOWN_SUPPORT       = YES
```

### Then run

```bash
doxygen Doxyfile
```

Push docs/ to GitHub Pages

## 9. Linting and Quality Checks

- Use `doxygen-lint` to catch mistakes
- Enable `WARN_IF_UNDOCUMENTED = YES` in Doxyfile
- Consider running spell check or grammar check in CI/CD
