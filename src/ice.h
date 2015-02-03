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

struct ice_transport {
  NiceAgent *agent;
  gboolean gathering_done;
  GMutex gather_mutex;
  gboolean negotiation_done;
  GMutex negotiate_mutex;
};

struct ice_transport *
create_ice_transport(void);

void
destroy_ice_transport(struct ice_transport *ice);

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_ICE_H_
