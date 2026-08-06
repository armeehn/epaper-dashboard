# epaper-blocks registry (contrib)

Community blocks for the e-paper dashboard — in the spirit of
i3blocks-contrib, but **blocks are data, not code**: a ~40-line JSON file
that declares an HTTPS source, value extractions, and a widget. The device's
fixed engine does everything else. Nothing you write here ever executes.

## Contributing a block

1. Create `blocks/<your-block-id>/block.json` (lowercase-kebab id). Copy an
   existing block as a starting point; the full field reference is in
   `../DESIGN.md`.
2. Rules a reviewer will check (and the device enforces):
   - `source.url` is `https://`, a public host, params only in path/query;
   - no secrets — params are plain strings/numbers/choices;
   - descriptor ≤ 4 KB; extracted lists ≤ 6 rows; response ≤ 16 KB;
   - the API is keyless or accepts a user-supplied non-secret parameter.
3. Add a screenshot (render it with the project's preview harness) and a
   short README in your block's directory.
4. Open a PR. On merge, the registry maintainer signs your block with the
   registry key and regenerates the signed `index.json`:

```sh
python3 ../tools/block_sign.py sign keys/<registry>.key <keyid> blocks/<id>/block.json
python3 ../tools/block_sign.py index keys/<registry>.key <keyid> <raw-base-url> blocks/*
```

Users then see it in the portal's **Blocks → registry** list and install it
with one tap; the device verifies the signature before storing anything.

## Keys

`keys/demo-registry.key` is a **demo key whose private half is public** — it
exists so the examples work out of the box. Anyone standing up a real
registry must generate their own (`block_sign.py keygen`), keep the private
key offline, and ship the public key in the firmware's `trusted_keys.h`.
