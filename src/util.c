// util.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include "util.h"

int
random_port(int min, int max)
{
  assert(min < max);
  srand(time(NULL));
  return rand() % (max + 1 - min) + min;
}
