// ice.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a BSD license.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "util.h"
#include "dtls.h"
#include "sctp.h"
#include "ice.h"
#include "rtcdc.h"
#include "log.h"

static void
candidate_gathering_done_cb(NiceAgent *agent, guint stream_id, gpointer user_data)
{
  struct rtcdc_peer_connection *peer = (struct rtcdc_peer_connection *)user_data;
  struct rtcdc_transport *transport = peer->transport;
  struct ice_transport *ice = transport->ice;
  ice->gathering_done = TRUE;
  if (peer->on_candidate) {
    peer->on_candidate(peer, "", peer->user_data);
  }
}

static void
component_state_changed_cb(NiceAgent *agent, guint stream_id,
  guint component_id, guint state, gpointer user_data)
{
  //...
}

static void
new_local_candidate_cb(NiceAgent *agent, NiceCandidate *candidate, gpointer user_data)
{
  struct rtcdc_peer_connection *peer = (struct rtcdc_peer_connection *)user_data;
  if (peer->on_candidate) {
    gchar *cand = nice_agent_generate_local_candidate_sdp(agent, candidate);
    peer->on_candidate(peer, cand, peer->user_data);
    g_free(cand);
  }
}

static void
new_selected_pair_cb(NiceAgent *agent, guint stream_id, guint component_id,
                     NiceCandidate *lcandidate, NiceCandidate *rcandidate,
                     gpointer user_data)
{
  struct rtcdc_peer_connection *peer = (struct rtcdc_peer_connection *)user_data;
  struct rtcdc_transport *transport = peer->transport;
  struct ice_transport *ice = transport->ice;
  ice->negotiation_done = TRUE;
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
  unsigned char read_buf[BUFFER_SIZE];

  if (!ice->negotiation_done) {
    log_msg("Received data before ice negotiation complete\n");
    return;
  }

  log_msg("Nice agent received message of size: %d\n", len);

  g_mutex_lock(&dtls->dtls_mutex);
  BIO_write(dtls->incoming_bio, buf, len);
  int nbytes = SSL_read(dtls->ssl, read_buf, sizeof(read_buf));
  if (!dtls->handshake_done) {
    if (SSL_is_init_finished(dtls->ssl)) {
      dtls->handshake_done = TRUE;
      log_msg("dtls handshake is done\n");
    } else if (BIO_ctrl_pending(dtls->outgoing_bio)) {
      char out_buf[BUFFER_SIZE];
      while (BIO_ctrl_pending(dtls->outgoing_bio) > 0) {
        int nbytes = BIO_read(dtls->outgoing_bio, out_buf, sizeof(out_buf));
        if (nbytes > 0) {
          g_queue_push_tail(dtls->outgoing_queue, create_sctp_message(out_buf, nbytes));
        }
      }
    }
  }
  g_mutex_unlock(&dtls->dtls_mutex);

  if (nbytes > 0) {
    g_mutex_lock(&sctp->sctp_mutex);
    log_msg("Sending decrypted message of size %d to sctp\n", nbytes);
    g_queue_push_tail(sctp->in_messages, create_sctp_message(read_buf, nbytes));
    g_mutex_unlock(&sctp->sctp_mutex);
  } else {
    log_msg("No data received from DTLS for message: %d\n", len);
  }
}

struct ice_transport *
create_ice_transport(struct rtcdc_peer_connection *peer,
                     const char *stun_server, uint16_t stun_port)
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

  // change the role automatically when detecting role conflict
  g_object_set(G_OBJECT(agent), "controlling-mode", 1, NULL);
  if (stun_server != NULL && strcmp(stun_server, "") != 0)
    g_object_set(G_OBJECT(agent), "stun-server", stun_server, NULL);
  if (stun_port > 0)
    g_object_set(G_OBJECT(agent), "stun-server-port", stun_port, NULL);

  g_signal_connect(G_OBJECT(agent), "candidate-gathering-done",
    G_CALLBACK(candidate_gathering_done_cb), peer);
  g_signal_connect(G_OBJECT(agent), "component-state-changed",
    G_CALLBACK(component_state_changed_cb), peer);
  g_signal_connect(G_OBJECT(agent), "new-candidate-full",
    G_CALLBACK(new_local_candidate_cb), peer);
  g_signal_connect(G_OBJECT(agent), "new-selected-pair-full",
    G_CALLBACK(new_selected_pair_cb), peer);

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
    g_usleep(2500);
  if (peer->exit_thread)
    return NULL;

  while (!peer->exit_thread && !ice->negotiation_done)
    g_usleep(2500);
  if (peer->exit_thread)
    return NULL;

  char buf[BUFFER_SIZE];

  // need an external thread to start SCTP when DTLS handshake is done
  while (!peer->exit_thread) {
    g_mutex_lock(&dtls->dtls_mutex);

    if (!dtls->handshake_done) {
      if (SSL_is_init_finished(dtls->ssl)) {
        dtls->handshake_done = TRUE;
      }
    }

    while (!g_queue_is_empty(dtls->outgoing_queue)) {
      struct sctp_message *msg = g_queue_pop_head(dtls->outgoing_queue);
      log_msg("Nice agent sending msg of size: %d\n", msg->len);
      nice_agent_send(ice->agent, ice->stream_id, 1, msg->len, msg->data);
      free(msg->data);
      free(msg);
    }
    g_mutex_unlock(&dtls->dtls_mutex);

    g_usleep(500);
  }

  return NULL;
}
