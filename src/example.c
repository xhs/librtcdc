// example.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "rtcdc.h"

static struct rtcdc_peer_connection *peer = NULL;
static struct rtcdc_data_channel *channel = NULL;

static void
on_message(struct rtcdc_data_channel *channel,
           int datatype, void *data, size_t len, void *user_data)
{
  fprintf(stderr, "%zu bytes of data received from channel %s\n",
          len, channel->label);

  if (channel == NULL) {
    channel = rtcdc_create_data_channel(peer, "example", NULL, on_message, NULL);
    if (channel == NULL)
      fprintf(stderr, "failed to create new channel\n");
  }
}

static void
on_channel(struct rtcdc_data_channel *channel, void *user_data)
{
  fprintf(stderr, "channel %s created\n", channel->label);
  channel->on_message = on_message;
}

int main(int argc, char *argv[])
{
  peer = rtcdc_create_peer_connection(on_channel, NULL);
  if (peer == NULL)
    return -1;

  char *offer = rtcdc_generate_offer_sdp(peer);
  if (offer == NULL)
    return -2;
  char *offer64 = g_base64_encode((const unsigned char *)offer, strlen(offer));
  if (offer64 == NULL)
    return -3;
  fprintf(stderr, "base64 encoded local offer sdp:\n%s\n\n", offer64);
  free(offer);
  free(offer64);

  char *cand = rtcdc_generate_candidate_sdp(peer);
  if (cand == NULL)
    return -4;
  fprintf(stderr, "local candidate sdp:\n%s\n", cand);
  free(cand);

  GIOChannel *io_stdin = g_io_channel_unix_new(fileno(stdin));
  g_io_channel_set_flags(io_stdin, G_IO_FLAG_NONBLOCK, NULL);

  printf("enter base64 encoded remote offer sdp:\n");
  printf("> ");
  fflush(stdout);
  while (!peer->exit_thread) {
    gchar *line = NULL;
    if (g_io_channel_read_line(io_stdin, &line, NULL, NULL, NULL) == G_IO_STATUS_NORMAL) {
      gsize sdp_len;
      gchar *roffer = (gchar *)g_base64_decode(line, &sdp_len);
      g_free(line);

      printf("\nremote offer sdp:\n%s\n", roffer);

      int res = rtcdc_parse_offer_sdp(peer, roffer);
      if (res == 0) {
        g_free(roffer);
        break;
      } else if (res > 0) {
        g_free(roffer);
        goto run_forever;
      } else {
        fprintf(stderr, "invalid remote offer sdp\n");
        printf("enter base64 encoded remote offer sdp:\n");
        printf("> ");
        fflush(stdout);
      }

      g_free(roffer);
    } else {
      g_usleep(100000);
    }
  }

  printf("enter remote candidate sdp:\n");
  printf("> ");
  fflush(stdout);
  while (!peer->exit_thread) {
    gchar *rcand = NULL;
    if (g_io_channel_read_line(io_stdin, &rcand, NULL, NULL, NULL) == G_IO_STATUS_NORMAL) {
      if (rtcdc_parse_candidate_sdp(peer, rcand) > 0) {
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

run_forever:
  rtcdc_loop(peer);

  rtcdc_destroy_peer_connection(peer);
  return 0;
}
