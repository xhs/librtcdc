// sctp.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "common.h"
#include "util.h"
#include "ice.h"
#include "dtls.h"
#include "sctp.h"
#include "dcep.h"

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
handle_notification_message(struct sctp_transport *sctp, union sctp_notification *notify, size_t len)
{
  //...
}

static void
handle_rtcdc_message(struct sctp_transport *sctp, void *packets, size_t len,
                     uint32_t ppid, uint16_t sid)
{
  fprintf(stderr, "ppid = %x, sid = %x\n", ppid, sid);

  switch (ppid) {
    case WEBRTC_CONTROL_PPID:
      {
        uint8_t msg_type = ((uint8_t *)packets)[0];
        if (msg_type == DATA_CHANNEL_OPEN) {
          fprintf(stderr, "rtcdc open request\n");
        } else if (msg_type == DATA_CHANNEL_ACK) {
          fprintf(stderr, "rtcdc open ack\n");
        }
      }
      break;
    case WEBRTC_STRING_PPID:
    case WEBRTC_STRING_PARTIAL_PPID:
    case WEBRTC_BINARY_PPID:
    case WEBRTC_BINARY_PARTIAL_PPID:
      fprintf(stderr, "rtcdc string/binary\n");
      break;
    case WEBRTC_STRING_EMPTY_PPID:
    case WEBRTC_BINARY_EMPTY_PPID:
      break;
    default:
      fprintf(stderr, "unknown ppid\n");
      break;
  }
}

static int
sctp_data_received_cb(struct socket *sock, union sctp_sockstore addr, void *data,
                      size_t len, struct sctp_rcvinfo recv_info, int flags, void *user_data)
{
  struct sctp_transport *sctp = (struct sctp_transport *)user_data;
  if (sctp == NULL || len == 0)
    return 0;

  fprintf(stderr, "sctp data received\n");

  if (flags & MSG_NOTIFICATION)
    handle_notification_message(sctp, (union sctp_notification *)data, len);
  else
    handle_rtcdc_message(sctp, data, len, recv_info.rcv_ppid, recv_info.rcv_sid);

  return 1;
}

struct sctp_transport *
create_sctp_transport(int lport, int rport)
{
  struct sctp_transport *sctp = (struct sctp_transport *)calloc(1, sizeof *sctp);
  if (sctp == NULL)
    return NULL;

  if (lport > 0)
    sctp->local_port = lport;
  else
    sctp->local_port = random_integer(10000, 60000);

  if (rport > 0) {
    sctp->role = PEER_CLIENT;
    sctp->remote_port = rport;
  } else
    sctp->role = PEER_SERVER;

  usrsctp_init(0, sctp_data_ready_cb, NULL);
  usrsctp_register_address(sctp);
  usrsctp_sysctl_set_sctp_ecn_enable(0);
  struct socket *s = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP,
                                    sctp_data_received_cb, NULL, 0, sctp);
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
  init_msg.sinit_num_ostreams = 16;
  init_msg.sinit_max_instreams = 1024;
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
  free(sctp);
  sctp = NULL;
}

gpointer
sctp_thread(gpointer user_data)
{
  struct ice_transport *ice = (struct ice_transport *)user_data;
  struct dtls_transport *dtls = ice->dtls;
  struct sctp_transport *sctp = ice->sctp;

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
  struct ice_transport *ice = (struct ice_transport *)user_data;
  if (ice == NULL || ice->dtls == NULL || ice->sctp == NULL)
    return NULL;

  struct dtls_transport *dtls = ice->dtls;
  struct sctp_transport *sctp = ice->sctp;

  while (!ice->exit_thread && !ice->negotiation_done)
    g_usleep(10000);
  if (ice->exit_thread)
    return NULL;

  while (!ice->exit_thread && !dtls->handshake_done)
    g_usleep(10000);
  if (ice->exit_thread)
    return NULL;

  if (sctp->role == PEER_CLIENT) {
    struct sockaddr_conn sconn;
    memset(&sconn, 0, sizeof sconn);
    sconn.sconn_family = AF_CONN;
    sconn.sconn_port = htons(sctp->remote_port);
    sconn.sconn_addr = (void *)sctp;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    sconn.sconn_len = sizeof *sctp;
#endif
    usrsctp_connect(sctp->sock, (struct sockaddr *)&sconn, sizeof sconn);
  } else {
    usrsctp_listen(sctp->sock, 1);
  }

  return NULL;
}
