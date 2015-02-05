// util.h
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#ifndef _RTCDC_UTIL_H_
#define _RTCDC_UTIL_H_

#ifdef  __cplusplus
extern "C" {
#endif

int
random_integer(int min, int max);

void
random_number_string(char *dest, int len);

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_UTIL_H_
