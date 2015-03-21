// rtcdc.h
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a BSD license.

#ifndef _RTCDC_H_
#define _RTCDC_H_

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef RTCDC_MAX_CHANNEL_NUM
#define RTCDC_MAX_CHANNEL_NUM 128
#endif

#ifndef RTCDC_MAX_IN_STREAM
#define RTCDC_MAX_IN_STREAM 1024
#endif

#ifndef RTCDC_MAX_OUT_STREAM
#define RTCDC_MAX_OUT_STREAM 256
#endif

#define RTCDC_PEER_ROLE_UNKNOWN 0
#define RTCDC_PEER_ROLE_CLIENT  1
#define RTCDC_PEER_ROLE_SERVER  2

#define RTCDC_CHANNEL_STATE_CLOSED     0
#define RTCDC_CHANNEL_STATE_CONNECTING 1
#define RTCDC_CHANNEL_STATE_CONNECTED  2

#define RTCDC_DATATYPE_STRING 0
#define RTCDC_DATATYPE_BINARY 1
#define RTCDC_DATATYPE_EMPTY  2

struct ice_transport;
struct dtls_context;
struct dtls_transport;
struct sctp_transport;
struct rtcdc_data_channel;

typedef void (*rtcdc_on_open_cb)(struct rtcdc_data_channel *channel, void *user_data);

typedef void (*rtcdc_on_message_cb)(struct rtcdc_data_channel *channel,
                                    int datatype, void *data, size_t len, void *user_data);

typedef void (*rtcdc_on_close_cb)(struct rtcdc_data_channel *channel, void *user_data);

typedef void (*rtcdc_on_channel_cb)(struct rtcdc_peer_connection *peer,
                                    struct rtcdc_data_channel *channel, void *user_data);

typedef void (*rtcdc_on_candidate_cb)(struct rtcdc_peer_connection *peer,
                                      const char *candidate, void *user_data);

typedef void (*rtcdc_on_connect_cb)(struct rtcdc_peer_connection *peer, void *user_data);

struct rtcdc_data_channel {
  uint8_t type;
  uint16_t priority;
  uint32_t rtx;
  uint32_t lifetime;
  char *label;
  char *protocol;
  int state;
  uint16_t sid;
  struct sctp_transport *sctp;
  rtcdc_on_open_cb on_open;
  rtcdc_on_message_cb on_message;
  rtcdc_on_close_cb on_close;
  void *user_data;
};

struct rtcdc_transport {
  struct dtls_context *ctx;
  struct ice_transport *ice;
  struct dtls_transport *dtls;
  struct sctp_transport *sctp;
};

struct rtcdc_peer_connection {
  char *stun_server;
  uint16_t stun_port;
  int exit_thread;
  struct rtcdc_transport *transport;
  int initialized;
  int role;
  struct rtcdc_data_channel *channels[RTCDC_MAX_CHANNEL_NUM];
  rtcdc_on_channel_cb on_channel;
  rtcdc_on_candidate_cb on_candidate;
  rtcdc_on_connect_cb on_connect;
  void *user_data;
};

struct rtcdc_peer_connection *
rtcdc_create_peer_connection(rtcdc_on_channel_cb, rtcdc_on_candidate_cb, rtcdc_on_connect_cb,
                             const char *stun_server, uint16_t stun_port,
                             void *user_data);

void
rtcdc_destroy_peer_connection(struct rtcdc_peer_connection *peer);

char *
rtcdc_generate_offer_sdp(struct rtcdc_peer_connection *peer);

char *
rtcdc_generate_local_candidate_sdp(struct rtcdc_peer_connection *peer);

int
rtcdc_parse_offer_sdp(struct rtcdc_peer_connection *peer, const char *offer);

int
rtcdc_parse_candidate_sdp(struct rtcdc_peer_connection *peer, const char *candidates);

struct rtcdc_data_channel *
rtcdc_create_data_channel(struct rtcdc_peer_connection *peer,
                          const char *label, const char *protocol,
                          rtcdc_on_open_cb, rtcdc_on_message_cb, rtcdc_on_close_cb,
                          void *user_data);

void
rtcdc_destroy_data_channel(struct rtcdc_data_channel *channel);

int
rtcdc_send_message(struct rtcdc_data_channel *channel, int datatype, void *data, size_t len);

void
rtcdc_loop(struct rtcdc_peer_connection *peer);

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_H_
