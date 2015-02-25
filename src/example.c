// example.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rtcdc.h"

int main(int argc, char *argv[])
{
  struct rtcdc_peer_connection peer;
  printf("%zu\n", sizeof peer);
  return 0;
}
