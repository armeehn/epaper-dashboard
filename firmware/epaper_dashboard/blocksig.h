#pragma once
// Signed-block envelope (.epb): base64 payload + ECDSA P-256/SHA-256
// signature over the exact payload bytes. Verified against the trusted
// keys baked into trusted_keys.h BEFORE the payload is parsed or stored.
#include <Arduino.h>

struct EpbInfo {
  bool sigPresent = false;
  bool sigOk = false;
  // True once the input is recognised as an epb1 envelope, even if opening it
  // then fails. Callers that fall back to bare JSON need this: without it an
  // envelope rejected for size is indistinguishable from something that was
  // never an envelope, and the fallback buries the real reason.
  bool envelope = false;
  char keyid[24] = "";
  char err[80] = "";
};

// Parse the envelope and verify its signature. On success payloadOut holds
// the raw payload JSON. If no signature is present, sigPresent=false and the
// caller decides (require-signed policy). Returns false only on malformed
// envelopes / oversized payloads.
//
// maxPayload caps the DECODED payload: pass BLK_MAX_DESC for a block
// descriptor, REGISTRY_MAX_PAYLOAD for a registry index. It is deliberately
// not defaulted — an index is several times the size of a descriptor, and
// quietly applying the descriptor cap to one is exactly how a growing
// registry stops loading.
bool epbOpen(const char* envJson, size_t len, String& payloadOut, EpbInfo& info,
             size_t maxPayload);

// Verify sig (DER, base64-decoded already) over payload bytes with the
// trusted key matching keyid. Exposed for tests.
bool epbVerifySig(const uint8_t* payload, size_t payloadLen,
                  const uint8_t* sigDer, size_t sigLen,
                  const char* keyid, char* err, size_t errLen);
