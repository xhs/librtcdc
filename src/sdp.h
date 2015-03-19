// sdp.h
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a BSD license.

#ifndef _RTCDC_SDP_H_
#define _RTCDC_SDP_H_

#ifdef  __cplusplus
extern "C" {
#endif

struct rtcdc_transport;
struct ice_transport;

char *
generate_local_sdp(struct rtcdc_transport *transport, int client);

char *
generate_local_candidate_sdp(struct rtcdc_transport *transport);

int
parse_remote_sdp(struct ice_transport *ice, const char *rsdp);

int
parse_remote_candidate_sdp(struct ice_transport *ice, const char *candidates);

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_SDP_H_
