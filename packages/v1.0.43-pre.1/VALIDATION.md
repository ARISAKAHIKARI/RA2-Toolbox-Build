# 1.0.43-pre.1 release validation

## Source integration

The development baseline is `20cfcb904e4c1bbf56b05202b903d72f5097a00f` on `codex/render-local-dirty-20261105`. Secondary-camera conversion and container tracking are carried forward from the published `e00b842e81676b41e85a2db4e8ad67cf60ca424b` release source. The newer ordinary SHP rank composition remains authoritative; the older independent rank reconstruction and minimum-physical-size policy are not reintroduced.

The development branch already contains the final building-slot Alive/TimeToDie correction, Windows FPS fixture reservation fix, and Assist97 build metadata. The release recompiles the x64 host, x86 Runtime, and x86 broker together with version `1.0.43.1`.

## Local integration checks

- `watch_tracking`: 111 controlled assertions passed, including conversion, rejected successors, transport/garrison/bunker chains, and pending or fixed camera behavior.
- `native_local_ui --profile-only`: the real asynchronous Task rejects results from another scale/GPU/material profile, retains the unchanged profile, and publishes a completed matching profile without changing the previous immutable result.
- The full legacy `native_local_ui` fixture fails its cyclic-message-list assertion on the unmodified `20cfcb9` baseline as well as the integration. The baseline test, `local_ui_task.h`, and `local_ui_renderer.h` were checked against their remote Git blobs. This old full-suite result is not counted as a pass; the release gates use the focused profile-publication case for the actual change.

## Required release gates

The `publish-v143-pre1.yml` workflow must complete the Linux renderer/FPS checks and Windows FPS callback, shared hardware service, local damage, shared rows, building/rank, profile-publication, and actual secondary-camera Renderer checks before publishing. PE versions, architectures, exports, ABI, dependencies, paired source identity, package membership, ZIP integrity, and downloaded release-asset SHA-256 values are verified against the same release commit.

These are build and controlled regression checks, not live-game visual, physical-GPU performance, or multiplayer acceptance. The release is a prerelease and preserves `v1.0.42` as the stable latest.
