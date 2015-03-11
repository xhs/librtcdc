// sctp.h
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a BSD license.

#ifndef _RTCDC_SCTP_H_
#define _RTCDC_SCTP_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <usrsctp.h>
#include <openssl/bio.h>
#include <glib.h>

struct rtcdc_peer_connection;

struct sctp_message {
  void *data;
  size_t len;
  uint16_t sid;
  uint32_t ppid;
};

struct sctp_transport {
  struct socket *sock;
  BIO *incoming_bio;
  BIO *outgoing_bio;
  int local_port;
  int remote_port;
  gboolean handshake_done;
  GAsyncQueue *deferred_messages;
  GMutex sctp_mutex;
#ifdef DEBUG_SCTP
  int incoming_stub;
  int outgoing_stub;
#endif
  int stream_cursor;
  void *user_data;
};

struct sctp_transport *
create_sctp_transport(struct rtcdc_peer_connection *peer);

void
destroy_sctp_transport(struct sctp_transport *sctp);

int
send_sctp_message(struct sctp_transport *sctp,
                  void *data, size_t len, uint16_t sid, uint32_t ppid);

gpointer
sctp_thread(gpointer peer);

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_SCTP_H_
