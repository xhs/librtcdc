# crtcdc.pxd
# Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
# This file is licensed under a GNU GPLv3 license.

ctypedef unsigned int uint16_t

cdef extern from "rtcdc.h":
  cdef struct rtcdc_peer_connection:
    pass

  cdef struct rtcdc_data_channel:
    char *label
    char *protocol
    int state
    void (*on_message)(rtcdc_data_channel *channel, \
                       int datatype, void *data, size_t length, void *callback)
    void *user_data

  rtcdc_peer_connection * \
  rtcdc_create_peer_connection(void (*on_channel)(rtcdc_data_channel *, void *), \
                               const char *stun_server, uint16_t port, void *callback)

  void \
  rtcdc_destroy_peer_connection(rtcdc_peer_connection *peer)

  char * \
  rtcdc_generate_offer_sdp(rtcdc_peer_connection *peer)

  int \
  rtcdc_parse_offer_sdp(rtcdc_peer_connection *peer, const char *offer)

  char * \
  rtcdc_generate_candidate_sdp(rtcdc_peer_connection *peer)

  int \
  rtcdc_parse_candidate_sdp(rtcdc_peer_connection *peer, const char *candidates)

  rtcdc_data_channel * \
  rtcdc_create_data_channel(rtcdc_peer_connection *peer, \
                            const char *label, const char *protocol, \
                            void (*on_message)(rtcdc_data_channel *channel, \
                                               int datatype, void *data, size_t length, void *callback), \
                            void *callback)

  void \
  rtcdc_destroy_data_channel(rtcdc_data_channel *channel)

  int \
  rtcdc_send_message(rtcdc_data_channel *channel, int datatype, void *data, size_t length)

  void \
  rtcdc_loop(rtcdc_peer_connection *peer)
