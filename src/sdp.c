// sdp.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a BSD license.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <agent.h>
#include "common.h"
#include "util.h"
#include "sctp.h"
#include "dtls.h"
#include "ice.h"
#include "rtcdc.h"
#include "sdp.h"

char *
generate_local_sdp(struct rtcdc_transport *transport, int client)
{
  if (transport == NULL)
    return NULL;

  struct ice_transport *ice = transport->ice;
  struct sctp_transport *sctp = transport->sctp;
  struct dtls_context *ctx = transport->ctx;

  char buf[BUFFER_SIZE];
  memset(buf, 0, sizeof buf);

  char sessid[SESSION_ID_SIZE + 1];
  memset(sessid, 0, sizeof sessid);
  random_number_string(sessid, SESSION_ID_SIZE);

  int pos = sprintf(buf, "v=0\r\n");
  pos += sprintf(buf + pos, "o=- %s 2 IN IP4 127.0.0.1\r\n", sessid);
  pos += sprintf(buf + pos, "%s",
    "s=-\r\n"
    "t=0 0\r\n"
    "a=msid-semantic: WMS\r\n");
  pos += sprintf(buf + pos, "m=application 1 DTLS/SCTP %d\r\n", sctp->local_port);
  pos += sprintf(buf + pos, "c=IN IP4 0.0.0.0\r\n");

  gchar *lsdp = nice_agent_generate_local_sdp(ice->agent);
  gchar **lines = g_strsplit(lsdp, "\n", 0);
  g_free(lsdp);
  for (int i = 0; lines && lines[i]; ++i) {
    if (g_str_has_prefix(lines[i], "a=ice-ufrag:")
        || g_str_has_prefix(lines[i], "a=ice-pwd:")) {
      pos += sprintf(buf + pos, "%s\r\n", lines[i]);
    }
  }
  g_strfreev(lines);

  pos += sprintf(buf + pos, "a=fingerprint:sha-256 %s\r\n", ctx->fingerprint);

  if (client)
    pos += sprintf(buf + pos, "a=setup:active\r\n");
  else
    pos += sprintf(buf + pos, "a=setup:passive\r\n");

  pos += sprintf(buf + pos, "a=mid:data\r\n");
  pos += sprintf(buf + pos, "a=sctpmap:%d webrtc-datachannel 1024\r\n", sctp->local_port);

  return strndup(buf, pos);
}

char *
generate_local_candidate_sdp(struct rtcdc_transport *transport)
{
  if (transport == NULL || transport->ice == NULL)
    return NULL;

  char buf[BUFFER_SIZE];
  memset(buf, 0, sizeof buf);

  gchar *lsdp = nice_agent_generate_local_sdp(transport->ice->agent);
  gchar **lines = g_strsplit(lsdp, "\n", 0);
  g_free(lsdp);
  int pos = 0;
  for (int i = 0; lines && lines[i]; ++i) {
    if (g_str_has_prefix(lines[i], "a=candidate:")) {
      pos += sprintf(buf + pos, "%s\r\n", lines[i]);
    }
  }
  g_strfreev(lines);

  return strndup(buf, pos);
}

int
parse_remote_sdp(struct ice_transport *ice, const char *rsdp)
{
  if (ice == NULL || ice->agent == NULL || rsdp == NULL)
    return -1;
  
  return nice_agent_parse_remote_sdp(ice->agent, rsdp);
}

int
parse_remote_candidate_sdp(struct ice_transport *ice, const char *candidates)
{
  if (ice == NULL || ice->agent == NULL || candidates == NULL)
    return -1;

  char **lines;
  lines = g_strsplit(candidates, "\r\n", 0);

  GSList *list = NULL;
  for (int i = 0; lines && lines[i]; ++i) {
    NiceCandidate *rcand = nice_agent_parse_remote_candidate_sdp(ice->agent, ice->stream_id, lines[i]);
    if (rcand == NULL)
      continue;
    list = g_slist_append(list, rcand);
  }
  g_strfreev(lines);

  int ret = nice_agent_set_remote_candidates(ice->agent, ice->stream_id, g_slist_length(list), list);
  g_slist_free_full(list, (GDestroyNotify)&nice_candidate_free);

  return ret;
}
