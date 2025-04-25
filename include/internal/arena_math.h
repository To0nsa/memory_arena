/**
 * @file arena_math.h
 * @author Toonsa
 * @date 2025
 *
 * @brief
 * Math utility functions for memory alignment and overflow-safe arithmetic.
 *
 * @details
 * This header defines a small collection of inline utility functions
 * used internally in the arena allocator for:
 * - Ensuring proper alignment (`align_up`)
 * - Checking for safe multiplication (`would_overflow_mul`)
 * - Computing the next power of two (`next_power_of_two`)
 *
 * These helpers are optimized for performance and portability across platforms.
 *
 * @note
 * This file contains only inline functions and does not require linking.
 */

#ifndef ARENA_MATH_H
#define ARENA_MATH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief
	 * Align a value upwards to the nearest multiple of `alignment`.
	 *
	 * @details
	 * Returns the smallest multiple of `alignment` that is greater than
	 * or equal to `value`. `alignment` must be a power of two.
	 *
	 * @param value      The value to align.
	 * @param alignment  The alignment boundary (must be power of two).
	 *
	 * @return The aligned value.
	 *
	 * @ingroup arena_internal
	 *
	 * @note
	 * This function is commonly used for computing aligned memory offsets.
	 */
	static inline size_t align_up(size_t value, size_t alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}

	/**
	 * @brief
	 * Perform a checked multiplication of two `size_t` values.
	 *
	 * @details
	 * Computes `a * b` and stores the result in `*result`. If the multiplication
	 * would overflow `SIZE_MAX`, the function returns `true` and does not modify `*result`.
	 *
	 * If the platform supports `__builtin_mul_overflow`, it will be used for fast
	 * hardware-checked overflow detection. Otherwise, a portable fallback is used.
	 *
	 * @param a       First operand.
	 * @param b       Second operand.
	 * @param result  Output pointer to store the product if safe.
	 *
	 * @return `true` if the multiplication would overflow, `false` otherwise.
	 *
	 * @ingroup arena_internal
	 *
	 * @note
	 * This function ensures safe allocation size computations in arena logic.
	 */
	static inline bool would_overflow_mul(size_t a, size_t b, size_t* result)
	{
#if defined(__has_builtin)
#if __has_builtin(__builtin_mul_overflow)
		return __builtin_mul_overflow(a, b, result);
#endif
#endif

		if (a == 0 || b == 0)
		{
			*result = 0;
			return false;
		}
		if (a > SIZE_MAX / b)
		{
			return true;
		}
		*result = a * b;
		return false;
	}

	/**
	 * @brief
	 * Return the next power of two greater than or equal to `x`.
	 *
	 * @details
	 * If `x` is already a power of two, it is returned unchanged.
	 * If `x` is zero, the function returns 1.
	 *
	 * This is implemented using bitwise operations for speed and portability.
	 *
	 * @param x The input value.
	 *
	 * @return The next power of two ≥ `x`.
	 *
	 * @ingroup arena_internal
	 */
	static inline size_t next_power_of_two(size_t x)
	{
		if (x == 0)
			return 1;
		--x;
		x |= x >> 1;
		x |= x >> 2;
		x |= x >> 4;
		x |= x >> 8;
		x |= x >> 16;
#if SIZE_MAX > UINT32_MAX
		x |= x >> 32;
#endif
		return x + 1;
	}

#ifdef __cplusplus
}
#endif

#endif // ARENA_MATH_H
