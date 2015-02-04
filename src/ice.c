// ice.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "sctp.h"
#include "ice.h"

static void
candidate_gathering_done_cb(NiceAgent *agent, guint stream_id, gpointer user_data)
{
  struct ice_transport *ice = (struct ice_transport *)user_data;
  ice->gathering_done = TRUE;
}

static void
component_state_changed_cb(NiceAgent *agent, guint stream_id, 
  guint component_id, guint state, gpointer user_data)
{
  struct ice_transport *ice = (struct ice_transport *)user_data;
  if (state == NICE_COMPONENT_STATE_READY) {
    ice->negotiation_done = TRUE;
  } else if (state == NICE_COMPONENT_STATE_FAILED) {
    g_main_loop_quit(ice->loop);
    ice->exit_thread = TRUE;
  }
}

static void
data_received_cb(NiceAgent *agent, guint stream_id, guint component_id,
  guint len, gchar *buf, gpointer user_data)
{
  struct ice_transport *ice = (struct ice_transport *)user_data;
  if (!ice->negotiation_done)
    return;

  struct dtls_transport *dtls = ice->dtls;
  struct sctp_transport *sctp = ice->sctp;

  g_mutex_lock(&dtls->dtls_mutex);
  BIO_write(dtls->incoming_bio, buf, len);
  g_mutex_unlock(&dtls->dtls_mutex);

  if (SSL_is_init_finished(dtls->ssl) != 1) {
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
create_ice_transport(struct dtls_transport *dtls, struct sctp_transport *sctp, int controlling)
{
  if (dtls == NULL || sctp == NULL)
    return NULL;

  struct ice_transport *ice = (struct ice_transport *)calloc(1, sizeof *ice);
  if (ice == NULL)
    return NULL;
  ice->dtls = dtls;
  ice->sctp = sctp;

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
    G_CALLBACK(candidate_gathering_done_cb), ice);
  g_signal_connect(G_OBJECT(agent), "component-state-changed",
    G_CALLBACK(component_state_changed_cb), ice);

  guint stream_id = nice_agent_add_stream(agent, 1);
  if (stream_id == 0)
    goto trans_err;
  ice->stream_id = stream_id;

  nice_agent_set_stream_name(agent, stream_id, "application");

  nice_agent_attach_recv(agent, stream_id, 1,
    g_main_loop_get_context(loop), data_received_cb, ice);

  if (!nice_agent_gather_candidates(agent, stream_id))
    goto trans_err;

  if (0) {
trans_err:
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
  struct ice_transport *ice = (struct ice_transport *)user_data;
  struct dtls_transport *dtls = ice->dtls;

  while (!ice->exit_thread && !ice->gathering_done)
    g_usleep(10000);
  if (ice->exit_thread)
    return NULL;

  while (!ice->exit_thread && !ice->negotiation_done)
    g_usleep(10000);
  if (ice->exit_thread)
    return NULL;

  if (dtls->role == PEER_CLIENT)
    SSL_do_handshake(dtls->ssl);

  char buf[BUFFER_SIZE];
  while (!ice->exit_thread) {
    if (BIO_ctrl_pending(dtls->outgoing_bio) > 0) {
      g_mutex_lock(&dtls->dtls_mutex);
      int nbytes = BIO_read(dtls->outgoing_bio, buf, sizeof buf);
      g_mutex_unlock(&dtls->dtls_mutex);

      if (nbytes > 0) {
        nice_agent_send(ice->agent, ice->stream_id, 1, nbytes, buf);
      }

      if (SSL_is_init_finished(dtls->ssl) != 1) {
        g_mutex_lock(&dtls->dtls_mutex);
        SSL_do_handshake(dtls->ssl);
        g_mutex_unlock(&dtls->dtls_mutex);
      }
    } else {
      g_usleep(5000);
    }
  }

  return NULL;
}
