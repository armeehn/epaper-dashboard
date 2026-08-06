## What this changes and why



## Checklist

- [ ] `bash tests/host/run_tests.sh` passes
- [ ] Checks added for any parser or logic change
- [ ] `python3 tools/make_assets.py` re-run and `portal_assets.h` committed, if `web/` changed
- [ ] No hardware constant (resolution, refresh time, chip, grid) hard-coded in `web/` — it belongs in `device{}` from `/api/state`
- [ ] `docs/HARDWARE.md` updated with an honest status, if a panel preset was added

## Tested on

- [ ] Host suite only
- [ ] Real hardware — which panel and board:

<!--
Adding a block? It goes to https://github.com/armeehn/epaper-blocks instead;
nothing in this repository needs to change.
-->
