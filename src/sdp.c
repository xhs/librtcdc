// sdp.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <agent.h>
#include "common.h"
#include "util.h"
#include "sctp.h"
#include "dtls.h"
#include "sdp.h"

char *
generate_local_sdp(struct ice_transport *ice, struct dtls_context *ctx, int client)
{
  if (ice == NULL || ice->agent == NULL || ice->sctp == NULL || ctx == NULL)
    return NULL;

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
  pos += sprintf(buf + pos, "m=application 1 DTLS/SCTP %d\r\n", ice->sctp->local_port);
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
  pos += sprintf(buf + pos, "a=sctpmap:%d webrtc-datachannel 1024\r\n", ice->sctp->local_port);

  return strndup(buf, pos);
}

char *
generate_local_candidate_sdp(struct ice_transport *ice)
{
  if (ice == NULL || ice->agent == NULL)
    return NULL;

  char buf[BUFFER_SIZE];
  memset(buf, 0, sizeof buf);

  gchar *lsdp = nice_agent_generate_local_sdp(ice->agent);
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

  gchar **lines;
  if (g_strstr_len("\r\n", strlen(rsdp), rsdp) == NULL)
    lines = g_strsplit(rsdp, "\n", 0);
  else
    lines = g_strsplit(rsdp, "\r\n", 0);

  char buf[BUFFER_SIZE];
  memset(buf, 0, sizeof buf);
  int pos = 0;
  for (int i = 0; lines && lines[i]; ++i) {
    if (g_str_has_prefix(lines[i], "m=application")) {
      gchar **columns = g_strsplit(lines[i], " ", 0);
      if (columns[0] && columns[1] && columns[2] && columns[3])
        ice->sctp->remote_port = atoi(columns[3]);
      g_strfreev(columns);
    }
    pos += sprintf(buf + pos, "%s\n", lines[i]);
  }
  g_strfreev(lines);

  if (ice->sctp->remote_port <= 0)
    return -1;

  return nice_agent_parse_remote_sdp(ice->agent, buf);
}

int
parse_remote_candidate_sdp(struct ice_transport *ice, const char *rcand_sdp)
{
  if (ice == NULL || ice->agent == NULL || rcand_sdp == NULL)
    return -1;

  NiceCandidate *rcand = nice_agent_parse_remote_candidate_sdp(ice->agent, ice->stream_id, rcand_sdp);
  if (rcand == NULL)
    return -1;

  GSList *list = NULL;
  list = g_slist_append(list, rcand);
  int ret = nice_agent_set_remote_candidates(ice->agent, ice->stream_id, 1, list);

  nice_candidate_free(rcand);
  g_slist_free(list);

  return ret;
}
