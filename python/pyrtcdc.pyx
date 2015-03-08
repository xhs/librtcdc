# pyrtcdc.pyx
# Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
# This file is licensed under a BSD license.

cimport crtcdc
from crtcdc cimport rtcdc_peer_connection, rtcdc_data_channel, python_callbacks
from libc.stdlib cimport malloc, free

STATE_CLOSED     = 0
STATE_CONNECTING = 1
STATE_CONNECTED  = 2

DATATYPE_STRING = 0
DATATYPE_BINARY = 1
DATATYPE_EMPTY  = 2

cdef void on_channel_callback(rtcdc_data_channel *channel, void *callback):
  cdef DataChannel dc
  if callback is not NULL:
    on_channel = <object>callback
    if on_channel:
      dc = DataChannel()
      dc._channel = channel
      on_channel(dc)

cdef void on_open_callback(rtcdc_data_channel *channel, void *user_data):
  cdef DataChannel dc
  cdef python_callbacks *callbacks
  callbacks = <python_callbacks *>user_data
  if callbacks is not NULL:
    dc = DataChannel()
    dc._channel = channel
    if callbacks.on_open is not NULL:
      on_open = <object>callbacks.on_open
      if on_open:
        on_open(dc)

cdef void on_message_callback(rtcdc_data_channel *channel, \
                              int datatype, void *data, size_t length, void *user_data):
  cdef DataChannel dc
  cdef python_callbacks *callbacks
  callbacks = <python_callbacks *>user_data
  if callbacks is not NULL:
    dc = DataChannel()
    dc._channel = channel
    buf = <char*>data
    if callbacks.on_message is not NULL:
      on_message = <object>callbacks.on_message
      if on_message:
        on_message(dc, datatype, buf[:length])

cdef void on_close_callback(rtcdc_data_channel *channel, void *user_data):
  cdef DataChannel dc
  cdef python_callbacks *callbacks
  callbacks = <python_callbacks *>user_data
  if callbacks is not NULL:
    dc = DataChannel()
    dc._channel = channel
    if callbacks.on_close is not NULL:
      on_close = <object>callbacks.on_close
      if on_close:
        on_close(dc)

cdef python_callbacks *init_callbacks():
  cdef python_callbacks *callbacks
  callbacks = <python_callbacks *>malloc(3 * sizeof(void *))
  if callbacks is NULL:
    raise MemoryError()
  callbacks.on_open = NULL
  callbacks.on_message = NULL
  callbacks.on_close = NULL
  return callbacks

cdef class PeerConnection:
  cdef rtcdc_peer_connection *_peer

  def __cinit__(self, on_channel=None, stun_server='', stun_port=0):
    if stun_server is None:
      stun_server = ''
    self._peer = crtcdc.rtcdc_create_peer_connection(on_channel_callback, stun_server, stun_port, <void *>on_channel)
    if not self._peer:
      raise MemoryError()

  def __dealloc__(self):
    if self._peer is not NULL:
      crtcdc.rtcdc_destroy_peer_connection(self._peer)

  def destroy(self):
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

  def create_data_channel(self, char *label, char *protocol, on_open=None, on_message=None, on_close=None):
    cdef python_callbacks *callbacks
    callbacks = init_callbacks()
    callbacks.on_open = <void *>on_open
    callbacks.on_message = <void *>on_message
    callbacks.on_close = <void *>on_close
    cdef DataChannel dc = DataChannel()
    dc._channel = crtcdc.rtcdc_create_data_channel(self._peer, label, protocol, \
                                                   on_open_callback, on_message_callback, on_close_callback, \
                                                   <void *>callbacks)
    if dc._channel is NULL:
      raise MemoryError()
    return dc

  @property
  def stun_server(self):
    if self._peer.stun_server is NULL:
      return ''
    return self._peer.stun_server

  @property
  def stun_port(self):
    return self._peer.stun_port

  def loop(self):
    crtcdc.rtcdc_loop(self._peer)

  def __setattr__(self, name, value):
    if name is 'on_channel':
      self._peer.on_channel = on_channel_callback
      self._peer.user_data = <void *>value

cdef class DataChannel:
  cdef rtcdc_data_channel *_channel

  def send_message(self, int datatype, char *data):
    if self._channel is NULL:
      return -1
    return crtcdc.rtcdc_send_message(self._channel, datatype, data, len(data))

  @property
  def label(self):
    return self._channel.label

  @property
  def protocol(self):
    return self._channel.protocol

  def __setattr__(self, name, value):
    cdef python_callbacks *callbacks
    if self._channel.user_data is NULL:
        self._channel.user_data = <void *>init_callbacks()
    callbacks = <python_callbacks *>self._channel.user_data
    if name is 'on_open':
      self._channel.on_open = on_open_callback
      callbacks.on_open = <void *>value
    elif name is 'on_message':
      self._channel.on_message = on_message_callback
      callbacks.on_message = <void *>value
    elif name is 'on_close':
      self._channel.on_close = on_close_callback
      callbacks.on_close = <void *>value
