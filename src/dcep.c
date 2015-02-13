// dcep.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "common.h"
#include "sctp.h"
#include "dcep.h"

void
handle_rtcdc_message(struct sctp_transport *sctp, void *packets, size_t len,
                     uint32_t ppid, uint16_t sid)
{
  switch (ppid) {
    case WEBRTC_CONTROL_PPID:
      {
        uint8_t msg_type = ((uint8_t *)packets)[0];
        if (msg_type == DATA_CHANNEL_OPEN) {
          fprintf(stderr, "rtcdc open request\n");
        } else if (msg_type == DATA_CHANNEL_ACK) {
          fprintf(stderr, "rtcdc open ack\n");
        }
      }
      break;
    case WEBRTC_STRING_PPID:
    case WEBRTC_STRING_PARTIAL_PPID:
    case WEBRTC_BINARY_PPID:
    case WEBRTC_BINARY_PARTIAL_PPID:
      fprintf(stderr, "rtcdc string/binary\n");
      break;
    case WEBRTC_STRING_EMPTY_PPID:
    case WEBRTC_BINARY_EMPTY_PPID:
      break;
    default:
      break;
  }
}
