/**
 * Q2Admin
 *
 * Sample program for testing regular expressions using the same code
 * q2admin uses for matching.
 *
 * Build command:
 * gcc -c -o g_regex.o g_regex.c
 * gcc -o test-re test-re.c g_regex.o
 */

#include <stdio.h>
#include "g_regex.h"

int main(int argc, char **argv) {
    re_t r;
    int len;

    if (argc < 2) {
        printf("Usage: %s \"pattern\" \"teststring\"\n", argv[0]);
        return 0;
    }

    r = re_compile(argv[1]);
    if (!r) {
        printf("can't compile pattern \"%s\"\n", argv[1]);
        return 0;
    }

    if (re_matchp(r, argv[2], &len) == 0) {
        printf("\"%s\" matches \"%s\"\n", argv[1], argv[2]);
    } else {
        printf("NO match\n");
    }
}

