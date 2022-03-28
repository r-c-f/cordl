/* sassert -- static assertion on as many platforms as possible
 *
 * Version 1.1
 *
 * Copyright 2022 Ryan Farley <ryan.farley@gmx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#pragma once
#include <assert.h>


/* because static_assert is guaranteed to be a macro in C11, we can avoid
 * custom names by only defining it on lesser platforms where needed*/
#if !defined(static_assert)


#if defined (__TINYC__)
	#warning "No native static assertion support; see errors about divide-by-zero for failures"
	#define static_assert(exp, str) \
		enum { static_assert_= 1/!!(exp), }

#elif defined(__GNUC__)
	/* see if we can get away with _Static_assert, which is present for
	 * all C standards in gcc >= 4.6 */
	#if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
		#define static_assert(exp, str) _Static_assert(exp, str)
	/* this is a filthy hack to trigger an error even if we can't actually
	 * have native support */
	#else
		#warning "No native static assertion support; see errors about array size for failures"
		#define static_assert(exp, str) \
			extern char (*static_assert(void)) [sizeof(char[1 - 2*!(exp)])]
	#endif
/* clang -- has a nice feature to test for it */
#elif defined(__clang__) && __has_feature(c_static_assert)
	#define static_assert(exp, str) _Static_assert(exp, str)

/* Microsoft -- anything NT *should* have this
 *
 * Note that it does not support messages.
*/
#elif defined(_MSC_VER)
	#include <winnt.h>
	#define static_assert(exp, str) C_ASSERT(exp)
#else
	/* treat this as an error condition for now */
	#if !defined(NDEBUG)
		#error "Static assertions not available"
	#endif
#endif

#endif /* !defined(static_assert) */

