#include "number-to-string.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int failures = 0;

static void TestNumberToString(unsigned long value, const char *expected) {
    char buf[NUMBER_BUF_SIZE];
    if (strlen(expected) >= sizeof(buf)) {
        fprintf(stderr, "NUMBER_BUF_SIZE (%d) too small for expected result (%s)\n", (int) NUMBER_BUF_SIZE, expected);
        ++failures;
        return;
    }
    char *result = NumberToString(value, &buf);
    assert(result >= buf && result < buf + sizeof(buf));
    if (strcmp(result, expected) != 0) {
        fprintf(stderr, "NumberToString(%lu) failed: expected '%s', received '%s\n", value, expected, result);
        ++failures;
    }
}

int main() {
    // We expect long to be exactly 32 or 64 bits in size. If this fails, e.g.
    // on a platform with 128 bit longs, please add additional testcases.
    assert(ULONG_MAX == UINT32_MAX || ULONG_MAX == UINT64_MAX);

    TestNumberToString(0, "0");
    TestNumberToString(1, "1");
    TestNumberToString(42, "42");
    TestNumberToString(999, "999");
    TestNumberToString(1000, "1,000");
    TestNumberToString(10000, "10,000");
    TestNumberToString(100000, "100,000");
    TestNumberToString(999999, "999,999");
    TestNumberToString(1000000, "1,000,000");
    TestNumberToString(123456789, "123,456,789");
    TestNumberToString(1234567890, "1,234,567,890");
    TestNumberToString(UINT32_MAX, "4,294,967,295");
    if (ULONG_MAX >= UINT64_MAX) {
        TestNumberToString(UINT64_MAX, "18,446,744,073,709,551,615");
    }
    return failures;
}
