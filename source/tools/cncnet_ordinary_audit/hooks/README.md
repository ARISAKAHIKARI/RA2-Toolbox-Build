# Ordinary Spawner read-only audit reproduction

This tool set never injects or executes a game or Spawner DLL. It compiles two
small helpers against Toolbox's pure C++17 headers and parses PE registration
tables plus explicitly addressed public source patches.

Inputs are supplied by path; no game executable or DLL is bundled:

- Ordinary CnCNetYR v17 DLL SHA-256:
  `58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d`.
- Toolbox product-code commit:
  `b372ace50101e0099a5e9533c50470b4cabb1536`.
- Toolbox release commit:
  `5508c80f7b81abad4fc51847eb7f3530acddc9af`.
- Corresponding released Windows Runtime DLL SHA-256 (metadata only):
  `eba0ce9f13376aa193942a4765b07fc28e2f63d7a577bed9450325d8e951f4f7`.
- Public Spawner source commit:
  `fbe8d7d2620584d04ba14fa3580943bfa1448d40`.

Provide a byte-exact public source root containing `src/`. The bundled
`upstream-source-pin.json` has 69 `{path, git_blob_sha}` records for the fixed Git tree
`0a1f3074b95213b5a36b7938e2b711950f179513`. The dynamic parser verifies every
file's Git blob SHA before scanning. It does not fetch or inspect Private or
YRpp submodule contents.

```sh
python3 run_audit.py \
  --repo /path/to/RA2-Toolbox-Build \
  --ordinary-dll /path/to/ordinary/CnCNet-Spawner.dll \
  --upstream-source /path/to/pinned-public-spawner \
  --output-dir /path/to/audit-output
```

Each of the three individual audit scripts accepts the same paths. They run in
the listed order so later scripts consume the earlier JSON outputs. A
GNU-compatible C++17 compiler (`g++`; guard helper also accepts `--cxx`) and
Python 3 are required.

Expected fixed-baseline results: 39 manager write sites, 196 hooks plus 49
static patch descriptors, one CameraClamp installation-site intersection,
75 explicit public dynamic patch calls with no manager-site intersection,
and 155 guard declarations / 151 distinct guard ranges with no descriptor or
dynamic-source intersections. The shared CameraClamp entry is distinct from
its body guard, which begins after the entry's five bytes.

The actual `.syhks00` table declares size zero for
`BlowPipe_Key_BlowfishEngine_Allocation` at `0x4381DA` and
`BlowStraw_Key_BlowfishEngine_Allocation` at `0x43830A`. They are never treated
as empty intervals. The pinned public
`src/Misc/Blowfish/Hooks.Blowfish.cpp` callbacks return `0x4381F3` and
`0x438323`, respectively. The report preserves `size` and `declared_size` as
zero and assigns each a conservative `coverage_size` of 25: the entire
half-open source-skipped block `[0x4381DA, 0x4381F3)` or
`[0x43830A, 0x438323)`. This envelope is not a claim that the loader actually
overwrites 25 bytes. Every intersection uses `coverage_size` when present,
otherwise `size`; an unknown nonpositive coverage fails instead of silently
disappearing. Both physical-site and guard reports include this method and
the two descriptor records. The public source gate verifies the same 69 Git
blob identities before the complete audit runs.

The dynamic scan is a conservative inventory of conditional source branches;
it does not establish activation in a particular live game. Address-range
nonintersection is not a proof of equivalent function semantics, simulation
synchronization or multiplayer compatibility. The separate controlled Runtime
tests and research note describe the camera ownership handoff.
