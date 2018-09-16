// sctp.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a BSD license.

#include "common.h"
#include "util.h"
#include "ice.h"
#include "dtls.h"
#include "sctp.h"
#include "dcep.h"
#include "rtcdc.h"

static int g_sctp_ref = 0;

static int interested_events[] = {
  //...
};

static int
sctp_data_ready_cb(void *reg_addr, void *data, size_t len, uint8_t tos, uint8_t set_df)
{
  struct sctp_transport *sctp = (struct sctp_transport *)reg_addr;
  g_mutex_lock(&sctp->sctp_mutex);
  if (sctp->outgoing_q != NULL)
    g_queue_push_tail(sctp->outgoing_q, create_sctp_message(data, len));
  else
    fprintf(stderr, "Tried to handle sctp message before queue initialization\n");
  g_mutex_unlock(&sctp->sctp_mutex);
  return 0;
}

static void
handle_notification_message(struct rtcdc_peer_connection *peer, union sctp_notification *notify, size_t len)
{
  //...
}

static int
sctp_data_received_cb(struct socket *sock, union sctp_sockstore addr, void *data,
                      size_t len, struct sctp_rcvinfo recv_info, int flags, void *peer_data)
{
  if (peer_data == NULL || len == 0)
    return -1;

  struct rtcdc_peer_connection *peer = (struct rtcdc_peer_connection *)peer_data;
  struct rtcdc_transport *transport = peer->transport;
  struct sctp_transport *sctp = transport->sctp;

#ifdef DEBUG_SCTP
  printf("data of length %zu received on stream %u with SSN %u, TSN %u, PPID %u\n",
         len,
         recv_info.rcv_sid,
         recv_info.rcv_ssn,
         recv_info.rcv_tsn,
         ntohl(recv_info.rcv_ppid));
#endif

  if (flags & MSG_NOTIFICATION)
    handle_notification_message(peer, (union sctp_notification *)data, len);
  else
    handle_rtcdc_message(peer, data, len, ntohl(recv_info.rcv_ppid), recv_info.rcv_sid);

  free(data);
  return 0;
}

struct sctp_transport *
create_sctp_transport(struct rtcdc_peer_connection *peer)
{
  if (peer == NULL || peer->transport == NULL)
    return NULL;

  struct sctp_transport *sctp = (struct sctp_transport *)calloc(1, sizeof *sctp);
  if (sctp == NULL)
    return NULL;
  peer->transport->sctp = sctp;
  sctp->local_port = 5000; // XXX: Hardcoded for now

  if (g_sctp_ref == 0) {
    usrsctp_init(0, sctp_data_ready_cb, NULL);
    usrsctp_sysctl_set_sctp_ecn_enable(0);
  }
  g_sctp_ref++;

  usrsctp_register_address(sctp);
  struct socket *s = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP,
                                    sctp_data_received_cb, NULL, 0, peer);
  if (s == NULL)
    goto trans_err;
  sctp->sock = s;

  sctp->incoming_q = g_queue_new();
  sctp->outgoing_q = g_queue_new();
  if ((sctp->incoming_q == NULL) || (sctp->outgoing_q == NULL))
    goto trans_err;

#ifdef DEBUG_SCTP
  int sd;
  struct sockaddr_in stub_addr;
  memset(&stub_addr, 0, sizeof stub_addr);
  inet_pton(AF_INET, "127.0.0.1", &stub_addr.sin_addr);
  stub_addr.sin_family = AF_INET;

  sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  stub_addr.sin_port = htons(60001);
  bind(sd, (const struct sockaddr *)&stub_addr, sizeof stub_addr);
  stub_addr.sin_port = htons(60002);
  connect(sd, (const struct sockaddr *)&stub_addr, sizeof stub_addr);
  sctp->incoming_stub = sd;

  sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  stub_addr.sin_port = htons(60002);
  bind(sd, (const struct sockaddr *)&stub_addr, sizeof stub_addr);
  stub_addr.sin_port = htons(60001);
  connect(sd, (const struct sockaddr *)&stub_addr, sizeof stub_addr);
  sctp->outgoing_stub = sd;
#endif

  struct linger lopt;
  lopt.l_onoff = 1;
  lopt.l_linger = 0;
  usrsctp_setsockopt(s, SOL_SOCKET, SO_LINGER, &lopt, sizeof lopt);

  struct sctp_paddrparams peer_param;
  memset(&peer_param, 0, sizeof peer_param);
  peer_param.spp_flags = SPP_PMTUD_DISABLE;
  peer_param.spp_pathmtu = 1200;
  usrsctp_setsockopt(s, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &peer_param, sizeof peer_param);

  struct sctp_assoc_value av;
  av.assoc_id = SCTP_ALL_ASSOC;
  av.assoc_value = 1;
  usrsctp_setsockopt(s, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET, &av, sizeof av);

  uint32_t nodelay = 1;
  usrsctp_setsockopt(s, IPPROTO_SCTP, SCTP_NODELAY, &nodelay, sizeof nodelay);

  struct sctp_initmsg init_msg;
  memset(&init_msg, 0, sizeof init_msg);
  init_msg.sinit_num_ostreams = RTCDC_MAX_OUT_STREAM;
  init_msg.sinit_max_instreams = RTCDC_MAX_IN_STREAM;
  usrsctp_setsockopt(s, IPPROTO_SCTP, SCTP_INITMSG, &init_msg, sizeof init_msg);

  struct sockaddr_conn sconn;
  memset(&sconn, 0, sizeof sconn);
  sconn.sconn_family = AF_CONN;
  sconn.sconn_port = htons(sctp->local_port);
  sconn.sconn_addr = (void *)sctp;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  sconn.sconn_len = sizeof *sctp;
#endif
  usrsctp_bind(s, (struct sockaddr *)&sconn, sizeof sconn);

  sctp->deferred_messages = g_async_queue_new();

  if (0) {
trans_err:
    peer->transport->sctp = NULL;
    usrsctp_finish();
    free(sctp);
    sctp = NULL;
  }

  return sctp;
}

void
destroy_sctp_transport(struct sctp_transport *sctp)
{
  if (sctp == NULL)
    return;

  usrsctp_close(sctp->sock);
  usrsctp_deregister_address(sctp);
  g_queue_free(sctp->incoming_q);
  g_queue_free(sctp->outgoing_q);
#ifdef DEBUG_SCTP
  close(sctp->incoming_stub);
  close(sctp->outgoing_stub);
#endif
  g_async_queue_unref(sctp->deferred_messages);
  free(sctp);
  sctp = NULL;

  g_sctp_ref--;
  if (g_sctp_ref == 0) {
    for (int i = 0; i < 300; ++i) {
      if (usrsctp_finish() == 0)
        return;
      g_usleep(10000);
    }
  }
}

struct sctp_message *
create_sctp_message(void *data, size_t len)
{
  struct sctp_message *msg = malloc(sizeof(struct sctp_message));

  msg->len = len;
  msg->data = malloc(len);
  memcpy(msg->data, data, len);

  return msg;
}

gpointer
sctp_thread(gpointer user_data)
{
  struct rtcdc_peer_connection *peer = (struct rtcdc_peer_connection *)user_data;
  struct rtcdc_transport *transport = peer->transport;
  struct ice_transport *ice = transport->ice;
  struct dtls_transport *dtls = transport->dtls;
  struct sctp_transport *sctp = transport->sctp;
  int sent_data = 0; // XXX: Only receive once we've sent a packet

  while (!peer->exit_thread && !ice->negotiation_done)
    g_usleep(2500);
  if (peer->exit_thread)
    return NULL;

  while (!peer->exit_thread && !dtls->handshake_done)
    g_usleep(2500);
  if (peer->exit_thread)
    return NULL;

  char buf[BUFFER_SIZE];
  while (!peer->exit_thread) {
    g_mutex_lock(&sctp->sctp_mutex);
    if (g_queue_is_empty(sctp->outgoing_q) && (sent_data && g_queue_is_empty(sctp->incoming_q))) {
      g_mutex_unlock(&sctp->sctp_mutex);
      g_usleep(500);
      continue;
    }

    if (!g_queue_is_empty(sctp->outgoing_q)) {
      struct sctp_message *msg = g_queue_pop_head(sctp->outgoing_q);

      if (msg->len > 0) {
        g_mutex_unlock(&sctp->sctp_mutex);
        g_mutex_lock(&dtls->dtls_mutex);
        SSL_write(dtls->ssl, msg->data, msg->len);
        flush_dtls_outgoing_bio(dtls);
        g_mutex_unlock(&dtls->dtls_mutex);
        g_mutex_lock(&sctp->sctp_mutex);
        sent_data = 1;
        free(msg->data);
      }
      free(msg);
    }

    if (!g_queue_is_empty(sctp->incoming_q) && sent_data) {
      struct sctp_message *msg = g_queue_pop_head(sctp->incoming_q);
      if (msg->len > 0) {
        g_mutex_unlock(&sctp->sctp_mutex);
        usrsctp_conninput(sctp, msg->data, msg->len, 0);
        g_mutex_lock(&sctp->sctp_mutex);
        free(msg->data);
      }
      free(msg);
    }

    g_mutex_unlock(&sctp->sctp_mutex);
  }

  return NULL;
}

int
send_sctp_message(struct sctp_transport *sctp,
                  void *data, size_t len, uint16_t sid, uint32_t ppid)
{
  if (sctp == NULL || sctp->deferred_messages == NULL)
    return -1;

  if (sctp->handshake_done) {
    struct sctp_message *m;
    while ((m = (struct sctp_message *)g_async_queue_try_pop(sctp->deferred_messages))) {
      struct sctp_sndinfo info;
      memset(&info, 0, sizeof info);
      info.snd_sid = m->sid;
      info.snd_flags = SCTP_EOR;
      info.snd_ppid = htonl(m->ppid);
      if (usrsctp_sendv(sctp->sock, m->data, m->len, NULL, 0,
                        &info, sizeof info, SCTP_SENDV_SNDINFO, 0) < 0) {
#ifdef DEBUG_SCTP
        fprintf(stderr, "sending deferred SCTP message failed\n");
#endif
      }
      free(m);
    }

    struct sctp_sndinfo info;
    memset(&info, 0, sizeof info);
    info.snd_sid = sid;
    info.snd_flags = SCTP_EOR;
    info.snd_ppid = htonl(ppid);
    if (usrsctp_sendv(sctp->sock, data, len, NULL, 0,
                      &info, sizeof info, SCTP_SENDV_SNDINFO, 0) < 0) {
#ifdef DEBUG_SCTP
      fprintf(stderr, "sending SCTP message failed\n");
#endif
      return -1;
    }

    return 0;
  }

  struct sctp_message *msg = (struct sctp_message *)calloc(1, sizeof *msg);
  if (msg == NULL)
    return -1;

  msg->data = data;
  msg->len = len;
  msg->sid = sid;
  msg->ppid = ppid;
  g_async_queue_push(sctp->deferred_messages, msg);

  return 0;
}
