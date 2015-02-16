// dcep.h
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#ifndef _RTCDC_DCEP_H_
#define _RTCDC_DCEP_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define WEBRTC_CONTROL_PPID        50
#define WEBRTC_STRING_PPID         51
#define WEBRTC_BINARY_PARTIAL_PPID 52
#define WEBRTC_BINARY_PPID         53
#define WEBRTC_STRING_PARTIAL_PPID 54
#define WEBRTC_STRING_EMPTY_PPID   56
#define WEBRTC_BINARY_EMPTY_PPID   57

#define DATA_CHANNEL_OPEN 0x03
#define DATA_CHANNEL_ACK  0x02

#define DATA_CHANNEL_RELIABLE                          0x00
#define DATA_CHANNEL_RELIABLE_UNORDERED                0x80
#define DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT           0x01
#define DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT_UNORDERED 0x81
#define DATA_CHANNEL_PARTIAL_RELIABLE_TIMED            0x02
#define DATA_CHANNEL_PARTIAL_RELIABLE_TIMED_UNORDERED  0x82

#define DATA_CHANNEL_PRIORITY_BELOW_NORMAL 128
#define DATA_CHANNEL_PRIORITY_NORMAL       256
#define DATA_CHANNEL_PRIORITY_HIGH         512
#define DATA_CHANNEL_PRIORITY_EXTRA_HIGH   1024

#define DATA_CHANNEL_CLOSED     0
#define DATA_CHANNEL_CONNECTING 1
#define DATA_CHANNEL_CONNECTED  2

struct dcep_open_message {
  uint8_t message_type;
  uint8_t channel_type;
  uint16_t priority;
  uint32_t reliability_param;
  uint16_t label_length;
  uint16_t protocol_length;
  char label_and_protocol[0];
} __attribute__((packed, aligned(1)));

struct dcep_ack_message {
  uint8_t message_type;
} __attribute__((packed, aligned(1)));

struct data_channel {
  uint8_t type;
  uint16_t priority;
  uint32_t rtx;
  uint32_t lifetime;
  char *label;
  char *protocol;
  int state;
  uint16_t sid;
  void (*on_message)(struct data_channel *ch, void *buf, int len);
};

void
handle_rtcdc_message(struct sctp_transport *sctp, void *packets, size_t len,
                     uint32_t ppid, uint16_t sid);

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_DCEP_H_
