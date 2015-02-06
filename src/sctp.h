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

struct sctp_transport {
  struct socket *sock;
  BIO *incoming_bio;
  BIO *outgoing_bio;
  int local_port;
  int remote_port;
  int role;
  GMutex sctp_mutex;
#ifdef DEBUG_SCTP
  int incoming_stub;
  int outgoing_stub;
#endif
};

struct sctp_transport *
create_sctp_transport(int lport, int rport);

void
destroy_sctp_transport(struct sctp_transport *sctp);

gpointer
sctp_thread(gpointer ice_trans);

gpointer
sctp_startup_thread(gpointer ice_trans);

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_SCTP_H_
