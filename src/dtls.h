// dtls.h
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a BSD license.

#ifndef _RTCDC_DTLS_H_
#define _RTCDC_DTLS_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <glib.h>

#define SHA256_FINGERPRINT_SIZE (95 + 1)

struct rtcdc_peer_connection;

struct dtls_context {
  SSL_CTX *ctx;
  char fingerprint[SHA256_FINGERPRINT_SIZE];
};

struct dtls_transport {
  SSL *ssl;
  BIO *incoming_bio;
  BIO *outgoing_bio;
  gboolean handshake_done;
  GMutex dtls_mutex;
};

struct dtls_context *
create_dtls_context(const char *common);

void
destroy_dtls_context(struct dtls_context *context);

struct dtls_transport *
create_dtls_transport(struct rtcdc_peer_connection *peer,
                      const struct dtls_context *context, int client);

void
destroy_dtls_transport(struct dtls_transport *dtls);

#ifdef  __cplusplus
}
#endif

#endif // _RTCDC_DTLS_H_
