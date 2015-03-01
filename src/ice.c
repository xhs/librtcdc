// ice.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "util.h"
#include "dtls.h"
#include "sctp.h"
#include "ice.h"
#include "rtcdc.h"

static void
candidate_gathering_done_cb(NiceAgent *agent, guint stream_id, gpointer user_data)
{
  struct rtcdc_peer_connection *peer = (struct rtcdc_peer_connection *)user_data;
  struct rtcdc_transport *transport = peer->transport;
  struct ice_transport *ice = transport->ice;
  ice->gathering_done = TRUE;
}

static void
component_state_changed_cb(NiceAgent *agent, guint stream_id, 
  guint component_id, guint state, gpointer user_data)
{
  struct rtcdc_peer_connection *peer = (struct rtcdc_peer_connection *)user_data;
  struct rtcdc_transport *transport = peer->transport;
  struct ice_transport *ice = transport->ice;
  if (state == NICE_COMPONENT_STATE_READY) {
    ice->negotiation_done = TRUE;
  } else if (state == NICE_COMPONENT_STATE_FAILED) {
    g_main_loop_quit(ice->loop);
    peer->exit_thread = TRUE;
  }
}

static void
data_received_cb(NiceAgent *agent, guint stream_id, guint component_id,
  guint len, gchar *buf, gpointer user_data)
{
  struct rtcdc_peer_connection *peer = (struct rtcdc_peer_connection *)user_data;
  struct rtcdc_transport *transport = peer->transport;
  struct ice_transport *ice = transport->ice;
  struct dtls_transport *dtls = transport->dtls;
  struct sctp_transport *sctp = transport->sctp;
  if (!ice->negotiation_done)
    return;

  g_mutex_lock(&dtls->dtls_mutex);
  BIO_write(dtls->incoming_bio, buf, len);
  g_mutex_unlock(&dtls->dtls_mutex);

  if (!dtls->handshake_done && SSL_is_init_finished(dtls->ssl))
    dtls->handshake_done = TRUE;

  if (!dtls->handshake_done) {
    g_mutex_lock(&dtls->dtls_mutex);
    SSL_do_handshake(dtls->ssl);
    g_mutex_unlock(&dtls->dtls_mutex);
  } else {
    unsigned char buf[BUFFER_SIZE];
    int nbytes = SSL_read(dtls->ssl, buf, sizeof buf);
    if (nbytes > 0) {
      g_mutex_lock(&sctp->sctp_mutex);
      BIO_write(sctp->incoming_bio, buf, nbytes);
      g_mutex_unlock(&sctp->sctp_mutex);
    }
  }
}

struct ice_transport *
create_ice_transport(struct rtcdc_peer_connection *peer, int controlling)
{
  if (peer == NULL || peer->transport == NULL)
    return NULL;

  struct ice_transport *ice = (struct ice_transport *)calloc(1, sizeof *ice);
  if (ice == NULL)
    return NULL;
  peer->transport->ice = ice;

  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  if (loop == NULL) {
    free(ice);
    return NULL;
  }
  ice->loop = loop;

  NiceAgent *agent = nice_agent_new(g_main_loop_get_context(loop),
    NICE_COMPATIBILITY_RFC5245);
  if (agent == NULL)
    goto trans_err;
  ice->agent = agent;

  g_object_set(G_OBJECT(agent), "controlling-mode", controlling, NULL);

  g_signal_connect(G_OBJECT(agent), "candidate-gathering-done",
    G_CALLBACK(candidate_gathering_done_cb), peer);
  g_signal_connect(G_OBJECT(agent), "component-state-changed",
    G_CALLBACK(component_state_changed_cb), peer);

  guint stream_id = nice_agent_add_stream(agent, 1);
  if (stream_id == 0)
    goto trans_err;
  ice->stream_id = stream_id;

  nice_agent_set_stream_name(agent, stream_id, "application");

  nice_agent_attach_recv(agent, stream_id, 1,
    g_main_loop_get_context(loop), data_received_cb, peer);

  if (!nice_agent_gather_candidates(agent, stream_id))
    goto trans_err;

  if (0) {
trans_err:
    peer->transport->ice = NULL;
    g_object_unref(agent);
    g_main_loop_unref(loop);
    free(ice);
    ice = NULL;
  }

  return ice;
}

void
destroy_ice_transport(struct ice_transport *ice)
{
  if (ice == NULL)
    return;

  g_object_unref(ice->agent);
  g_main_loop_unref(ice->loop);
  free(ice);
  ice = NULL;
}

gpointer
ice_thread(gpointer user_data)
{
  struct rtcdc_peer_connection *peer = (struct rtcdc_peer_connection *)user_data;
  struct rtcdc_transport *transport = peer->transport;
  struct ice_transport *ice = transport->ice;
  struct dtls_transport *dtls = transport->dtls;

  while (!peer->exit_thread && !ice->gathering_done)
    g_usleep(2000);
  if (peer->exit_thread)
    return NULL;

  while (!peer->exit_thread && !ice->negotiation_done)
    g_usleep(2000);
  if (peer->exit_thread)
    return NULL;

  if (transport->role == RTCDC_PEER_ROLE_CLIENT) {
    // ugly
    g_usleep(500000);
    SSL_do_handshake(dtls->ssl);
  }

  // need a external thread to start SCTP when DTLS handshake is done
  char buf[BUFFER_SIZE];
  while (!peer->exit_thread) {
    if (BIO_ctrl_pending(dtls->outgoing_bio) > 0) {
      g_mutex_lock(&dtls->dtls_mutex);
      int nbytes = BIO_read(dtls->outgoing_bio, buf, sizeof buf);
      g_mutex_unlock(&dtls->dtls_mutex);

      if (nbytes > 0) {
        nice_agent_send(ice->agent, ice->stream_id, 1, nbytes, buf);
      }

      if (!dtls->handshake_done) {
        g_mutex_lock(&dtls->dtls_mutex);
        SSL_do_handshake(dtls->ssl);
        g_mutex_unlock(&dtls->dtls_mutex);
      }
    } else {
      g_usleep(2000);
    }
  }

  return NULL;
}
