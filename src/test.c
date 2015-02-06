// test.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "dtls.h"
#include "sctp.h"
#include "ice.h"
#include "sdp.h"

int main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s client|server\n", argv[0]);
    return -1;
  }

  int client;
  if (strcmp(argv[1], "client") == 0)
    client = 1;
  else if (strcmp(argv[1], "server") == 0)
    client = 0;
  else {
    fprintf(stderr, "usage: %s client|server\n", argv[0]);
    return -1;
  }

  struct dtls_context *dtls_ctx = create_dtls_context("./test.crt", "./test.key");
  if (dtls_ctx == NULL) {
    fprintf(stderr, "DTLS context error\n");
    return -1;
  }

  struct dtls_transport *dtls = create_dtls_transport(dtls_ctx, client);
  if (dtls == NULL) {
    fprintf(stderr, "DTLS transport error\n");
    destroy_dtls_context(dtls_ctx);
    return -1;
  }

  struct sctp_transport *sctp = create_sctp_transport(6000, 0);
  if (sctp == NULL) {
    fprintf(stderr, "SCTP transport error\n");
    destroy_dtls_context(dtls_ctx);
    destroy_dtls_transport(dtls);
    return -1;
  }

  int ret = 0;
  int controlling = client ? 1 : 0;
  struct ice_transport *ice = create_ice_transport(dtls, sctp, controlling);
  if (ice == NULL) {
    fprintf(stderr, "ICE transport error\n");
    ret = -1;
    goto clean_up;
  }

  char *lsdp = generate_local_sdp(ice, dtls_ctx, client);
  if (lsdp == NULL) {
    fprintf(stderr, "Local SDP error\n");
    ret = -1;
    goto clean_up;
  }
  printf("local sdp:\n%s\n", lsdp);
  char *lsdp64 = g_base64_encode((const unsigned char *)lsdp, strlen(lsdp));
  free(lsdp);
  printf("base64 encoded local sdp:\n%s\n\n", lsdp64);
  free(lsdp64);

  char *lcand = generate_local_candidate_sdp(ice);
  if (lcand == NULL) {
    fprintf(stderr, "Local candidate error\n");
    ret = -1;
    goto clean_up;
  }
  printf("local candidate:\n%s\n", lcand);
  free(lcand);

  GIOChannel *io_stdin = g_io_channel_unix_new(fileno(stdin));
  g_io_channel_set_flags(io_stdin, G_IO_FLAG_NONBLOCK, NULL);

  printf("enter base64 encoded remote sdp:\n");
  printf("> ");
  fflush(stdout);
  while (!ice->exit_thread) {
    gchar *line = NULL;
    if (g_io_channel_read_line(io_stdin, &line, NULL, NULL, NULL) == G_IO_STATUS_NORMAL) {
      gsize sdp_len;
      gchar *rsdp = (gchar *)g_base64_decode(line, &sdp_len);
      g_free(line);

      printf("\nremote sdp:\n%s\n", rsdp);

      int res = parse_remote_sdp(ice, rsdp);
      if (res == 0) {
        g_free(rsdp);
        break;
      } else if (res > 0) {
        g_free(rsdp);
        goto done;
      } else {
        fprintf(stderr, "invalid remote sdp\n");
        printf("enter base64 encoded remote sdp:\n");
        printf("> ");
        fflush(stdout);
      }

      g_free(rsdp);
    } else {
      g_usleep(100000);
    }
  }

  printf("enter remote candidate sdp:\n");
  printf("> ");
  fflush(stdout);
  while (!ice->exit_thread) {
    gchar *rcand = NULL;
    if (g_io_channel_read_line(io_stdin, &rcand, NULL, NULL, NULL) == G_IO_STATUS_NORMAL) {
      if (parse_remote_candidate_sdp(ice, rcand) > 0) {
        g_free(rcand);
        break;
      } else {
        fprintf(stderr, "invalid remote candidate sdp\n");
        printf("enter remote candidate sdp:\n");
        printf("> ");
        fflush(stdout);
      }
      g_free(rcand);
    } else {
      g_usleep(100000);
    }
  }

done:
  printf("done\n");

clean_up:
  destroy_dtls_context(dtls_ctx);
  destroy_dtls_transport(dtls);
  destroy_sctp_transport(sctp);
  destroy_ice_transport(ice);

  return ret;
}
