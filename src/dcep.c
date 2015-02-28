// dcep.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "common.h"
#include "sctp.h"
#include "dcep.h"
#include "rtcdc.h"

static struct rtcdc_data_channel *
allocate_new_data_channel(struct dcep_open_message *open_req, uint16_t sid)
{
  struct rtcdc_data_channel *ch = (struct rtcdc_data_channel *)calloc(1, sizeof *ch);
  if (ch == NULL)
    return NULL;

  ch->type = open_req->channel_type;
  ch->priority = ntohs(open_req->priority);
  if (ch->type & 0x01)
    ch->rtx = ntohl(open_req->reliability_param);
  else if (ch->type & 0x02)
    ch->lifetime = ntohl(open_req->reliability_param);
  if (open_req->label_length > 0)
    ch->label = strndup(open_req->label_and_protocol, ntohs(open_req->label_length));
  if (open_req->protocol_length > 0)
    ch->protocol = strndup(open_req->label_and_protocol + ntohs(open_req->label_length),
                           ntohs(open_req->protocol_length));
  ch->state = RTCDC_CHANNEL_STATE_CONNECTED;
  ch->sid = sid;

  return ch;
}

static void
handle_rtcdc_open_request(struct rtcdc_peer_connection *peer, uint16_t sid, void *data, size_t len)
{
  struct dcep_open_message *open_req = (struct dcep_open_message *)data;
  if (len < sizeof *open_req)
    return;

  int i;
  for (i = 0; i < RTCDC_MAX_CHANNEL_NUM; ++i) {
    if (peer->channels[i])
      continue;
    break;
  }

  if (i == RTCDC_MAX_CHANNEL_NUM)
    return;

  struct rtcdc_data_channel *ch = allocate_new_data_channel(open_req, sid);
  if (ch == NULL)
    return;
  ch->sctp = peer->transport->sctp;
  peer->channels[i] = ch;

  if (peer->on_channel)
    peer->on_channel(ch, peer->user_data);

  struct dcep_ack_message ack;
  ack.message_type = DATA_CHANNEL_ACK;

  if (send_sctp_message(ch->sctp, &ack, sizeof ack, sid, WEBRTC_CONTROL_PPID) < 0) {
#ifdef DEBUG_SCTP
    fprintf(stderr, "sending DCEP ack failed\n");
#endif
  }
}

static void
handle_rtcdc_open_ack(struct rtcdc_peer_connection *peer, uint16_t sid)
{
  for (int i = 0; i < RTCDC_MAX_CHANNEL_NUM; ++i) {
    struct rtcdc_data_channel *ch = peer->channels[i];
    if (ch && ch->sid == sid) {
      ch->state = RTCDC_CHANNEL_STATE_CONNECTED;
      break;
    }
  }
}

static void
handle_rtcdc_data(struct rtcdc_peer_connection *peer, uint16_t sid, int type, void *data, size_t len)
{
  for (int i = 0; i < RTCDC_MAX_CHANNEL_NUM; ++i) {
    struct rtcdc_data_channel *ch = peer->channels[i];
    if (ch && ch->sid == sid) {
      if (ch->state == RTCDC_CHANNEL_STATE_CLOSED)
        ch->state = RTCDC_CHANNEL_STATE_CONNECTED;

      if (ch->on_message)
        ch->on_message(ch, type, data, len, ch->user_data);

      break;
    }
  }
}

void
handle_rtcdc_message(struct rtcdc_peer_connection *peer, void *data, size_t len,
                     uint32_t ppid, uint16_t sid)
{
  switch (ppid) {
    case WEBRTC_CONTROL_PPID:
      {
        uint8_t msg_type = ((uint8_t *)data)[0];
        if (msg_type == DATA_CHANNEL_OPEN)
          handle_rtcdc_open_request(peer, sid, data, len);
        else if (msg_type == DATA_CHANNEL_ACK)
          handle_rtcdc_open_ack(peer, sid);
      }
      break;
    case WEBRTC_STRING_PPID:
    case WEBRTC_STRING_PARTIAL_PPID:
      handle_rtcdc_data(peer, sid, RTCDC_DATATYPE_STRING, data, len);
      break;
    case WEBRTC_BINARY_PPID:
    case WEBRTC_BINARY_PARTIAL_PPID:
      handle_rtcdc_data(peer, sid, RTCDC_DATATYPE_BINARY, data, len);
      break;
    case WEBRTC_STRING_EMPTY_PPID:
    case WEBRTC_BINARY_EMPTY_PPID:
      handle_rtcdc_data(peer, sid, RTCDC_DATATYPE_EMPTY, data, len);
      break;
    default:
      break;
  }
}
