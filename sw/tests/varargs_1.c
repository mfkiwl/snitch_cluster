// Copyright 2020 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
#include <stdarg.h>

// Use `-O0` for this function and don't inline.
int __attribute__((noinline)) __attribute__((optnone)) sum(int N, ...) {
    int sum = 0;
    va_list va;
    va_start(va, N);
    for (int i = 0; i < N; i++) {
        sum += va_arg(va, int);
    }
    va_end(va);
    return sum;
}

int main() {
    int e = 0;
    e += sum(1, 1) != 1;
    e += sum(2, 1, 2) != 1 + 2;
    e += sum(3, 4, 5, 6) != 4 + 5 + 6;
    e += sum(4, 2, 6, 8, 1) != 2 + 6 + 8 + 1;
    return e;
}
