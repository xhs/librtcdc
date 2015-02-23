// sdp.h
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#ifndef _RTCDC_SDP_H_
#define _RTCDC_SDP_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include "dtls.h"
#include "ice.h"

char *
generate_local_sdp(struct ice_transport *ice, struct dtls_context *ctx, int client);

char *
generate_local_candidate_sdp(struct ice_transport *ice);

int
parse_remote_sdp(struct ice_transport *ice, const char *rsdp);

int
parse_remote_candidate_sdp(struct ice_transport *ice, const char *candidates);

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_SDP_H_
