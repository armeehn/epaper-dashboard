#include "blocksig.h"
#include "blocks.h"       // BLK_MAX_DESC
#include "net_util.h"     // b64decode
#include "trusted_keys.h"
#include <ArduinoJson.h>

#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

bool epbVerifySig(const uint8_t* payload, size_t payloadLen,
                  const uint8_t* sigDer, size_t sigLen,
                  const char* keyid, char* err, size_t errLen) {
  const char* pem = nullptr;
  for (int i = 0; TRUSTED_KEYS[i].keyid; i++)
    if (strcmp(TRUSTED_KEYS[i].keyid, keyid) == 0) { pem = TRUSTED_KEYS[i].pem; break; }
  if (!pem) {
    snprintf(err, errLen, "unknown signing key '%s'", keyid);
    return false;
  }

  uint8_t hash[32];
#if defined(MBEDTLS_VERSION_NUMBER) && MBEDTLS_VERSION_NUMBER >= 0x03000000
  mbedtls_sha256(payload, payloadLen, hash, 0);
#else
  mbedtls_sha256_ret(payload, payloadLen, hash, 0);
#endif

  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  int rc = mbedtls_pk_parse_public_key(&pk, (const unsigned char*)pem, strlen(pem) + 1);
  if (rc != 0) {
    snprintf(err, errLen, "bad trusted key (%d)", rc);
    mbedtls_pk_free(&pk);
    return false;
  }
  rc = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), sigDer, sigLen);
  mbedtls_pk_free(&pk);
  if (rc != 0) {
    snprintf(err, errLen, "signature INVALID (%d)", rc);
    return false;
  }
  return true;
}

bool epbOpen(const char* envJson, size_t len, String& payloadOut, EpbInfo& info) {
  info = EpbInfo();
  JsonDocument doc;
  if (deserializeJson(doc, envJson, len)) {
    strlcpy(info.err, "not a valid .epb (bad JSON)", sizeof(info.err));
    return false;
  }
  const char* fmt = doc["format"] | "";
  const char* payloadB64 = doc["payload"] | "";
  if (strcmp(fmt, "epb1") != 0 || !payloadB64[0]) {
    strlcpy(info.err, "not an epb1 envelope", sizeof(info.err));
    return false;
  }
  // transient heap buffer (rarely called; keeps 4 KB out of static RAM)
  uint8_t* payload = (uint8_t*)malloc(BLK_MAX_DESC + 4);
  if (!payload) {
    strlcpy(info.err, "out of memory", sizeof(info.err));
    return false;
  }
  int plen = b64decode(payloadB64, strlen(payloadB64), payload, BLK_MAX_DESC + 3);
  if (plen <= 0 || plen > BLK_MAX_DESC) {
    strlcpy(info.err, "payload empty or too large", sizeof(info.err));
    free(payload);
    return false;
  }
  payload[plen] = 0;

  JsonArrayConst sigs = doc["sigs"];
  if (sigs.size() > 0) {
    info.sigPresent = true;
    for (JsonObjectConst s : sigs) {
      const char* alg = s["alg"] | "";
      const char* keyid = s["keyid"] | "";
      const char* sigB64 = s["sig"] | "";
      if (strcmp(alg, "ecdsa-p256-sha256") != 0) continue;
      uint8_t sig[96];
      int slen = b64decode(sigB64, strlen(sigB64), sig, sizeof(sig));
      if (slen <= 0) continue;
      char verr[80];
      if (epbVerifySig(payload, plen, sig, slen, keyid, verr, sizeof(verr))) {
        info.sigOk = true;
        strlcpy(info.keyid, keyid, sizeof(info.keyid));
        break;
      } else {
        strlcpy(info.err, verr, sizeof(info.err));
        strlcpy(info.keyid, keyid, sizeof(info.keyid));
      }
    }
  }
  payloadOut = String((const char*)payload);
  free(payload);
  return true;
}
