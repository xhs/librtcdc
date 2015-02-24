// sctp.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include "common.h"
#include "util.h"
#include "ice.h"
#include "dtls.h"
#include "sctp.h"
#include "dcep.h"
#include "rtcdc.h"

static int interested_events[] = {
  //...
};

static int
sctp_data_ready_cb(void *reg_addr, void *data, size_t len, uint8_t tos, uint8_t set_df)
{
  struct sctp_transport *sctp = (struct sctp_transport *)reg_addr;
  g_mutex_lock(&sctp->sctp_mutex);
  BIO_write(sctp->outgoing_bio, data, len);
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
                      size_t len, struct sctp_rcvinfo recv_info, int flags, void *user_data)
{
  if (user_data == NULL || len == 0)
    return -1;

  struct rtcdc_peer_connection *peer = (struct rtcdc_peer_connection *)user_data;
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
create_sctp_transport(struct rtcdc_peer_connection *peer, int lport, int rport)
{
  if (peer == NULL)
    return NULL;

  struct sctp_transport *sctp = (struct sctp_transport *)calloc(1, sizeof *sctp);
  if (sctp == NULL)
    return NULL;

  if (lport > 0)
    sctp->local_port = lport;
  else
    sctp->local_port = random_integer(10000, 60000);

  if (rport > 0) {
    sctp->remote_port = rport;
    sctp->stream_cursor = 0;
  } else
    sctp->stream_cursor = 1;

  usrsctp_init(0, sctp_data_ready_cb, NULL);
  usrsctp_register_address(sctp);
  usrsctp_sysctl_set_sctp_ecn_enable(0);
  struct socket *s = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP,
                                    sctp_data_received_cb, NULL, 0, peer);
  if (s == NULL)
    goto trans_err;
  sctp->sock = s;

  BIO *bio = BIO_new(BIO_s_mem());
  if (bio == NULL)
    goto trans_err;
  BIO_set_mem_eof_return(bio, -1);
  sctp->incoming_bio = bio;

  bio = BIO_new(BIO_s_mem());
  if (bio == NULL)
    goto trans_err;
  BIO_set_mem_eof_return(bio, -1);
  sctp->outgoing_bio = bio;

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
  // send abort when call close()
  usrsctp_setsockopt(s, SOL_SOCKET, SO_LINGER, &lopt, sizeof lopt);

  struct sctp_assoc_value av;
  av.assoc_id = SCTP_ALL_ASSOC;
  av.assoc_value = 1;
  usrsctp_setsockopt(s, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET, &av, sizeof av);

  uint32_t nodelay = 1;
  usrsctp_setsockopt(s, IPPROTO_SCTP, SCTP_NODELAY, &nodelay, sizeof nodelay);

  struct sctp_initmsg init_msg;
  memset(&init_msg, 0, sizeof init_msg);
  init_msg.sinit_num_ostreams = MAX_OUT_STREAM;
  init_msg.sinit_max_instreams = MAX_IN_STREAM;
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
  usrsctp_finish();
  BIO_free_all(sctp->incoming_bio);
  BIO_free_all(sctp->outgoing_bio);
#ifdef DEBUG_SCTP
  close(sctp->incoming_stub);
  close(sctp->outgoing_stub);
#endif
  g_async_queue_unref(sctp->deferred_messages);
  free(sctp);
  sctp = NULL;
}

gpointer
sctp_thread(gpointer user_data)
{
  struct rtcdc_peer_connection *peer = (struct rtcdc_peer_connection *)user_data;
  struct rtcdc_transport *transport = peer->transport;
  struct ice_transport *ice = transport->ice;
  struct dtls_transport *dtls = transport->dtls;
  struct sctp_transport *sctp = transport->sctp;

  while (!ice->exit_thread && !ice->negotiation_done)
    g_usleep(10000);
  if (ice->exit_thread)
    return NULL;

  while (!ice->exit_thread && !dtls->handshake_done)
    g_usleep(10000);
  if (ice->exit_thread)
    return NULL;

  char buf[BUFFER_SIZE];
  while (!ice->exit_thread) {
    if (BIO_ctrl_pending(sctp->incoming_bio) <= 0 && BIO_ctrl_pending(sctp->outgoing_bio) <= 0) {
      g_usleep(5000);
      continue;
    }

    if (BIO_ctrl_pending(sctp->incoming_bio) > 0) {
      g_mutex_lock(&sctp->sctp_mutex);
      int nbytes = BIO_read(sctp->incoming_bio, buf, sizeof buf);
      g_mutex_unlock(&sctp->sctp_mutex);
#ifdef DEBUG_SCTP
      send(sctp->incoming_stub, buf, nbytes, 0);
#endif
      if (nbytes > 0) {
        usrsctp_conninput(sctp, buf, nbytes, 0);
      }
    }

    if (BIO_ctrl_pending(sctp->outgoing_bio) > 0) {
      g_mutex_lock(&sctp->sctp_mutex);
      int nbytes = BIO_read(sctp->outgoing_bio, buf, sizeof buf);
      g_mutex_unlock(&sctp->sctp_mutex);
#ifdef DEBUG_SCTP
      send(sctp->outgoing_stub, buf, nbytes, 0);
#endif
      if (nbytes > 0) {
        g_mutex_lock(&dtls->dtls_mutex);
        SSL_write(dtls->ssl, buf, nbytes);
        g_mutex_unlock(&dtls->dtls_mutex);
      }
    }
  }

  return NULL;
}

gpointer
sctp_startup_thread(gpointer user_data)
{
  struct rtcdc_peer_connection *peer = (struct rtcdc_peer_connection *)user_data;
  struct rtcdc_transport *transport = peer->transport;
  struct ice_transport *ice = transport->ice;
  struct dtls_transport *dtls = transport->dtls;
  struct sctp_transport *sctp = transport->sctp;

  while (!ice->exit_thread && !ice->negotiation_done)
    g_usleep(10000);
  if (ice->exit_thread)
    return NULL;

  while (!ice->exit_thread && !dtls->handshake_done)
    g_usleep(10000);
  if (ice->exit_thread)
    return NULL;

  if (transport->role == RTCDC_ROLE_CLIENT) {
    struct sockaddr_conn sconn;
    memset(&sconn, 0, sizeof sconn);
    sconn.sconn_family = AF_CONN;
    sconn.sconn_port = htons(sctp->remote_port);
    sconn.sconn_addr = (void *)sctp;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    sconn.sconn_len = sizeof *sctp;
#endif
    if (usrsctp_connect(sctp->sock, (struct sockaddr *)&sconn, sizeof sconn) < 0) {
#ifdef DEBUG_SCTP
      fprintf(stderr, "sctp connection failed\n");
#endif
    } else {
#ifdef DEBUG_SCTP
      fprintf(stderr, "sctp connection connected\n");
#endif
      sctp->handshake_done = TRUE;
    }
  } else {
    struct sockaddr_conn sconn;
    memset(&sconn, 0, sizeof sconn);
    sconn.sconn_family = AF_CONN;
    sconn.sconn_port = htons(sctp->local_port);
    sconn.sconn_addr = (void *)sctp;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    sconn.sconn_len = sizeof *sctp;
#endif
    usrsctp_listen(sctp->sock, 1);
    socklen_t len = sizeof sconn;
    struct socket *s = usrsctp_accept(sctp->sock, (struct sockaddr *)&sconn, &len);
    if (s) {
#ifdef DEBUG_SCTP
    fprintf(stderr, "sctp connection accepted\n");
#endif
      sctp->handshake_done = TRUE;
      struct socket *t = sctp->sock;
      sctp->sock = s;
      usrsctp_close(t);
    } else {
#ifdef DEBUG_SCTP
    fprintf(stderr, "sctp connection failed\n");
#endif
    }
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
        fprintf(stderr, "sending sctp message failed\n");
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
      fprintf(stderr, "sending sctp message failed\n");
#endif
      return -1;
    }
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
