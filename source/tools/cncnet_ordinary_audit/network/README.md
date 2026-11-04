# Ordinary CnCNet v17 network audit fixtures

These scripts reuse the checkout's `source/assist/tests/network_house_runtime.py`
and its existing synthetic Win32/world fixtures. Dependencies: Python 3,
`unicorn`, `pefile`, and `capstone` (the shared fixture imports Capstone).
Supply authorized local binaries; no game binaries or memory captures belong in
the repository. The runtime must match this checkout's current fixture ABI.

```sh
python release_cross_sender_cache.py \
  --repo /path/to/RA2-Toolbox-Build \
  --game /private/native-engine.exe \
  --runtime /private/GameRuntime.dll \
  --output /tmp/cross-sender-cache.json

python ordinary_timing_roundtrip.py \
  --repo /path/to/RA2-Toolbox-Build \
  --game /private/native-engine.exe \
  --runtime /private/GameRuntime.dll \
  --spawner /private/CnCNet-Spawner.dll \
  --output /tmp/ordinary-timing.json
```

`release_cross_sender_cache.py` runs four cases: protocols 1/2, each with and
without an intervening sender preserving another sender's House. It executes
the released Toolbox's actual five adapter thunks and native constructors,
sender, compressor and decoder. The two intervention cases reproduce source
group substitution at a third receiver; PASS means the expected result was
reproduced, **not** that the foreign-House feature is safe. No Spawner module is
mapped in this test.

`ordinary_timing_roundtrip.py` runs eight cases: sessions 3/4, adapter off/on,
and timing event alone/mixed with native MegaMission events. It maps the actual
ordinary DLL and executes its three event-size exports through a modeled
Syringe REGISTERS bridge. It checks native roundtrip kind/order/House/payload,
including the v17 `ResponseTime2` kind `0x30`. It requires the official
CnCNetYR v0.0.0.17 non-Hardened DLL with SHA-256
`58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d`.

Both scripts check the required original native sender continuation at
`0x64B9F7` before executing cases. A modified engine that differs writes a
BLOCKED report and exits 2; its instructions are never restored or replaced.
The report records a direct JMP target's section, raw offset, raw bytes and
mapped bytes when available. PASS reports exit 0. Assertion/emulation failures
otherwise retain their traceback and nonzero exit status.

Validated 2026-10-28 against released GameRuntime SHA-256
`eba0ce9f13376aa193942a4765b07fc28e2f63d7a577bed9450325d8e951f4f7`
and authorized engine SHA-256
`268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0`:
4/4 cache cases and 8/8 timing cases reproduced their expected results.
Engine SHA-256
`c287d62a4d6a25b547e9339d071dd6e03adcf545699062e478aaeee38c60e89a`
is BLOCKED: the required continuation jumps to zero-filled `.p_text` raw
padding. This is a fixture coverage limit, not a demonstrated Toolbox defect.

These bounded runs use synthetic Houses, modeled Windows APIs and capture-only
transport. They do not initialize the full Spawner, run its adaptive timing
loop or sockets, execute complete missions, or establish live multiplayer
compatibility/OOS behavior.
