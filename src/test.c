// test.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

  struct sctp_transport *sctp = create_sctp_transport(0, 0);
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
  free(lsdp);

  char *lcand = generate_local_candidate_sdp(ice);
  if (lcand == NULL) {
    fprintf(stderr, "Local candidate error\n");
    ret = -1;
    goto clean_up;
  }
  free(lcand);

clean_up:
  destroy_dtls_context(dtls_ctx);
  destroy_dtls_transport(dtls);
  destroy_sctp_transport(sctp);
  destroy_ice_transport(ice);

  return ret;
}
