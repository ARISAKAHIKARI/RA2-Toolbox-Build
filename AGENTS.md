# RA2 Toolbox continuity

User requirement (2026-10-21): record every new reverse-engineering finding from now on. Do not backfill earlier work solely for completeness.

Before investigating an engine behavior, search `source/research/INDEX.md`, its linked notes, and the persistent `RA2-Reverse-Engineering-Notes.sqlite` notebook. Resolve the notebook by its Library identity/name; materialize it for SQLite queries and replace that same file when adding findings. If unavailable, use the repository notes and preserve new findings here for subsequent writeback.

For each new finding record: question, supported executable SHA-256, function addresses/ABI/field offsets, evidence and test paths, conclusion, rejected hypotheses and unresolved points. Distinguish disassembly, controlled execution and real-game observations. Keep a concise human-readable note and index in the repository as well. Never publish users' private game executables or raw dumps.

Do not claim controlled Win32/game fixtures prove Windows visual, audio-device, projectile-physics or multiplayer behavior. UI feedback may use verified native local voice entry points; automatic continuations must not repeat user-command acknowledgement.
