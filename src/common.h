// common.h
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#ifndef _RTCDC_COMMON_H_
#define _RTCDC_COMMON_H_

#ifdef  __cplusplus
extern "C" {
#endif

#define BUFFER_SIZE (1 << 16)

#define PEER_CLIENT 0
#define PEER_SERVER 1

#define SESSION_ID_SIZE 16

#define CHANNEL_NUMBER_BASE 16
#define CHANNEL_NUMBER_STEP 16

#define MAX_IN_STREAM  1024
#define MAX_OUT_STREAM 128

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_COMMON_H_
