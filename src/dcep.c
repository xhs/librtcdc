// dcep.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "common.h"
#include "sctp.h"
#include "dcep.h"

static struct data_channel *
new_data_channel(struct dcep_open_message *open_req, uint16_t sid)
{
  struct data_channel *ch = (struct data_channel *)calloc(1, sizeof *ch);
  if (ch == NULL)
    return NULL;

  ch->type = open_req->channel_type;
  ch->priority = ntohs(open_req->priority);
  if (ch->type & 0x01)
    ch->rtx = ntohl(open_req->reliability_param);
  else if (ch->type & 0x02)
    ch->lifetime = ntohl(open_req->reliability_param);
  if (open_req->label_length > 0)
    ch->label = strndup(open_req->label_and_protocol, open_req->label_length);
  if (open_req->protocol_length > 0)
    ch->protocol = strndup(open_req->label_and_protocol + open_req->label_length, open_req->protocol_length);
  ch->state = DATA_CHANNEL_CONNECTED;
  ch->sid = sid;

  return ch;
}

static void
handle_rtcdc_open_request(struct sctp_transport *sctp, uint16_t sid, void *packets, size_t len)
{
  struct dcep_open_message *open_req = (struct dcep_open_message *)packets;
  if (len < sizeof *open_req)
    return;

  struct dcep_ack_message ack;
  ack.message_type = DATA_CHANNEL_ACK;

  struct sctp_sndinfo info;
  memset(&info, 0, sizeof info);
  info.snd_sid = sid;
  info.snd_ppid = WEBRTC_CONTROL_PPID;

  if (usrsctp_sendv(sctp->sock, &ack, sizeof ack, NULL, 0,
                    &info, sizeof info, SCTP_SENDV_SNDINFO, 0) < 0) {
    fprintf(stderr, "sending ack failed\n");
    return;
  }

  int i;
  for (i = 0; i < sctp->channel_num; ++i) {
    if (sctp->channels[i])
      continue;
    break;
  }

  if (i == sctp->channel_num) {
    struct data_channel **new_channels =
      (struct data_channel **)calloc(i + CHANNEL_NUMBER_STEP, sizeof(struct data_channel *));
    if (new_channels == NULL)
      return;

    memcpy(new_channels, sctp->channels, i * sizeof(struct data_channel));
    free(sctp->channels);
    sctp->channels = new_channels;
    sctp->channel_num += CHANNEL_NUMBER_STEP;
  }

  struct data_channel *ch = new_data_channel(open_req, sid);
  if (ch == NULL)
    return;
  sctp->channels[i] = ch;

  if (sctp->on_channel)
    sctp->on_channel(ch);
}

static void
handle_rtcdc_open_ack(struct sctp_transport *sctp, uint16_t sid)
{
  for (int i = 0; i < sctp->channel_num; ++i) {
    struct data_channel *ch = sctp->channels[i];
    if (ch && ch->sid == sid) {
      ch->state = DATA_CHANNEL_CONNECTED;
      break;
    }
  }
}

static void
handle_rtcdc_data(struct sctp_transport *sctp, uint16_t sid, int type, void *packets, size_t len)
{
  for (int i = 0; i < sctp->channel_num; ++i) {
    struct data_channel *ch = sctp->channels[i];
    if (ch && ch->sid == sid) {
      if (ch->state == DATA_CHANNEL_CLOSED)
        ch->state = DATA_CHANNEL_CONNECTED;

      if (ch->on_message)
        ch->on_message(ch, type, packets, len);

      break;
    }
  }
}

void
handle_rtcdc_message(struct sctp_transport *sctp, void *packets, size_t len,
                     uint32_t ppid, uint16_t sid)
{
  switch (ppid) {
    case WEBRTC_CONTROL_PPID:
      {
        uint8_t msg_type = ((uint8_t *)packets)[0];
        if (msg_type == DATA_CHANNEL_OPEN)
          handle_rtcdc_open_request(sctp, sid, packets, len);
        else if (msg_type == DATA_CHANNEL_ACK)
          handle_rtcdc_open_ack(sctp, sid);
      }
      break;
    case WEBRTC_STRING_PPID:
    case WEBRTC_STRING_PARTIAL_PPID:
      handle_rtcdc_data(sctp, sid, DATA_TYPE_STRING, packets, len);
      break;
    case WEBRTC_BINARY_PPID:
    case WEBRTC_BINARY_PARTIAL_PPID:
      handle_rtcdc_data(sctp, sid, DATA_TYPE_BINARY, packets, len);
      break;
    case WEBRTC_STRING_EMPTY_PPID:
    case WEBRTC_BINARY_EMPTY_PPID:
      handle_rtcdc_data(sctp, sid, DATA_TYPE_EMPTY, packets, len);
      break;
    default:
      break;
  }
}

struct data_channel *
create_reliable_data_channel(struct sctp_transport *sctp, const char *label, const char *protocol,
                             void (*on_message)(struct data_channel *ch, int datatype, void *packets, size_t len))
{
  if (sctp == NULL || label == NULL)
    return NULL;

  struct dcep_open_message *req;
  int rlen = sizeof *req + strlen(label) + strlen(protocol);
  req = (struct dcep_open_message *)calloc(1, rlen);
  if (req == NULL)
    return NULL;

  int i;
  for (i = 0; i < sctp->channel_num; ++i) {
    if (sctp->channels[i])
      continue;
    break;
  }

  if (i == sctp->channel_num) {
    struct data_channel **new_channels =
      (struct data_channel **)calloc(i + CHANNEL_NUMBER_STEP, sizeof(struct data_channel *));
    if (new_channels == NULL) {
      free(req);
      return NULL;
    }

    memcpy(new_channels, sctp->channels, i * sizeof(struct data_channel));
    free(sctp->channels);
    sctp->channels = new_channels;
    sctp->channel_num += CHANNEL_NUMBER_STEP;
  }

  struct data_channel *ch = (struct data_channel *)calloc(1, sizeof *ch);
  if (ch == NULL) {
    free(req);
    return NULL;
  }
  sctp->channels[i] = ch;

  ch->type = DATA_CHANNEL_RELIABLE;
  ch->state = DATA_CHANNEL_CONNECTING;
  ch->on_message = on_message;
  ch->label = strdup(label);
  ch->protocol = strdup(protocol);
  ch->sid = sctp->stream_cursor;
  sctp->stream_cursor += 2;

  req->message_type = DATA_CHANNEL_OPEN;
  req->channel_type = ch->type;
  req->priority = htons(0);
  req->reliability_param = htonl(0);
  req->label_length = htons(strlen(label));
  req->protocol_length = htons(strlen(protocol));
  memcpy(req->label_and_protocol, label, strlen(label));
  memcpy(req->label_and_protocol + strlen(label), protocol, strlen(protocol));

  struct sctp_sndinfo info;
  memset(&info, 0, sizeof info);
  info.snd_sid = ch->sid;
  info.snd_flags = SCTP_EOR;
  info.snd_ppid = WEBRTC_CONTROL_PPID;

  int s = usrsctp_sendv(sctp->sock, req, rlen, NULL, 0,
                        &info, sizeof info, SCTP_SENDV_SNDINFO, 0);
  free(req);

  if (s < 0) {
    free(ch);
    ch = NULL;
    sctp->channels[i] = NULL;
  }

  return ch;
}
