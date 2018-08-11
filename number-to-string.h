#ifndef NUMBER_TO_STRING_H_INCLUDED
#define NUMBER_TO_STRING_H_INCLUDED

#include <limits.h>

// Size of the character buffer passed to NumberToString().
#define NUMBER_BUF_SIZE (sizeof(unsigned long) * CHAR_BIT / 2)

// Converts an unsigned long value to its decimal string representation, with
// groups of three digits separated by commas. For example, 12345 is rendered
// as "12,345".
char *NumberToString(unsigned long value, char (*buf)[NUMBER_BUF_SIZE]);

#endif /* ndef NUMBER_TO_STRING_H_INCLUDED */
