#include "parse-frequency.h"

#include <ctype.h>
#include <limits.h>

static bool IsAsciiDigit(char ch) {
    return ch >= '0' && ch <= '9';
}

bool ParseFrequency(const char *str, unsigned long *result) {
    // Logically, the result is defined as val * multi / scale.
    unsigned long val = 0;
    unsigned long scale = 0;
    unsigned long multi = 1;
    const char *p = str;
    while (isspace(*p)) ++p;
    if (!IsAsciiDigit(*p)) {
        // Must start with a digit.
        return false;
    }
    for ( ; *p; ++p) {
        if (IsAsciiDigit(*p)) {
            int add = *p - '0';
            if (val > (ULONG_MAX - add)/10) {
                return false;
            }
            val = 10*val + add;
            if (scale > ULONG_MAX/10) {
                return false;
            }
            scale *= 10;
        } else if (*p == '.' || *p == ',') {
            // Referencing p[-1] and p[1] is safe; we know we started the loop
            // at a digit (which must precede p) and str is zero-terminated,
            // while *p != '\0' here (so the zero terminator must succeed p).
            if (!IsAsciiDigit(p[-1]) || !IsAsciiDigit(p[1])) {
                // Periods and commas can only occur between digits.
                return false;
            }
            if (*p == '.') {
                if (scale != 0) {
                    // Can't contain more than one decimal point.
                    return false;
                }
                scale = 1;
            } else {
                // Grouping comma is ignored.
            }
        } else {
            break;
        }
    }
    if (scale == 0) {
        scale = 1;
    }
    while (*p && isspace(*p)) ++p;
    if (*p) {
        char ch = tolower(*p++);
        if (ch == 'k') {
            multi = 1;
        } else if (ch == 'm') {
            multi = 1000;
        } else if (ch == 'g') {
            multi = 1000000;
        } else {
            return false;
        }
        if (*p && (tolower(*p++) != 'h' || tolower(*p++) != 'z')) {
            return false;
        }
    }
    while (*p && isspace(*p)) ++p;
    if (*p) {
        return false;
    }
    // multi and scale are both powers of 10, so one must divide the other.
    if (multi > scale) {
        multi /= scale;
        if (val > ULONG_MAX/multi) {
            return false;
        }
        *result = val * multi;
        return true;
    } else {
        scale /= multi;
        *result = val / scale;
        return true;
    }
}
