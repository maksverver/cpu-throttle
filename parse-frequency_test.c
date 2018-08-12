#include "parse-frequency.h"

#include <stdio.h>

static int failures;

// Arbitrary value that is different from any value that could legitimately
// occur during tests. Used to mark results that have not been overwritten.
#define INVALID_RESULT (~(unsigned long)31337)

static void ExpectFailure(const char *str) {
    unsigned long result = INVALID_RESULT;
    if (ParseFrequency(str, &result)) {
        fprintf(stderr, "ParseFrequency(\"%s\") unexpectedly succeeded!\n", str);
        ++failures;
    }
    if (result != INVALID_RESULT) {
        fprintf(stderr, "ParseFrequency(\"%s\") unexpectedly set result to %lu!\n", str, result);
        ++failures;
    }
}

static void ExpectSuccess(const char *str, unsigned long expected) {
    unsigned long result = INVALID_RESULT;
    if (!ParseFrequency(str, &result)) {
        fprintf(stderr, "ParseFrequency(\"%s\") failed!\n", str);
        ++failures;
    } else {
        if (result == INVALID_RESULT) {
            fprintf(stderr, "ParseFrequency(\"%s\") did not modify result!\n", str);
            ++failures;
        } else if (result != expected) {
            fprintf(stderr, "ParseFrequency(\"%s\") returned wrong result: "
                    "expected %lu, received %lu!\n", str, expected, result);
            ++failures;
        }
    }
}

int main() {
    // Basic numbers.
    ExpectSuccess("1", 1);
    ExpectSuccess("1234567890", 1234567890);
    ExpectSuccess("0100", 100);

    // Leading sign(s) are not allowed.
    ExpectFailure("+123");
    ExpectFailure("-123");

    // Some invalid strings.
    ExpectFailure("");
    ExpectFailure("garbage");
    ExpectFailure("1 eggs");
    ExpectFailure("uhm 1");

    // Grouping commas are allowed between digits
    ExpectSuccess("1,234,567", 1234567);
    ExpectSuccess("1,2,3", 123);
    ExpectFailure("1,,23");
    ExpectFailure(",123");
    ExpectFailure("123,");

    // Supported suffixes: kHz, Mhz, gHz.
    ExpectSuccess("42 kHz", 42);
    ExpectSuccess("42 mHz", 42000);
    ExpectSuccess("42 gHz", 42000000);
    ExpectFailure("42 Hz");
    ExpectFailure("42 gHz bla");
    ExpectFailure("42 gHzbla");

    // Whitespace before suffix is optional.
    ExpectSuccess("42mHz", 42000);
    ExpectSuccess("42m", 42000);

    // Suffix is case insensitive.
    ExpectSuccess("42K", 42);
    ExpectSuccess("42M", 42000);
    ExpectSuccess("42G", 42000000);
    ExpectSuccess("42khz", 42);
    ExpectSuccess("42mHZ", 42000);
    ExpectSuccess("42GhZ", 42000000);

    // Extra whitespace around string is acceptable.
    ExpectSuccess("\r123\tmHz\n", 123000);

    // Whitespace within number of suffix is not acceptable.
    ExpectFailure("12 3");
    ExpectFailure("123 m Hz");

    // One decimal point is allowed.
    ExpectSuccess("0.0", 0);
    ExpectSuccess("1.0", 1);
    ExpectSuccess("123.45", 123);
    ExpectSuccess("123.99", 123);
    ExpectFailure("1..0");
    ExpectFailure("1.,0");
    ExpectFailure("1,.0");
    ExpectFailure(".0");
    ExpectFailure("0.");
    ExpectFailure("1.M");
    ExpectFailure("1.0.0");

    // Decimals combine with suffix multipliers.
    ExpectSuccess("123.45m", 123450);
    ExpectSuccess("123.456m", 123456);
    ExpectSuccess("123.45678m", 123456);
    ExpectSuccess("123.45678g", 123456780);
    ExpectSuccess("123.456789g", 123456789);
    ExpectSuccess("1,234.56m", 1234560);
    ExpectSuccess("1.234,56g", 1234560);

    // Overflow is detected.
    // (These tests assume `unsigned long` to be at most 64 bits. If these
    // ever fail, e.g. on a 128-bit platform, please update the test cases.)
    ExpectFailure("100000000000000000000");
    ExpectFailure("1.00000000000000000000");
    ExpectFailure("0.00000000000000000000");
    ExpectFailure("20,000,000,000,000G");

    return failures;
}
