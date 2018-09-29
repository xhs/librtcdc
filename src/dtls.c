// dtls.c
// Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
// This file is licensed under a BSD license.

#include <stdio.h>
#include <stdlib.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include "common.h"
#include "dtls.h"
#include "rtcdc.h"
#include "sctp.h"

static EVP_PKEY *
gen_key()
{
  EVP_PKEY *pkey = EVP_PKEY_new();
  BIGNUM *exponent = BN_new();
  RSA *rsa = RSA_new();
  if (!pkey || !exponent || !rsa ||
      !BN_set_word(exponent, 0x10001) || // 65537
      !RSA_generate_key_ex(rsa, 1024, exponent, NULL) ||
      !EVP_PKEY_assign_RSA(pkey, rsa)) {
    EVP_PKEY_free(pkey);
    BN_free(exponent);
    RSA_free(rsa);
    return NULL;
  }
  BN_free(exponent);
  return pkey;
}

static X509 *
gen_cert(EVP_PKEY* pkey, const char *common, int days) {
  X509 *x509 = NULL;
  BIGNUM *serial_number = NULL;
  X509_NAME *name = NULL;

  if ((x509 = X509_new()) == NULL)
    return NULL;

  if (!X509_set_pubkey(x509, pkey))
    return NULL;

  ASN1_INTEGER* asn1_serial_number;
  if ((serial_number = BN_new()) == NULL ||
      !BN_pseudo_rand(serial_number, 64, 0, 0) ||
      (asn1_serial_number = X509_get_serialNumber(x509)) == NULL ||
      !BN_to_ASN1_INTEGER(serial_number, asn1_serial_number))
    goto cert_err;

  if (!X509_set_version(x509, 0L)) // version 1
    goto cert_err;

  if ((name = X509_NAME_new()) == NULL ||
      !X509_NAME_add_entry_by_NID(
          name, NID_commonName, MBSTRING_UTF8,
          (unsigned char*)common, -1, -1, 0) ||
      !X509_set_subject_name(x509, name) ||
      !X509_set_issuer_name(x509, name))
    goto cert_err;

  if (!X509_gmtime_adj(X509_get_notBefore(x509), 0) ||
      !X509_gmtime_adj(X509_get_notAfter(x509), days * 24 * 3600))
    goto cert_err;

  if (!X509_sign(x509, pkey, EVP_sha1()))
    goto cert_err;

  if (0) {
cert_err:
    X509_free(x509);
    x509 = NULL;
  }
  BN_free(serial_number);
  X509_NAME_free(name);

  return x509;
}

static int
verify_peer_certificate_cb(int ok, X509_STORE_CTX *ctx)
{
  return 1;
}

struct dtls_context *
create_dtls_context(const char *common)
{
  if (common == NULL)
    return NULL;

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

  SSL_CTX_set_read_ahead(ctx, 1); // for DTLS
  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_peer_certificate_cb);

  EVP_PKEY *key = gen_key();
  if (key == NULL)
    goto ctx_err;
  SSL_CTX_use_PrivateKey(ctx, key);

  X509 *cert = gen_cert(key, common, 365);
  if (cert == NULL)
    goto ctx_err;
  SSL_CTX_use_certificate(ctx, cert);

  if (SSL_CTX_check_private_key(ctx) != 1)
    goto ctx_err;

  unsigned int len;
  unsigned char buf[BUFFER_SIZE];
  X509_digest(cert, EVP_sha256(), buf, &len);

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
create_dtls_transport(struct rtcdc_peer_connection *peer,
                      const struct dtls_context *context)
{
  if (peer == NULL || peer->transport == NULL || context == NULL || context->ctx == NULL)
    return NULL;

  struct dtls_transport *dtls = (struct dtls_transport *)calloc(1, sizeof *dtls);
  if (dtls == NULL)
    return NULL;
  peer->transport->dtls = dtls;

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

  dtls->outgoing_q = g_queue_new();
  if (dtls->outgoing_q == NULL)
    goto trans_err;

  SSL_set_bio(dtls->ssl, dtls->incoming_bio, dtls->outgoing_bio);

  EC_KEY *ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  SSL_set_options(dtls->ssl, SSL_OP_SINGLE_ECDH_USE);
  SSL_set_tmp_ecdh(dtls->ssl, ecdh);
  EC_KEY_free(ecdh);

  if (0) {
trans_err:
    peer->transport->dtls = NULL;
    SSL_free(ssl);
    free(dtls);
    dtls = NULL;
  }

  return dtls;
}

void
destroy_dtls_transport(struct dtls_transport *dtls)
{
  if (dtls == NULL)
    return;

  SSL_free(dtls->ssl);
  free(dtls);
  dtls = NULL;
}

void
flush_dtls_outgoing_bio(struct dtls_transport *dtls)
{
  if ((dtls == NULL) || (dtls->outgoing_bio == NULL) || (dtls->outgoing_q == NULL))
    return;

  char buf[BUFFER_SIZE];
  while (BIO_ctrl_pending(dtls->outgoing_bio)) {
    int nbytes = BIO_read(dtls->outgoing_bio, buf, sizeof(buf));
    if (nbytes > 0) {
      struct sctp_message *msg = create_sctp_message(buf, nbytes);
      if (msg != NULL) {
        g_queue_push_tail(dtls->outgoing_q, msg);
      }
    }
  }
}
