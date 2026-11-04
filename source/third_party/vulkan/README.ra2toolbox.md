# Vulkan declarations

These are unmodified official KhronosGroup/Vulkan-Headers files from tag
`v1.3.290`, commit `b379292b2ab6df5771ba9870d53cf8b2c9295daf`.
`UPSTREAM.json` records original paths, Git blob SHA-1, byte sizes and SHA-256.
The original Git blob hash of every bundled upstream file was verified after
materialization. Original license files and per-file SPDX notices are retained.

The two `include/vulkan` headers are placed in this directory; upstream
`include/vk_video` is placed under `vk_video/` so the original relative includes
resolve without a global include-path change. No upstream file contents changed.
The video declarations are transitive dependencies of the official core header,
not an enabled video feature.

Define `VK_NO_PROTOTYPES` before including `vulkan_core.h`. The experiment uses
the official Vulkan ABI declarations and dynamically loads Vulkan 1.0 entry
points. This directory contains no Vulkan loader, driver, SDK binary or linked
runtime dependency. Availability, fallback and GPU behavior belong to the host
implementation and are not established by vendoring these declarations.

The core header uses Apache-2.0. Preserve `LICENSE.md`, `LICENSES/Apache-2.0.txt`,
`LICENSES/MIT.txt` and each upstream header's SPDX notice in source distributions.
