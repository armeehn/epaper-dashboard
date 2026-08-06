#pragma once
// Trusted block-signing public keys (ECDSA P-256, PEM).
//
// A .epb is accepted only if one of these keys verifies its signature, and
// the check runs BEFORE the descriptor is parsed or stored. Which registry a
// device browses is a runtime setting; which keys it trusts is this file —
// changing it means recompiling and reflashing.
//
// The default anchor below is the key of the official block registry,
// https://github.com/armeehn/epaper-blocks. Its private half is held offline
// and is in no repository; the matching public half is published there as
// keys/epaper-blocks.pub.pem.
//
// To trust your own registry instead:
//   1. python3 tools/block_sign.py keygen my-registry
//   2. replace the entry below with my-registry.pub.pem and your own keyid
//      (keep my-registry.key offline — .gitignore already excludes *.key)
//   3. reflash
// Additional keys can be appended; any one match is sufficient.
struct TrustedKey {
  const char* keyid;   // matched against the .epb "keyid"
  const char* pem;
};

static const TrustedKey TRUSTED_KEYS[] = {
  { "epaper-blocks-2026",
    "-----BEGIN PUBLIC KEY-----\n"
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEu/tMflolZjRwz5IabE714wA3xlVo\n"
    "hKNz02/Ng80semfejzaQRyjKqKpfDD8gsHOpug02CNKv7dJUNXJoevV/rQ==\n"
    "-----END PUBLIC KEY-----\n" },
  { nullptr, nullptr }   // terminator
};
