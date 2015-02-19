// sctp.h
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#ifndef _RTCDC_SCTP_H_
#define _RTCDC_SCTP_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <usrsctp.h>
#include <openssl/bio.h>
#include <glib.h>

struct data_channel;

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
  int role;
  gboolean handshake_done;
  GAsyncQueue *deferred_messages;
  GMutex sctp_mutex;
#ifdef DEBUG_SCTP
  int incoming_stub;
  int outgoing_stub;
#endif
  struct data_channel **channels;
  int channel_num;
  int stream_cursor;
  void (*on_channel)(struct data_channel *ch);
};

struct sctp_transport *
create_sctp_transport(int lport, int rport,
                      void (*)(struct data_channel *ch));

void
destroy_sctp_transport(struct sctp_transport *sctp);

gpointer
sctp_thread(gpointer ice_trans);

gpointer
sctp_startup_thread(gpointer ice_trans);

int
send_sctp_message(struct sctp_transport *sctp,
                  void *data, size_t len, uint16_t sid, uint32_t ppid);

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_SCTP_H_
