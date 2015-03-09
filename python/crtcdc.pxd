# crtcdc.pxd
# Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
# This file is licensed under a BSD license.

ctypedef unsigned int uint16_t

cdef struct peer_callbacks:
  void *on_channel
  void *on_candidate

cdef struct channel_callbacks:
  void *on_open
  void *on_message
  void *on_close

cdef extern from "rtcdc.h":
  cdef struct rtcdc_peer_connection:
    char *stun_server
    uint16_t stun_port
    void (*on_channel)(rtcdc_data_channel *channel, void *user_data)
    void (*on_candidate)(const char *candidate, void *user_data)
    void *user_data

  cdef struct rtcdc_data_channel:
    char *label
    char *protocol
    int state
    void (*on_open)(rtcdc_data_channel *channel, void *user_data)
    void (*on_message)(rtcdc_data_channel *channel, \
                       int datatype, void *data, size_t length, void *user_data)
    void (*on_close)(rtcdc_data_channel *channel, void *user_data)
    void *user_data

  rtcdc_peer_connection * \
  rtcdc_create_peer_connection(void (*on_channel)(rtcdc_data_channel *, void *user_data), \
                               void (*on_candidate)(const char *candidate, void *user_data), \
                               const char *stun_server, uint16_t port, void *user_data)

  void \
  rtcdc_destroy_peer_connection(rtcdc_peer_connection *peer)

  char * \
  rtcdc_generate_offer_sdp(rtcdc_peer_connection *peer)

  int \
  rtcdc_parse_offer_sdp(rtcdc_peer_connection *peer, const char *offer)

  int \
  rtcdc_parse_candidate_sdp(rtcdc_peer_connection *peer, const char *candidates)

  rtcdc_data_channel * \
  rtcdc_create_data_channel(rtcdc_peer_connection *peer, \
                            const char *label, const char *protocol, \
                            void (*on_open)(rtcdc_data_channel *channel, void *user_data), \
                            void (*on_message)(rtcdc_data_channel *channel, \
                                               int datatype, void *data, size_t length, void *user_data), \
                            void (*on_close)(rtcdc_data_channel *channel, void *user_data), \
                            void *user_data)

  void \
  rtcdc_destroy_data_channel(rtcdc_data_channel *channel)

  int \
  rtcdc_send_message(rtcdc_data_channel *channel, int datatype, void *data, size_t length)

  void \
  rtcdc_loop(rtcdc_peer_connection *peer)
