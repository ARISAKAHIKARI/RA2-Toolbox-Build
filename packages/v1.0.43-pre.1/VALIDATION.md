# 1.0.43-pre.1 release validation

## Source integration

The development baseline is `20cfcb904e4c1bbf56b05202b903d72f5097a00f` on `codex/render-local-dirty-20261105`. Secondary-camera conversion and container tracking are carried forward from the published `e00b842e81676b41e85a2db4e8ad67cf60ca424b` release source. The newer ordinary SHP rank composition remains authoritative; the older independent rank reconstruction and minimum-physical-size policy are not reintroduced.

The development branch already contains the final building-slot Alive/TimeToDie correction, Windows FPS fixture reservation fix, and Assist97 build metadata. The release recompiles the x64 host, x86 Runtime, and x86 broker together with version `1.0.43.1`.

## Local integration checks

- `watch_tracking`: 111 controlled assertions passed, including conversion, rejected successors, transport/garrison/bunker chains, and pending or fixed camera behavior.
- `native_local_ui --profile-only`: the real asynchronous Task rejects results from another scale/GPU/material profile, retains the unchanged profile, and publishes a completed matching profile without changing the previous immutable result.
- The full legacy `native_local_ui` fixture fails its cyclic-message-list assertion on the unmodified `20cfcb9` baseline as well as the integration. The baseline test, `local_ui_task.h`, and `local_ui_renderer.h` were checked against their remote Git blobs. The production Engine already isolates each UI producer, while that old assertion still expects a broken message list to reject the entire capture. This old full-suite result is not counted as a pass; the release gates use the focused profile-publication case for the actual change.

## Required release gates

Publication is bound to `main`. The push workflow builds the event's immutable commit, and the publisher requires that commit to remain the `main` head both before creating a draft and before publication. The version tag and both distribution packages must identify that same commit. Workflow dispatch on a different branch may run validation but cannot publish.

The `publish-v143-pre1.yml` workflow must complete the Linux renderer/FPS checks and Windows FPS callback, shared hardware service, local damage, shared rows, building/rank, profile-publication, and actual secondary-camera Renderer checks before publishing. PE versions, architectures, exports, ABI, dependencies, paired source identity, package membership, ZIP integrity, and downloaded release-asset SHA-256 values are verified against the same release commit.

These are build and controlled regression checks, not live-game visual, physical-GPU performance, or multiplayer acceptance. The release is a prerelease and preserves `v1.0.42` as the stable latest.

## Windows fixture address ownership

The first release workflow stopped before executing the FPS callback because a fixed `VirtualAlloc` reservation at `0x00880000` collided with an existing process reservation. The fixture now follows the repository's existing sidebar SEH test pattern: the test PE's own loader-mapped BSS owns all three absolute data ranges before process initialization. The runner verifies a fixed-base PE32/x86 test image and disables relocation only for that fixture; runtime checks require actual arena bounds, committed writable MEM_IMAGE pages, and the current module as allocation owner. Production FPS code and product ASLR settings are unchanged. Three fresh processes must each complete the original callback assertions; no failure is retried or skipped.

## Exact frozen baselines on Windows checkout

The next Windows run completed the FPS callbacks, paired product build, local damage, shared workers, building/radar/rank, and profile-publication checks, then stopped at the watch fixture's byte-digest guard before running that fixture. All five committed frozen headers match the original manifest exactly, but a Windows-style `core.autocrlf=true` checkout reproduces a digest mismatch by translating LF to CRLF. A narrowly scoped `.gitattributes` rule now preserves this frozen directory byte for byte; the expected hashes, manifest, and strict byte checks remain unchanged. An isolated checkout with automatic CRLF conversion enabled verifies all five original digests with the new rule. The watch fixture runs before the full product build so its setup and behavior are checked earlier.
