// ice.h
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#ifndef _RTCDC_ICE_H_
#define _RTCDC_ICE_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <agent.h>
#include <glib.h>
#include "util.h"
#include "dtls.h"

struct ice_transport {
  struct dtls_transport *dtls;
  struct sctp_transport *sctp;
  NiceAgent *agent;
  guint stream_id;
  GMainLoop *loop;
  gboolean exit_thread;
  gboolean gathering_done;
  gboolean negotiation_done;
};

struct ice_transport *
create_ice_transport(struct dtls_transport *dtls, struct sctp_transport *sctp, int controlling);

void
destroy_ice_transport(struct ice_transport *ice);

gpointer
ice_thread(gpointer ice_trans);

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_ICE_H_
