/**
 * Copyright (C) The Q2Admin contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
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

