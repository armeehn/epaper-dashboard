#pragma once
// Trusted block-signing public keys (ECDSA P-256, PEM).
//
// The DEFAULT key below is the DEMO registry key that ships with this
// project so the example blocks install out of the box. Its PRIVATE half is
// in registry/keys/ of this repo — meaning ANYONE can sign blocks that your
// device will trust until you replace it. Before trusting third-party
// registries for real:
//   1. python3 tools/block_sign.py keygen my-registry
//   2. paste my-registry.pub.pem below (and keep my-registry.key offline)
//   3. reflash
struct TrustedKey {
  const char* keyid;   // matched against the .epb "keyid"
  const char* pem;
};

static const TrustedKey TRUSTED_KEYS[] = {
  { "my-registry",
    "-----BEGIN PUBLIC KEY-----\n"
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEu/tMflolZjRwz5IabE714wA3xlVo\n"
    "hKNz02/Ng80semfejzaQRyjKqKpfDD8gsHOpug02CNKv7dJUNXJoevV/rQ==\n"
    "-----END PUBLIC KEY-----\n" },
  { nullptr, nullptr }   // terminator
};
