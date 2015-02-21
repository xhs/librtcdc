// rtcdc.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "ice.h"
#include "dtls.h"
#include "sctp.h"
#include "rtcdc.h"

struct rtcdc_transport {
  struct dtls_context *ctx;
  struct ice_transport *ice;
};

struct rtcdc_peer_connection *
rtcdc_create_peer_connection(void (*on_channel)(struct rtcdc_data_channel *channel))
{
  struct rtcdc_peer_connection *peer =
    (struct rtcdc_peer_connection *)calloc(1, sizeof *peer);
  if (peer == NULL)
    return NULL;
  peer->on_channel = on_channel;

  struct rtcdc_transport *transport = 
    (struct rtcdc_transport *)calloc(1, sizeof *transport);
  if (transport == NULL)
    goto trans_null_err;
  peer->transport = transport;

  struct dtls_context *ctx = create_dtls_context("./test.crt", "./test.key");
  if (ctx == NULL)
    goto ctx_null_err;
  transport->ctx = ctx;

  //...

  if (0) {
ctx_null_err:
    free(transport);
trans_null_err:
    free(peer);
    peer = NULL;
  }

  return peer;
}
