// dtls.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a GNU GPLv3 license.

#include <stdio.h>
#include "common.h"
#include "dtls.h"

static int
verify_peer_certificate_cb(int ok, X509_STORE_CTX *ctx)
{
  return 1;
}

struct dtls_context *
create_dtls_context(const char *cert, const char *key)
{
  struct dtls_context *context = (struct dtls_context *)calloc(1, sizeof *context);
  if (context == NULL)
    return NULL;

  SSL_library_init();
  OpenSSL_add_all_algorithms();

  SSL_CTX *ctx = SSL_CTX_new(DTLSv1_method());
  if (ctx == NULL)
    goto ctx_err;
  context->ctx = ctx;

  // ALL:NULL:eNULL:aNULL
  if (SSL_CTX_set_cipher_list(ctx, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH") != 1)
    goto ctx_err;

  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_peer_certificate_cb);
  // todo: generate cert/key dynamically
  SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM);
  SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM);
  if (SSL_CTX_check_private_key(ctx) != 1)
    goto ctx_err;

  BIO *cert_bio = BIO_new(BIO_s_file());
  if (cert_bio == NULL)
    goto ctx_err;
  
  if (BIO_read_filename(cert_bio, cert) != 1) {
    BIO_free_all(cert_bio);
    goto ctx_err;
  }

  X509 *x509_cert = PEM_read_bio_X509(cert_bio, NULL, 0, NULL);
  if (x509_cert == NULL) {
    BIO_free_all(cert_bio);
    goto ctx_err;
  }

  unsigned int len;
  unsigned char buf[BUFFER_SIZE];
  X509_digest(x509_cert, EVP_sha256(), buf, &len);
  BIO_free_all(cert_bio);
  X509_free(x509_cert);

  char *p = context->fingerprint;
  for (int i = 0; i < len; ++i) {
    snprintf(p, 4, "%02X:", buf[i]);
    p += 3;
  }
  *(p - 1) = 0;

  if (0) {
ctx_err:
    SSL_CTX_free(ctx);
    free(context);
    context = NULL;
  }

  return context;
}

void
destroy_dtls_context(struct dtls_context *context)
{
  if (context == NULL)
    return;

  SSL_CTX_free(context->ctx);
  free(context);
  context = NULL;
}

struct dtls_transport *
create_dtls_transport(const struct dtls_context *context, int client)
{
  if (context == NULL || context->ctx == NULL)
    return NULL;

  struct dtls_transport *dtls = (struct dtls_transport *)calloc(1, sizeof *dtls);
  if (dtls == NULL)
    return NULL;
  dtls->state = DTLS_TRANSPORT_STATE_CLOSED;

  SSL *ssl = SSL_new(context->ctx);
  if (ssl == NULL)
    goto trans_err;
  dtls->ssl = ssl;

  BIO *bio = BIO_new(BIO_s_mem());
  if (bio == NULL)
    goto trans_err;
  BIO_set_mem_eof_return(bio, -1);
  dtls->incoming_bio = bio;

  bio = BIO_new(BIO_s_mem());
  if (bio == NULL)
    goto trans_err;
  BIO_set_mem_eof_return(bio, -1);
  dtls->outgoing_bio = bio;

  SSL_set_bio(dtls->ssl, dtls->incoming_bio, dtls->outgoing_bio);

  EC_KEY *ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  SSL_set_options(dtls->ssl, SSL_OP_SINGLE_ECDH_USE);
  SSL_set_tmp_ecdh(dtls->ssl, ecdh);
  EC_KEY_free(ecdh);

  if (client == 1) {
    dtls->role = PEER_CLIENT;
    SSL_set_connect_state(dtls->ssl);
  } else {
    dtls->role = PEER_SERVER;
    SSL_set_accept_state(dtls->ssl);
  }

  if (0) {
trans_err:
    SSL_free(ssl);
    free(dtls);
    dtls = NULL;
  }

  return dtls;
}

void
destroy_dtls_transport(struct dtls_transport *dtls)
{
  if (dtls != NULL)
    return;

  SSL_free(dtls->ssl);
  BIO_free_all(dtls->incoming_bio);
  BIO_free_all(dtls->outgoing_bio);
  free(dtls);
  dtls = NULL;
}
