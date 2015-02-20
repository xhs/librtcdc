// rtcdc.h
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#ifndef _RTCDC_H_
#define _RTCDC_H_

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef RTCDC_MAX_CHANNEL_NUM
#define RTCDC_MAX_CHANNEL_NUM 128
#endif

struct rtcdc_transport;

struct rtcdc_data_channel {
  uint8_t type;
  uint16_t priority;
  uint32_t rtx;
  uint32_t lifetime;
  char *label;
  char *protocol;
  int state;
  uint16_t sid;
  void (*on_message)(struct rtcdc_data_channel *channel,
                     int datatype, void *data, size_t len);
  void *user_data;
};

struct rtcdc_peer_connection {
  struct rtcdc_transport *transport;
  struct rtcdc_data_channel *channels[RTCDC_MAX_CHANNEL_NUM];
  void (*on_channel)(struct rtcdc_data_channel *channel);
};

struct rtcdc_peer_connection *
rtcdc_create_peer_connection(void (*on_channel)(struct rtcdc_data_channel *channel));

void
rtcdc_destroy_peer_connection(struct rtcdc_peer_connection *peer);

char *
rtcdc_create_offer_sdp(struct rtcdc_peer_connection *peer);

int
rtcdc_create_answer_sdp(struct rtcdc_peer_connection *peer, const char *offer);

char **
rtcdc_generate_local_candidates(struct rtcdc_peer_connection *peer, int *num);

int
rtcdc_parse_remote_candidate(struct rtcdc_peer_connection *peer, const char *candidate);

struct rtcdc_data_channel *
rtcdc_create_reliable_data_channel(struct rtcdc_peer_connection *peer,
                                   const char *label, const char *protocol,
                                   void (*on_message)(struct rtcdc_data_channel *channel,
                                                      int datatype, void *data, size_t len),
                                   void *user_data);

void
rtcdc_destroy_data_channel(struct rtcdc_data_channel *channel);

int
rtcdc_send_message(struct rtcdc_data_channel *channel, int datatype, void *data, size_t len);

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_H_
