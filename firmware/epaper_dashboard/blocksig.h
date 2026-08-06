#pragma once
// Signed-block envelope (.epb): base64 payload + ECDSA P-256/SHA-256
// signature over the exact payload bytes. Verified against the trusted
// keys baked into trusted_keys.h BEFORE the payload is parsed or stored.
#include <Arduino.h>

struct EpbInfo {
  bool sigPresent = false;
  bool sigOk = false;
  char keyid[24] = "";
  char err[80] = "";
};

// Parse the envelope and verify its signature. On success payloadOut holds
// the raw block JSON. If no signature is present, sigPresent=false and the
// caller decides (require-signed policy). Returns false only on malformed
// envelopes / oversized payloads.
bool epbOpen(const char* envJson, size_t len, String& payloadOut, EpbInfo& info);

// Verify sig (DER, base64-decoded already) over payload bytes with the
// trusted key matching keyid. Exposed for tests.
bool epbVerifySig(const uint8_t* payload, size_t payloadLen,
                  const uint8_t* sigDer, size_t sigLen,
                  const char* keyid, char* err, size_t errLen);
