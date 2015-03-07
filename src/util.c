// util.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a BSD license.

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "util.h"

int
random_integer(int min, int max)
{
  assert(min < max);
  srand(time(NULL));
  return rand() % (max + 1 - min) + min;
}

void
random_number_string(char *dest, int len)
{
  const static char *numbers = "0123456789";
  srand(time(NULL));
  for (int i = 0; i < len; ++i) {
    int r = rand() % 10;
    *(dest + i) = numbers[r];
  }
}
