#include "number-to-string.h"

char *NumberToString(unsigned long value, char (*buf)[NUMBER_BUF_SIZE]) {
    char *p = *buf + NUMBER_BUF_SIZE;
    *--p = '\0';
    if (value == 0) {
        *--p = '0';
    } else {
        for (int n = 0; value > 0; ++n) {
            if (n == 3) {
                *--p = ',';
                n = 0;
            }
            *--p = '0' + value%10;
            value /= 10;
        }
    }
    return p;
}
