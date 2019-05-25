/***************************************************************************
 * ROM Properties Page shell extension. (librpbase)                        *
 * TextFuncs.cpp: Text encoding functions. (specialized conversions)       *
 *                                                                         *
 * Copyright (c) 2009-2018 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "TextFuncs.hpp"
#include "TextFuncs_NULL.hpp"

// C++ includes.
#include <string>
using std::string;

// The specialized code page conversions in this file aren't supported by
// standard system libraries.

namespace LibRpBase {

/** Specialized code page functions. **/

/**
 * Internal custom 8-bit to UTF-8 conversion function.
 * @param tbl	[in] 256-element decoding table. (Only supports Unicode BMP.)
 * @param str	[in] 8-bit text.
 * @param len	[in] Length of str, in bytes. (-1 for NULL-terminated string)
 * @return UTF-8 string.
 */
static string str8_to_utf8(const char16_t tbl[256], const char *str, int len)
{
	len = check_NULL_terminator(str, len);

	string s_utf8;
	s_utf8.reserve(len + 8);

	for (; *str != '\0' && len > 0; str++, len--) {
		// NOTE: The (uint8_t) cast is required.
		// Otherwise, *str is interpreted as a signed char,
		// which causes all sorts of shenanigans.
		char16_t ch16 = tbl[(uint8_t)*str];
		// NOTE: Masks for the first byte might not be needed...
		if (ch16 < 0x0080) {
			s_utf8 += (char)(ch16 & 0x7F);
		} else if (ch16 < 0x0800) {
			s_utf8 += (char)(0xC0 | ((ch16 >>  6) & 0x1F));
			s_utf8 += (char)(0x80 | ((ch16 >>  0) & 0x3F));
		} else /*if (ch16 < 0x10000)*/ {
			s_utf8 += (char)(0xE0 | ((ch16 >> 12) & 0x0F));
			s_utf8 += (char)(0x80 | ((ch16 >>  6) & 0x3F));
			s_utf8 += (char)(0x80 | ((ch16 >>  0) & 0x3F));
		}
	}

	return s_utf8;
}

/**
 * Convert Atari ST text to UTF-8.
 * Trailing NULL bytes will be removed.
 * @param str	[in] Atari ST text.
 * @param len	[in] Length of str, in bytes. (-1 for NULL-terminated string)
 * @return UTF-8 string.
 */
string atariST_to_utf8(const char *str, int len)
{
	// NOTE: libiconv supports Atari ST, but glibc iconv() does not.
	// TODO: Use '\n' for CR instead?

	// Atari ST lookup table.
	// Reference: https://en.wikipedia.org/wiki/Atari_ST_character_set
	// - Index: 8-bit character.
	// - Value: 16-bit UTF-16 codepoint.
	// NOTE: Some codepoints do not exist in Unicode.
	// They will be represented by U+FFFD.
	// NOTE: A few codepoints can be represented by emojis.
	// Those won't be supported here.
	static const char16_t atariST_lkup[256] = {
		// 0x00
		0x0000, 0x21E7, 0x21E9, 0x21E8, 0x21E6, 0x274E, 0xFFFD, 0xFFFD,
		0x2713, 0xFFFD, '\n', 0x266A, 0x240C, '\r', 0xFFFD, 0xFFFD,
		// 0x10
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		0xFFFD, 0xFFFD, 0x0259, 0x241B, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		// 0x20
		' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
		// 0x30
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
		// 0x40
		'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
		// 0x50
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
		// 0x60
		'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
		// 0x70
		'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', 0x2302,
		// 0x80
		0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
		0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
		// 0x90
		0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
		0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x00DF, 0x0192,
		// 0xA0
		0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
		0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
		// 0xB0
		0x00E3, 0x00F5, 0x00D8, 0x00F8, 0x0153, 0x0152, 0x00C0, 0x00C3,
		0x00D5, 0x00A8, 0x00B4, 0x2020, 0x00B6, 0x00A9, 0x00AE, 0x2122,
		// 0xC0
		0x0133, 0x0132, 0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5,
		0x05D6, 0x05D7, 0x05D8, 0x05D9, 0x05DB, 0x05DC, 0x05DE, 0x05E0,
		// 0xD0
		0x05E1, 0x05E2, 0x05E4, 0x05E6, 0x05E7, 0x05E8, 0x05E9, 0x05EA,
		0x05DF, 0x05DA, 0x05DD, 0x05E3, 0x05E5, 0x00A7, 0x2227, 0x221E,
		// 0xE0
		0x03B1, 0x03B2, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
		0x03A6, 0x0398, 0x03A9, 0x03B4, 0x222E, 0x03D5, 0x2208, 0x2229,
		// 0xF0
		0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
		0x00B0, 0x2022, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x00B3, 0x00AF,
	};

	return str8_to_utf8(atariST_lkup, str, len);
}

/**
 * Convert Atari ATASCII text to UTF-8.
 * Trailing NULL bytes will be removed.
 * @param str	[in] Atari ATASCII text.
 * @param len	[in] Length of str, in bytes. (-1 for NULL-terminated string)
 * @return UTF-8 string.
 */
string atascii_to_utf8(const char *str, int len)
{
	// TODO: Use '\n' for CR instead?

	// Atari 8-bit ATASCII lookup table.
	// Reference: https://en.wikipedia.org/wiki/ATASCII
	// - Index: 8-bit character.
	// - Value: 16-bit UTF-16 codepoint.
	// NOTE: Some codepoints do not exist in Unicode.
	// They will be represented by U+FFFD.
	// NOTE: Codepoints 128-255 are the same as 0-127 but inverted,
	// with exceptions for certain control characters. The inverted
	// symbols can't be represented easily, so we'll use whatever
	// Wikipedia has listed for symbols and duplicate the ASCII area.
	static const char16_t atascii_lkup[256] = {
		// 0x00
		0x2665, 0x251C, 0xFFFD, 0x2518, 0x2524, 0x2510, 0x2571, 0x2572,
		0x25E2, 0x2597, 0x25E3, 0x259D, 0x2598, 0xFFFD, 0x2582, 0x2596,
		// 0x10
		0x2663, 0x250C, 0x2500, 0x253C, 0x25CF, 0x2584, 0x258E, 0x252C,
		0x2534, 0x258C, 0x2514, 0x001B, 0x2191, 0x2193, 0x2190, 0x2192,
		// 0x20
		' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
		// 0x30
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
		// 0x40
		'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
		// 0x50
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
		// 0x60
		0x2666, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
		// 0x70
		'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 0x2660, '|', 0x21B0, 0x25C0, 0x25B6,

		// 0x80
		0xFFFD, 0xFFFD, 0x258A, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		0x25E4, 0x259B, 0x25E5, 0x2599, 0x259F, 0x2586, 0xFFFD, 0x259C,
		// 0x90
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0x2580, 0xFFFD, 0xFFFD,
		0xFFFD, 0x2590, 0xFFFD, '\n', 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		// 0xA0
		0x2588, '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
		// 0xB0
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
		// 0xC0
		'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
		// 0xD0
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
		// 0xE0
		0xFFFD, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
		// 0xF0
		'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 0xFFFD, '|', 0xFFFD, 0xFFFD, 0xFFFD,
	};

	return str8_to_utf8(atascii_lkup, str, len);
}

/**
 * Convert Commodore PETSCII text (C64 variant) to UTF-8.
 * Trailing NULL bytes will be removed.
 * @param str	[in] Commodore PETSCII text.
 * @param len	[in] Length of str, in bytes. (-1 for NULL-terminated string)
 * @param shifted [in] False for unshifted (uppercase+graphics); true for shifted (lowercase+uppercase).
 * @return UTF-8 string.
 */
string petscii_to_utf8(const char *str, int len, bool shifted)
{
	// TODO: Use '\n' for CR instead?

	// Commodore PETSCII lookup tables
	// Reference: https://en.wikipedia.org/wiki/PETSCII
	// - Index: 8-bit character.
	// - Value: 16-bit UTF-16 codepoint.
	// NOTE: Some codepoints do not exist in Unicode.
	// They will be represented by U+FFFD.

	// Based on C64.

	// Unshifted. (uppercase+graphics)
	static const char16_t petscii_unshifted_lkup[256] = {
		// 0x00
		0xFFFD, 0xFFFD, 0xFFFD, 0x0003, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		0xFFFD, 0xFFFD, '\n', 0xFFFD, 0xFFFD, '\r', 0xFFFD, 0xFFFD,
		// 0x10
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0x007F, 0xFFFD, 0xFFFD, 0xFFFD,
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		// 0x20
		' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
		// 0x30
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
		// 0x40
		'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
		// 0x50
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', 0x00A3, ']', 0x2191, 0x2190,
		// 0x60
		0x2500, 0x2660, 0x2502, 0x2500, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		0xFFFD, 0x256E, 0x2570, 0x256F, 0xFFFD, 0x2572, 0x2571, 0xFFFD,
		// 0x70
		0xFFFD, 0x25CF, 0xFFFD, 0x2665, 0xFFFD, 0x256D, 0x2573, 0x25CB,
		0x2663, 0xFFFD, 0x2666, 0x253C, 0xFFFD, 0x2502, 0x03C0, 0x25E5,
		// 0x80
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, '\n', 0xFFFD, 0xFFFD,
		// 0x90
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		// 0xA0
		0x00A0, 0x258C, 0x2584, 0x2594, 0x2581, 0x258F, 0x2592, 0x2595,
		0xFFFD, 0x25E4, 0xFFFD, 0x251C, 0x2597, 0x2514, 0x2510, 0x2582,
		// 0xB0
		0x250C, 0x2534, 0x252C, 0x2524, 0x258E, 0x258D, 0xFFFD, 0xFFFD,
		0xFFFD, 0x258E, 0xFFFD, 0x2596, 0x259D, 0x2518, 0x2598, 0x259A,

		/** 0xC0-0xDF is identical to 0x60-0x7F. **/

		// 0xC0
		0x2500, 0x2660, 0x2502, 0x2500, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		0xFFFD, 0x256E, 0x2570, 0x256F, 0xFFFD, 0x2572, 0x2571, 0xFFFD,
		// 0xD0
		0xFFFD, 0x25CF, 0xFFFD, 0x2665, 0xFFFD, 0x256D, 0x2573, 0x25CB,
		0x2663, 0xFFFD, 0x2666, 0x253C, 0xFFFD, 0x2502, 0x03C0, 0x25E5,

		/** 0xE0-0xFF is identical to 0xA0-0xBF. **/

		// 0xE0
		0x00A0, 0x258C, 0x2584, 0x2594, 0x2581, 0x258F, 0x2592, 0x2595,
		0xFFFD, 0x25E4, 0xFFFD, 0x251C, 0x2597, 0x2514, 0x2510, 0x2582,
		// 0xF0
		0x250C, 0x2534, 0x252C, 0x2524, 0x258E, 0x258D, 0xFFFD, 0xFFFD,
		0xFFFD, 0x258E, 0xFFFD, 0x2596, 0x259D, 0x2518, 0x2598, 0x259A,
	};

	// Shifted. (lowercase+uppercase)
	static const char16_t petscii_shifted_lkup[256] = {
		// 0x00
		0xFFFD, 0xFFFD, 0xFFFD, 0x0003, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		0xFFFD, 0xFFFD, '\n', 0xFFFD, 0xFFFD, '\r', 0xFFFD, 0xFFFD,
		// 0x10
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0x007F, 0xFFFD, 0xFFFD, 0xFFFD,
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		// 0x20
		' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
		// 0x30
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
		// 0x40
		'@', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
		// 0x50
		'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '[', 0x00A3, ']', 0x2191, 0x2190,
		// 0x60
		0x2500, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
		// 0x70
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0x253C, 0xFFFD, 0x2502, 0x2592, 0xFFFD,
		// 0x80
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, '\n', 0xFFFD, 0xFFFD,
		// 0x90
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
		// 0xA0
		0x00A0, 0x258C, 0x2584, 0x2594, 0x2581, 0x258F, 0x2592, 0x2595,
		0xFFFD, 0x25E4, 0xFFFD, 0x251C, 0x2597, 0x2514, 0x2510, 0x2582,
		// 0xB0
		0x250C, 0x2534, 0x252C, 0x2524, 0x258E, 0x258D, 0xFFFD, 0xFFFD,
		0xFFFD, 0x258E, 0x2713, 0x2596, 0x259D, 0x2518, 0x2598, 0x259A,

		/** 0xC0-0xDF is identical to 0x60-0x7F. **/

		// 0xC0
		0x2500, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
		// 0xD0
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0x253C, 0xFFFD, 0x2502, 0x2592, 0xFFFD,

		/** 0xE0-0xFF is identical to 0xA0-0xBF. **/

		// 0xE0
		0x00A0, 0x258C, 0x2584, 0x2594, 0x2581, 0x258F, 0x2592, 0x2595,
		0xFFFD, 0x25E4, 0xFFFD, 0x251C, 0x2597, 0x2514, 0x2510, 0x2582,
		// 0xF0
		0x250C, 0x2534, 0x252C, 0x2524, 0x258E, 0x258D, 0xFFFD, 0xFFFD,
		0xFFFD, 0x258E, 0x2713, 0x2596, 0x259D, 0x2518, 0x2598, 0x259A,
	};

	return str8_to_utf8(shifted ? petscii_shifted_lkup : petscii_unshifted_lkup, str, len);
}

}