# Optional DLSS 5 renderer

`RA2NeuralRenderer.exe` is a separate x64 process. The toolbox exchanges complete world frames through a bounded, pointer-free shared mapping. NVIDIA SDK objects and code are not linked into the xBRZ-based host or the 32-bit game runtime.

The helper uses the official NGX Vulkan Neural Rendering API supplied by NVIDIA's public RTX Remix `rtx-remix-ngx_sdk_dlnr@2` package. It creates an actual DLSS-NR feature, uploads color, current-to-previous motion, reverse orthographic depth and a confidence mask into Vulkan images, evaluates the feature, waits for completion and reads its output. It does not modify the game or replace DLSS 5 with an image filter. This software contains source code provided by NVIDIA Corporation.

The input is display-referred SDR `0xAARRGGBB`. Color is explicitly converted into `R16G16B16A16_SFLOAT`; it is not uploaded as incorrectly ordered RGBA bytes. Confidence zero, transparent and partially transparent pixels retain their exact input value. UI, original feedback and the high refresh cursor are composed after neural processing by the host.

The first version uploads and reads back an image in the helper in addition to the host's existing GPU readback. It is not a GPU-sharing or direct-presentation implementation. Successful compilation, transport fixtures and unsupported-device fallback do not prove actual inference, game image quality or hardware performance. A supported NVIDIA device and driver are required for those checks.

## Build

Prepare the official development dependencies outside the source tree:

```text
python source/tools/fetch_neural_sdk.py --output C:/build/ra2-neural-sdk --sevenzip "C:/Program Files/7-Zip/7z.exe"
```

Set `RA2_NGX_SDK` to that SDK directory and `RA2_NEURAL_OUTPUT` to the desired absolute EXE path, then run `source/neural_renderer/build-msvc.cmd`. The helper requires Visual Studio 2022 C++ tools and the Windows SDK. Its NGX static loader uses the MSVC runtime, so this component uses `/MT`; the toolbox host continues using its existing Zig build. No NVIDIA headers or model binaries are vendored into this source directory.

`--probe` checks the installed runtime and driver without claiming that a frame was processed. It exits with code 2 when unavailable. The normal `--ipc` interface is private to the toolbox and accepts inherited handles only.

See [RUNTIME_SETUP.md](RUNTIME_SETUP.md) for optional runtime installation. Do not ship a development SDK or copy arbitrary DLLs into the game directory. End-user packages containing the compiled helper also retain the NVIDIA SDK license.

## Verification

- `assist/tests/neural_pixels.cpp`: fixed ABI and plane limits, independent expected colors, channel order, half conversion, invalid color rejection and exact protected pixels.
- `assist/tests/neural_transport_windows.cpp`: real Win64 child processes and shared mappings using a controlled fixture, stale result rejection, cancellation before/after submission, transient protected views and sticky backend failure. It does not execute a neural model.
- Official runtime installer tests validate pinned bytes, NVIDIA Authenticode, replacement, damaged archive refusal and rollback without executing the DLL.
