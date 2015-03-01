# pyrtcdc.pyx
# Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
# This file is licensed under a GNU GPLv3 license.

cimport crtcdc
from crtcdc cimport rtcdc_peer_connection, rtcdc_data_channel

cdef void on_channel_callback(rtcdc_data_channel *channel, void *callback):
  cdef DataChannel dc = DataChannel()
  dc._channel = channel
  (<object>callback)(dc)

cdef void on_message_callback(rtcdc_data_channel *channel, \
                              int datatype, void *data, size_t length, void *callback):
  cdef DataChannel dc = DataChannel()
  dc._channel = channel
  buf = <char*>data
  (<object>callback)(dc, datatype, buf[:length])

cdef class PeerConnection:
  cdef rtcdc_peer_connection *_peer

  def __cinit__(self, on_channel, stun_server='', stun_port=0):
    if stun_server is None:
      stun_server = ''
    self._peer = crtcdc.rtcdc_create_peer_connection(on_channel_callback, stun_server, stun_port, <void*>on_channel)

  def __dealloc__(self):
    if self._peer is not NULL:
      crtcdc.rtcdc_destroy_peer_connection(self._peer)

  def generate_offer(self):
    return crtcdc.rtcdc_generate_offer_sdp(self._peer)

  def parse_offer(self, char *offer):
    return crtcdc.rtcdc_parse_offer_sdp(self._peer, offer)

  def generate_candidates(self):
    return crtcdc.rtcdc_generate_candidate_sdp(self._peer)

  def parse_candidates(self, char *candidates):
    return crtcdc.rtcdc_parse_candidate_sdp(self._peer, candidates)

  def create_data_channel(self, char *label, char *protocol, on_message):
    cdef DataChannel dc = DataChannel()
    dc._channel = crtcdc.rtcdc_create_data_channel(self._peer, label, protocol, \
                                                   on_message_callback, <void*>on_message)
    return dc

  @property
  def stun_server(self):
    return self._peer.stun_server

  @property
  def stun_port(self):
    return self._peer.stun_port

  def loop(self):
    crtcdc.rtcdc_loop(self._peer)

cdef class DataChannel:
  cdef rtcdc_data_channel *_channel

  def send_message(self, int datatype, char *data, size_t length):
    if self._channel is NULL:
      return -1
    return crtcdc.rtcdc_send_message(self._channel, datatype, data, length)

  @property
  def label(self):
    return self._channel.label

  @property
  def protocol(self):
    return self._channel.protocol

  def set_callback(self, on_message):
    self._channel.on_message = on_message_callback
    self._channel.user_data = <void*>on_message
