# epaper-blocks registry (contrib)

Community blocks for the e-paper dashboard — in the spirit of
i3blocks-contrib, but **blocks are data, not code**: a ~40-line JSON file
that declares an HTTPS source, value extractions, and a widget. The device's
fixed engine does everything else. Nothing you write here ever executes.

A registry is just **static files behind HTTPS** — this directory, served
from GitHub, is one. There is no server component.

## Installing from this registry

In the portal's **Blocks** tab, paste the index URL and press *Load*:

```
https://raw.githubusercontent.com/armeehn/epaper-dashboard/main/registry/index.json
```

The device verifies the index signature before listing anything, and
verifies each block's own signature before storing it.

> [!WARNING]
> These examples are signed with **`keys/demo-registry.key`, whose private
> half is in this repository** — by design, so the examples install out of
> the box. It is not a trust anchor. Anyone can sign a block your device
> will accept until you replace it; see *Running your own registry* below.

## Contributing a block

1. Create `blocks/<your-block-id>/block.json` (lowercase-kebab id, and the
   **directory name must equal the `id`** — the index publishes each entry
   as `<baseurl>/<id>/<id>.epb`). Copy an existing block as a starting
   point; the full field reference is in `../DESIGN.md`.
2. Rules a reviewer will check (and the device enforces):
   - `source.url` is `https://`, a public host, params only in path/query;
   - no secrets — params are plain strings/numbers/choices, so APIs needing
     a token are out by design;
   - descriptor ≤ 4 KB; extracted lists ≤ 6 rows; response ≤ 16 KB;
   - the API is keyless or accepts a user-supplied non-secret parameter.
3. Add a short README in your block's directory, and a screenshot if you can
   render one.
4. Open a PR. On merge the maintainer re-signs and regenerates the index:

```sh
./rebuild.sh                    # demo key; block URLs derived from origin
```

## Running your own registry

Two independent things — only the second needs a reflash:

| | Where it lives | To change |
| --- | --- | --- |
| Which registry you install from | pasted into the Blocks tab | nothing, it's runtime |
| Whose signature is trusted | `firmware/epaper_dashboard/trusted_keys.h` | recompile + flash |

1. **Mint a key** and keep the private half offline:

   ```sh
   python3 ../tools/block_sign.py keygen my-registry
   ```

2. **Trust it**: paste `my-registry.pub.pem` into `trusted_keys.h` with a
   `keyid` of your choice, **delete the `demo-registry-2026` entry**, and
   reflash. Leaving the demo key in place means anyone can still sign blocks
   your device accepts.

3. **Sign everything** with it — one command, using the same `keyid`:

   ```sh
   ./rebuild.sh my-registry.key my-registry-2026
   ```

   The base URL defaults to this repo's `origin` and current branch. Pass a
   third argument to publish elsewhere (GitHub Pages, your own host); it must
   point at the directory *containing* the block folders:

   ```sh
   ./rebuild.sh my-registry.key my-registry-2026 https://you.github.io/blocks
   ```

4. **Push**, then paste your `index.json` URL into the Blocks tab. It should
   report *"Registry index signature verified"*.

Check any signed file before publishing — this works on blocks and on the
index:

```sh
python3 ../tools/block_sign.py verify my-registry.pub.pem index.json
```

## Limits

The signed index is fetched with a **24 KB** cap (13 blocks ≈ 5.9 KB, so
roughly 50 blocks fit); block responses cap at **16 KB**; a device holds
**16 installed blocks**. Sources must be `https://` and must not resolve to
`.local`, `localhost`, or a private/loopback address.

## Keys

`keys/demo-registry.key` is a **demo key whose private half is public**.
Anyone standing up a real registry must generate their own, keep the private
key offline, and ship the public key in the firmware's `trusted_keys.h`.
`.gitignore` already excludes `keys/*.key` apart from this documented demo.
