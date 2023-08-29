#include "tls.h"

#if MG_TLS == MG_TLS_BUILTIN
struct tls_data {
  uint8_t client_random[32];  // From client hello
  uint8_t client_pub[32];     // From client hello
};
struct tls_ctx {
  struct mg_iobuf server_cert;  // Decoded server certificate
  struct mg_iobuf server_key;   // Decoded server private key
};

#define MG_LOAD_BE16(p) ((uint16_t) ((MG_U8P(p)[0] << 8U) | MG_U8P(p)[1]))
#define TLS_HDR_SIZE 5  // 1 byte type, 2 bytes version, 2 bytes len

static inline bool mg_is_big_endian(void) {
  int v = 1;
  return *(unsigned char *) &v == 1;
}
static inline uint16_t mg_swap16(uint16_t v) {
  return (uint16_t) ((v << 8U) | (v >> 8U));
}
static inline uint16_t mg_be16(uint16_t v) {
  return mg_is_big_endian() ? mg_swap16(v) : v;
}
#if 0
static inline uint32_t mg_swap32(uint32_t v) {
  return (v >> 24) | (v >> 8 & 0xff00) | (v << 8 & 0xff0000) | (v << 24);
}
static inline uint64_t mg_swap64(uint64_t v) {
  return (((uint64_t) mg_swap32((uint32_t) v)) << 32) |
         mg_swap32((uint32_t) (v >> 32));
}
static inline uint32_t mg_be32(uint32_t v) {
  return mg_is_big_endian() ? mg_swap32(v) : v;
}
#endif

static inline void add8(struct mg_iobuf *io, uint8_t data) {
  mg_iobuf_add(io, io->len, &data, sizeof(data));
}
static inline void add16(struct mg_iobuf *io, uint16_t data) {
  data = mg_htons(data);
  mg_iobuf_add(io, io->len, &data, sizeof(data));
}
static inline void add32(struct mg_iobuf *io, uint32_t data) {
  data = mg_htonl(data);
  mg_iobuf_add(io, io->len, &data, sizeof(data));
}

void mg_tls_init(struct mg_connection *c, struct mg_str hostname) {
  struct tls_data *tls = (struct tls_data *) calloc(1, sizeof(struct tls_data));
  if (tls != NULL) {
    // tls->send.align = tls->recv.align = MG_IO_SIZE;
    c->tls = tls;
    c->is_tls = c->is_tls_hs = 1;
  } else {
    mg_error(c, "tls oom");
  }
  (void) hostname;
}
void mg_tls_free(struct mg_connection *c) {
  struct tls_data *tls = c->tls;
  if (tls != NULL) {
    // mg_iobuf_free(&tls->send);
    // mg_iobuf_free(&tls->recv);
  }
  free(c->tls);
  c->tls = NULL;
}
long mg_tls_send(struct mg_connection *c, const void *buf, size_t len) {
  (void) c, (void) buf, (void) len;
  // MG_INFO(("BBBBBBBB"));
  return -1;
}
long mg_tls_recv(struct mg_connection *c, void *buf, size_t len) {
  (void) c, (void) buf, (void) len;
  char tmp[8192];
  long n = mg_io_recv(c, tmp, sizeof(tmp));
  if (n > 0) mg_hexdump(tmp, (size_t) n);
  MG_INFO(("AAAAAAAA"));
  return -1;
  // struct mg_tls *tls = (struct mg_tls *) c->tls;
  // long n = mbedtls_ssl_read(&tls->ssl, (unsigned char *) buf, len);
  // if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE)
  //   return MG_IO_WAIT;
  // if (n <= 0) return MG_IO_ERR;
  // return n;
}
size_t mg_tls_pending(struct mg_connection *c) {
  (void) c;
  return 0;
}
void mg_tls_handshake(struct mg_connection *c) {
  // struct tls_data *tls = c->tls;
  struct mg_iobuf *rio = &c->raw;
  struct mg_iobuf *wio = &c->send;

  // Look if we've pulled everything
  if (rio->len < TLS_HDR_SIZE) return;
  uint8_t record_type = rio->buf[0];
  uint16_t record_len = MG_LOAD_BE16(rio->buf + 3);
  uint16_t record_version = MG_LOAD_BE16(rio->buf + 1);
  if (record_type != 22) {
    mg_error(c, "not a handshake");
    return;
  }
  if (rio->len < (size_t) TLS_HDR_SIZE + record_len) return;
  // Got full hello
  // struct tls_hello *hello = (struct tls_hello *) (hdr + 1);
  MG_INFO(("CT=%d V=%hx L=%hu", record_type, record_version, record_len));
  // mg_hexdump(rio->buf, rio->len);

  // Send response. Server Hello
  size_t ofs = wio->len;
  add8(wio, 22), add16(wio, 0x303), add16(wio, 0);  // Layer: type, ver, len
  add8(wio, 2), add8(wio, 0), add16(wio, 0), add16(wio, 0x304);  // Hello
  mg_iobuf_add(wio, wio->len, NULL, 32);                         // 32 random
  mg_random(wio->buf + wio->len - 32, 32);                       // bytes
  add8(wio, 0);                                                  // Session ID
  add16(wio, 0x1301);  // Cipher: TLS_AES_128_GCM_SHA256
  add8(wio, 0);        // Compression method: 0
  add16(wio, 46);      // Extensions length
  add16(wio, 43), add16(wio, 2), add16(wio, 0x304);  // extension: TLS 1.3

  // Key share: use curve x25519 (id 29)
  add16(wio, 51), add16(wio, 36), add16(wio, 29), add16(wio, 32);  // keyshare
  mg_iobuf_add(wio, wio->len, NULL, 32);                           // 32 random
  mg_random(wio->buf + wio->len - 32, 32);                         // bytes
  *(uint16_t *) &wio->buf[ofs + 3] = mg_be16((uint16_t) (wio->len - ofs - 5));
  *(uint16_t *) &wio->buf[ofs + 7] = mg_be16((uint16_t) (wio->len - ofs - 9));

  // Change cipher. Cipher's payload is an encypted app data
  // ofs = wio->len;
  add8(wio, 20), add16(wio, 0x303);  // Layer: type, version
  add16(wio, 1), add8(wio, 1);

  ofs = wio->len;                                   // Application data
  add8(wio, 23), add16(wio, 0x303), add16(wio, 5);  // Layer: type, version
  // mg_iobuf_add(wio, wio->len, "\x01\x02\x03\x04\x05", 5);
  add8(wio, 22);                                       // handshake message
  add8(wio, 8);                                        // encrypted extensions
  add8(wio, 0), add16(wio, 2), add16(wio, 0);          // empty 2 bytes
  add8(wio, 11);                                       // certificate message
  add8(wio, 0), add16(wio, 4), add32(wio, 0x1020304);  // len
  *(uint16_t *) &wio->buf[ofs + 3] = mg_be16((uint16_t) (wio->len - ofs - 5));

  mg_io_send(c, wio->buf, wio->len);
  wio->len = 0;

  rio->len = 0;
  c->is_tls_hs = 0;
  mg_error(c, "doh");
}
void mg_tls_ctx_free(struct mg_mgr *mgr) {
  free(mgr->tls_ctx);
  mgr->tls_ctx = NULL;
}
static void pem_load(struct mg_str pem, struct mg_iobuf *io) {
  if (pem.len > 0) {
    if (pem.ptr[0] == '-') {
      char in[4], out[4];
      size_t i = 0, j = 0;
      while (i < pem.len && pem.ptr[i] != '\n') i++;   // Skip first line, -----
      for (; i < pem.len && pem.ptr[i] != '-'; i++) {  // Until the end -----
        if (pem.ptr[i] == '\r' || pem.ptr[i] == '\n') continue;
        in[j++] = pem.ptr[i];
        if (j >= sizeof(in)) {
          int n = mg_base64_decode(in, 4, out);
          mg_iobuf_add(io, io->len, out, n);
          j = 0;
        }
      }
      mg_hexdump(io->buf, io->len);
    } else {
      mg_iobuf_add(io, io->len, pem.ptr, pem.len);  // Already in DER
    }
  }
}
void mg_tls_ctx_init(struct mg_mgr *mgr, const struct mg_tls_opts *opts) {
  struct tls_ctx *ctx = (struct tls_ctx *) calloc(1, sizeof(*ctx));
  ctx->server_key.align = ctx->server_cert.align = 128;
  pem_load(opts->server_cert, &ctx->server_cert);
  pem_load(opts->server_key, &ctx->server_key);
  mgr->tls_ctx = ctx;
}
#endif
