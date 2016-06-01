// rtcdc.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a BSD license.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <glib.h>
#include "ice.h"
#include "dtls.h"
#include "sctp.h"
#include "sdp.h"
#include "dcep.h"
#include "rtcdc.h"
#include "common.h"
#include "log.h"

static struct dtls_context *g_dtls_context = NULL;
static int g_context_ref = 0;

static int
create_rtcdc_transport(struct rtcdc_peer_connection *peer, int role)
{
  if (peer == NULL) {
    log_msg("Peer is null\n");
    return -1;
  }

  struct rtcdc_transport *transport =
    (struct rtcdc_transport *)calloc(1, sizeof *transport);
  if (transport == NULL) {
    log_msg("calloc fail\n");
    return -1;
  }
  peer->transport = transport;
  peer->role = role;

  if (g_dtls_context == NULL) {
    g_dtls_context = create_dtls_context("librtcdc");
    if (g_dtls_context == NULL) {
      log_msg("dtls context failure\n");
      goto ctx_null_err;
    }
  }
  transport->ctx = g_dtls_context;
  g_context_ref++;

  struct dtls_transport *dtls = create_dtls_transport(peer, transport->ctx);
  if (dtls == NULL) {
    log_msg("dtls null\n");
    goto dtls_null_err;
  }

  struct sctp_transport *sctp = create_sctp_transport(peer);
  if (sctp == NULL) {
    log_msg("sctp transport NULL\n");
    goto sctp_null_err;
  }

  struct ice_transport *ice = create_ice_transport(peer, peer->stun_server, peer->stun_port);
  if (ice == NULL) {
    log_msg("ice null\n");
    goto ice_null_err;
  }

  peer->initialized = TRUE;

  if (0) {
ice_null_err:
    destroy_sctp_transport(sctp);
sctp_null_err:
    destroy_dtls_transport(dtls);
dtls_null_err:
    if (--g_context_ref <= 0) {
      destroy_dtls_context(g_dtls_context);
      g_dtls_context = NULL;
      g_context_ref = 0;
    }
ctx_null_err:
    peer->transport = NULL;
    free(transport);
    return -1;
  }

  return 0;
}

static void
destroy_rtcdc_transport(struct rtcdc_transport *transport)
{
  if (transport == NULL)
    return;

  if (transport->ice)
    destroy_ice_transport(transport->ice);
  if (transport->dtls)
    destroy_dtls_transport(transport->dtls);
  if (transport->sctp)
    destroy_sctp_transport(transport->sctp);

  if (--g_context_ref <= 0) {
    destroy_dtls_context(g_dtls_context);
    g_dtls_context = NULL;
    g_context_ref = 0;
  }

  free(transport);
  transport = NULL;
}

struct rtcdc_peer_connection *
rtcdc_create_peer_connection(rtcdc_on_channel_cb on_channel,
                             rtcdc_on_candidate_cb on_candidate,
                             rtcdc_on_connect_cb on_connect,
                             const char *stun_server, uint16_t stun_port,
                             void *user_data)
{
  char buf[INET_ADDRSTRLEN];
  if (stun_server != NULL && strcmp(stun_server, "") != 0) {
    memset(buf, 0, sizeof buf);
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;

    if ((getaddrinfo(stun_server, NULL, &hints, &servinfo)) != 0)
      return NULL;

    struct sockaddr_in *sa = (struct sockaddr_in *)servinfo->ai_addr;
    inet_ntop(AF_INET, &(sa->sin_addr), buf, INET_ADDRSTRLEN);
    freeaddrinfo(servinfo);
  }

  struct rtcdc_peer_connection *peer =
    (struct rtcdc_peer_connection *)calloc(1, sizeof *peer);
  if (peer == NULL)
    return NULL;
  if (stun_server)
    peer->stun_server = strdup(buf);
  peer->stun_port = stun_port > 0 ? stun_port : 3478;
  peer->on_channel = on_channel;
  peer->on_candidate = on_candidate;
  peer->on_connect = on_connect;
  peer->user_data = user_data;

  return peer;
}

void
rtcdc_destroy_peer_connection(struct rtcdc_peer_connection *peer)
{
  if (peer == NULL)
    return;

  if (peer->transport) {
    g_main_loop_quit(peer->transport->ice->loop);
    g_usleep(100000);
    destroy_rtcdc_transport(peer->transport);
  }

  if (peer->stun_server)
    free(peer->stun_server);

  if (peer->channels) {
    for (int i = 0; i < RTCDC_MAX_CHANNEL_NUM; ++i) {
      rtcdc_destroy_data_channel(peer->channels[i]);
    }
  }

  free(peer);
  peer = NULL;
}

char *
rtcdc_generate_offer_sdp(struct rtcdc_peer_connection *peer)
{
  if (peer == NULL) {
    log_msg("Peer is null?\n");
    return NULL;
  }

  if (peer->transport == NULL) {
    if (create_rtcdc_transport(peer, RTCDC_PEER_ROLE_CLIENT) < 0) {
      log_msg("Failed to create rtcdc_transport\n");
      return NULL;
    }
  }
  int client = peer->role == RTCDC_PEER_ROLE_CLIENT ? 1 : 0;
  return generate_local_sdp(peer->transport, peer->enable_draft_8, client, peer->have_offer);
}

char *
rtcdc_generate_local_candidate_sdp(struct rtcdc_peer_connection *peer)
{
  if (peer == NULL)
    return NULL;

  if (peer->transport == NULL) {
    if (create_rtcdc_transport(peer, RTCDC_PEER_ROLE_CLIENT) < 0)
      return NULL;
  }
  return generate_local_candidate_sdp(peer->transport);
}

int
rtcdc_parse_offer_sdp(struct rtcdc_peer_connection *peer, const char *offer)
{
  if (peer == NULL || offer == NULL)
    return -1;

  if (peer->transport == NULL) {
    if (create_rtcdc_transport(peer, RTCDC_PEER_ROLE_CLIENT) < 0)
      return -1;
  }

  char **lines;
  lines = g_strsplit(offer, "\r\n", 0);

  char buf[BUFFER_SIZE];
  memset(buf, 0, sizeof buf);
  int pos = 0;
  int remote_port = 0;
  for (int i = 0; lines && lines[i]; ++i) {
    if (g_str_has_prefix(lines[i], "a=sctp-port:")) {
      char **columns = g_strsplit(lines[i], ":", 0);
      remote_port = atoi(columns[1]);
      if (remote_port <= 0)
        return -1;
      peer->transport->sctp->remote_port = remote_port;
      g_strfreev(columns);
      peer->enable_draft_8 = TRUE;
#ifdef DEBUG_SCTP
      log_msg("New SDP detected, switching to draft-8 mode\n");
#endif
    } else if (g_str_has_prefix(lines[i], "a=setup:")) {
      char **columns = g_strsplit(lines[i], ":", 0);
      if (strcmp(columns[1], "active") == 0 && peer->role == RTCDC_PEER_ROLE_CLIENT) {
        peer->role = RTCDC_PEER_ROLE_SERVER;
      } else if (strcmp(columns[1], "passive") == 0 && peer->role == RTCDC_PEER_ROLE_SERVER) {
        peer->role = RTCDC_PEER_ROLE_CLIENT;
      } else { // actpass
        // nothing to do
      }
      g_strfreev(columns);
    } else if (g_str_has_prefix(lines[i], "a=sctpmap:")) {
        peer->enable_draft_8 = FALSE;
#ifdef DEBUG_SCTP
        log_msg("Old SDP detected, switching to non draft-8 mode\n");
#endif
        char **columns = g_strsplit(lines[i], " ", 0);
        char **cols2 = g_strsplit(columns[0], ":", 0);
        remote_port = atoi(cols2[1]);
        if (remote_port <= 0)
            return -1;

        peer->transport->sctp->remote_port = remote_port;
        g_strfreev(cols2);
        g_strfreev(columns);
    }
    pos += sprintf(buf + pos, "%s\n", lines[i]);
  }
  g_strfreev(lines);

  int res = parse_remote_sdp(peer->transport->ice, buf);
  if (res >= 0)
    peer->have_offer = TRUE;

  return res;
}

int
rtcdc_parse_candidate_sdp(struct rtcdc_peer_connection *peer, const char *candidates)
{
  if (peer == NULL || peer->transport == NULL)
    return -1;

  return parse_remote_candidate_sdp(peer->transport->ice, candidates);
}

struct rtcdc_data_channel *
rtcdc_create_data_channel(struct rtcdc_peer_connection *peer,
                          const char *label, const char *protocol,
                          rtcdc_on_open_cb on_open,
                          rtcdc_on_message_cb on_message,
                          rtcdc_on_close_cb on_close,
                          void *user_data)
{
  if (peer == NULL || peer->transport == NULL || peer->channels == NULL)
    return NULL;

  struct rtcdc_transport *transport = peer->transport;
  struct sctp_transport *sctp = transport->sctp;

  int i;
  for (i = 0; i < RTCDC_MAX_CHANNEL_NUM; ++i) {
    if (peer->channels[i])
      continue;
    break;
  }

  if (i == RTCDC_MAX_CHANNEL_NUM)
    return NULL;

  struct rtcdc_data_channel *ch = (struct rtcdc_data_channel *)calloc(1, sizeof *ch);
  if (ch == NULL)
    return NULL;
  ch->on_open = on_open;
  ch->on_message = on_message;
  ch->on_close = on_close;
  ch->user_data = user_data;
  ch->sctp = sctp;

  struct dcep_open_message *req;
  int rlen = sizeof *req + strlen(label) + strlen(protocol);
  req = (struct dcep_open_message *)calloc(1, rlen);
  if (req == NULL)
    goto open_channel_err;

  ch->type = DATA_CHANNEL_RELIABLE;
  ch->state = RTCDC_CHANNEL_STATE_CONNECTING;
  if (label)
    ch->label = strdup(label);
  if (protocol)
    ch->protocol = strdup(protocol);
  ch->sid = sctp->stream_cursor;
  sctp->stream_cursor += 2;

  req->message_type = DATA_CHANNEL_OPEN;
  req->channel_type = ch->type;
  req->priority = htons(0);
  req->reliability_param = htonl(0);
  if (label)
    req->label_length = htons(strlen(label));
  if (protocol)
    req->protocol_length = htons(strlen(protocol));
  memcpy(req->label_and_protocol, label, strlen(label));
  memcpy(req->label_and_protocol + strlen(label), protocol, strlen(protocol));

  int ret = send_sctp_message(sctp, req, rlen, ch->sid, WEBRTC_CONTROL_PPID);
  free(req);
  if (ret < 0)
    goto open_channel_err;

  if (0) {
open_channel_err:
    free(ch);
    ch = NULL;
  }

  peer->channels[i] = ch;
  return ch;
}

void
rtcdc_destroy_data_channel(struct rtcdc_data_channel *channel)
{
  if (channel == NULL)
    return;

  // todo: close channel
  if (channel->label)
    free(channel->label);
  if (channel->protocol)
    free(channel->protocol);
}

int
rtcdc_send_message(struct rtcdc_data_channel *channel, int datatype, void *data, size_t len)
{
  if (channel == NULL) {
    log_msg("NULL channel wtf?\n");
    return -1;
  }

  int ppid;
  if (datatype == RTCDC_DATATYPE_STRING) {
    if (data == NULL || len == 0)
      ppid = WEBRTC_STRING_EMPTY_PPID;
    else
      ppid = WEBRTC_STRING_PPID;
  } else if (datatype == RTCDC_DATATYPE_BINARY) {
    if (data == NULL || len == 0)
      ppid = WEBRTC_BINARY_EMPTY_PPID;
    else
      ppid = WEBRTC_BINARY_PPID;
  } else
    return -1;

  return send_sctp_message(channel->sctp, data, len, channel->sid, ppid);
}

static gpointer
startup_thread(gpointer user_data)
{
  struct rtcdc_peer_connection *peer = (struct rtcdc_peer_connection *)user_data;
  struct rtcdc_transport *transport = peer->transport;
  struct ice_transport *ice = transport->ice;
  struct dtls_transport *dtls = transport->dtls;
  struct sctp_transport *sctp = transport->sctp;

  while (!peer->exit_thread && !ice->negotiation_done)
    g_usleep(500);
  if (peer->exit_thread)
    return NULL;

#ifdef DEBUG_SCTP
  log_msg("ICE negotiation done\n");
#endif

  g_mutex_lock(&dtls->dtls_mutex);
  if (peer->role == RTCDC_PEER_ROLE_CLIENT) {
    log_msg("Client connect state\n");
    SSL_set_connect_state(dtls->ssl);
  } else {
    log_msg("Server accept state\n");
    SSL_set_accept_state(dtls->ssl);
  }

  char buf[BUFFER_SIZE];
  log_msg("SSL do handshake\n");
  SSL_do_handshake(dtls->ssl);
  while (BIO_ctrl_pending(dtls->outgoing_bio) > 0) {
    int nbytes = BIO_read(dtls->outgoing_bio, buf, sizeof buf);
    if (nbytes > 0) {
      g_queue_push_tail(dtls->outgoing_queue, create_sctp_message(buf, nbytes));
    }
  }
  g_mutex_unlock(&dtls->dtls_mutex);

  while (!peer->exit_thread && !dtls->handshake_done)
    g_usleep(500);
  if (peer->exit_thread)
    return NULL;

#ifdef DEBUG_SCTP
  log_msg("DTLS handshake done\n");
#endif

  if (peer->role == RTCDC_PEER_ROLE_CLIENT) {
    sctp->stream_cursor = 0; // use even streams
    struct sockaddr_conn sconn;
    memset(&sconn, 0, sizeof sconn);
    sconn.sconn_family = AF_CONN;
    sconn.sconn_port = htons(sctp->remote_port);
    sconn.sconn_addr = (void *)sctp;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    sconn.sconn_len = sizeof *sctp;
#endif
    log_msg("usrsctp_connect\n");
    int connect_result = usrsctp_connect(sctp->sock, (struct sockaddr *)&sconn, sizeof sconn);
    if ((connect_result < 0) && (errno != EINPROGRESS)) {
      log_msg("SCTP connection failed with errnor: %ld\n", errno);
    } else {
#ifdef DEBUG_SCTP
      log_msg("SCTP connected\n");
#endif
      sctp->handshake_done = TRUE;

      if (peer->on_connect)
        peer->on_connect(peer, peer->user_data);
    }
  } else {
    sctp->stream_cursor = 1; // use odd streams
    struct sockaddr_conn sconn;
    memset(&sconn, 0, sizeof sconn);
    sconn.sconn_family = AF_CONN;
    sconn.sconn_port = htons(sctp->remote_port);
    sconn.sconn_addr = (void *)sctp;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    sconn.sconn_len = sizeof *sctp;
#endif
    usrsctp_listen(sctp->sock, 1);
    socklen_t len = sizeof sconn;
    struct socket *s = usrsctp_accept(sctp->sock, (struct sockaddr *)&sconn, &len);
    if (s) {
#ifdef DEBUG_SCTP
      log_msg("SCTP accepted\n");
#endif
      sctp->handshake_done = TRUE;
      struct socket *t = sctp->sock;
      sctp->sock = s;
      usrsctp_close(t);

      if (peer->on_connect)
        peer->on_connect(peer, peer->user_data);
    } else {
#ifdef DEBUG_SCTP
      log_msg("SCTP acception failed\n");
#endif
    }
  }

  return NULL;
}

void
rtcdc_loop(struct rtcdc_peer_connection *peer)
{
  if (peer == NULL)
    return;

  while (!peer->initialized)
    g_usleep(500);

  GThread *thread_ice = g_thread_new("ICE thread", &ice_thread, peer);
  GThread *thread_sctp = g_thread_new("SCTP thread", &sctp_thread, peer);
  GThread *thread_startup = g_thread_new("Startup thread", &startup_thread, peer);

  struct ice_transport *ice = peer->transport->ice;
  g_main_loop_run(ice->loop);
  peer->exit_thread = TRUE;

  g_thread_join(thread_ice);
  g_thread_join(thread_sctp);
  g_thread_join(thread_startup);

  g_thread_unref(thread_ice);
  g_thread_unref(thread_sctp);
  g_thread_unref(thread_startup);
}
