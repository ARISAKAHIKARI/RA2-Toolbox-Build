"""Ordinary input is host-polled and translated to native events; special patches stay manager-owned."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]

def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8")

policy = read("assist/source/hook_policy.h")
policy_test = read("assist/tests/hook_policy.cpp")
for obsolete in ("needsCSInputPatch", "needsNativeInputPatch"):
    assert obsolete not in policy, obsolete
    assert obsolete not in policy_test, obsolete

# No other current Assist implementation or top-level test may retain the old
# feature-flag-to-input-patch dependency. Historical fixtures are intentionally
# outside this current-source contract.
for candidate in list((ROOT / "assist/source").rglob("*")) + list((ROOT / "assist/tests").glob("*")):
    if candidate.suffix not in (".h", ".hpp", ".cpp", ".py") or candidate.resolve() == Path(__file__).resolve():
        continue
    body = candidate.read_text(encoding="utf-8")
    for obsolete in ("needsCSInputPatch", "needsNativeInputPatch"):
        assert obsolete not in body, (candidate, obsolete)

# These are the ordinary input/command paths. None may install executable
# patches, allocate trampolines, or write another process's code.
ordinary_paths = (
    "assist/source/native_input_hooks.h",
    "assist/source/auto_cs_bridge.h",
    "assist/source/cooperative_bridge.h",
    "assist/source/pointer_bridge.h",
    "assist/source/native_orders_bridge.h",
    "assist/source/bridge.cpp",
    "shared/source/host_game_input.h",
    "assist/source/input_patch.h",
)
for relative in ordinary_paths:
    body = read(relative)
    for forbidden in (
        "gameentry::patch(",
        "VirtualProtect(",
        "WriteProcessMemory(",
        "makeInputTrampoline(",
        "writeInputCodeGuarded(",
        "patchNativeInput(",
    ):
        assert forbidden not in body, (relative, forbidden)

# The old registry adapter is inspection/diagnostics only.
patch_view = read("assist/source/input_patch.h")
assert "gameentry::inspect" in patch_view
assert "gameentry::markConflict" in patch_view

native = read("assist/source/native_input_hooks.h")
assert "there are no input detours" in native
assert "static void restoreNativeHooks(){}" in native
assert "loadInterfaces()" in native

cs = read("assist/source/auto_cs_bridge.h")
assert "csAllowedFor(" in cs and "csHooksTick(" in cs
assert "static void restoreCSHook(){csMaskSeq=-1;csMaskMatch=0;}" in cs

cooperative = read("assist/source/cooperative_bridge.h")
assert "static void restoreClickHooks(){}" in cooperative
assert "clickHooksTick(" in cooperative

pointer = read("assist/source/pointer_bridge.h")
assert "nativeMission(" in pointer and "nativeObjectEvent(" in pointer
assert "enqueue(" not in pointer
orders = read("assist/source/native_orders_bridge.h")
assert "nativeMission(" in orders

bridge = read("assist/source/bridge.cpp")
assert "csHooksTick(inputFlags);nativeHooksTick(inputFlags);clickHooksTick(inputFlags);" in bridge
assert "pointerTick();" in bridge and "snap.inputPatchMask=0;" in bridge

# The only Windows hooks in the input manager are public message-boundary
# hooks. They do not overwrite the game's executable code or swallow clicks.
host = read("shared/source/host_game_input.h")
hooks = re.findall(r"SetWindowsHookExW\(([^,]+)", host)
assert hooks == ["WH_GETMESSAGE", "WH_CALLWNDPROCRET"], hooks
assert "WH_KEYBOARD_LL" not in host and "WH_MOUSE_LL" not in host
assert "GetAsyncKeyState" in host and "pollPointer" in host
pointer_host = read("assist/source/pointer_host.h")
assert "GetAsyncKeyState" not in pointer_host
assert "externalReader.readFresh" in pointer_host

# Keep the deliberately opt-in patch family intact. These features still use
# GameEntryManager; removing all patch support would be a different regression.
manager = read("shared/source/game_entry_manager.cpp")
assert "VirtualProtect(site" in manager
assert "FlushInstructionCache" in manager
assert "patches.proposal" in manager and "patches.commit" in manager
for relative in (
    "assist/source/placement_bridge.h",
    "assist/source/direct_modes_bridge.h",
    "assist/source/network_sender_bridge.h",
):
    assert "gameentry::patch(" in read(relative), relative

experimental = read("shared/source/experimental_patch_policy.h")
assert "Retired scheduling/input/CS/sidebar detours remain forbidden." in experimental

print("PASS: ordinary input has no executable patch path; Win32 message hooks remain; explicit special patches stay manager-owned")
