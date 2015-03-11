// ice.h
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a BSD license.

#ifndef _RTCDC_ICE_H_
#define _RTCDC_ICE_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <nice/agent.h>

struct rtcdc_peer_connection;

struct ice_transport {
  NiceAgent *agent;
  guint stream_id;
  GMainLoop *loop;
  gboolean gathering_done;
  gboolean negotiation_done;
};

struct ice_transport *
create_ice_transport(struct rtcdc_peer_connection *peer,
                     const char *stun_server, uint16_t stun_port);

void
destroy_ice_transport(struct ice_transport *ice);

gpointer
ice_thread(gpointer peer);

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_ICE_H_
