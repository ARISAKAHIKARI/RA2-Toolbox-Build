from pathlib import Path

source = Path(__file__).resolve().parents[2]
manager = source / "shared/source/game_entry_manager.cpp"
registry = source / "assist/source/hook_registry.h"

m = manager.read_text(encoding="utf-8")
r = registry.read_text(encoding="utf-8")

assert "constexpr bool NoInlineGamePatches = true;" in r
assert "inline bool mayApplyPatch(Transition transition)" in r
assert "return transition==Transition::Restore || (!NoInlineGamePatches && transition==Transition::Install);" in r
assert "transition==Transition::Restore" in r
assert "mayApplyPatch(transition)" in m
assert m.index("mayApplyPatch(transition)") < m.index("if(transition==assist::hookregistry::Transition::Install)")
assert "VirtualProtect(site,r.length,PAGE_EXECUTE_READWRITE" in m
assert "memcpy(site,r.after,r.length)" in m
assert "FlushInstructionCache(GetCurrentProcess(),site,r.length)" in m

# The only permitted writer of game-image executable bytes in these runtime
# sources is the common GameEntryManager. Other source files must not bypass it.
roots = [
    source / "assist/source",
    source / "shared/source",
    source / "game_runtime/source",
    source / "external_reader/source",
    source / "cursor/source",
    source / "unit_monitor/source",
    source / "suite/source",
]
for root in roots:
    for path in root.rglob("*"):
        if path.suffix not in {".h", ".hpp", ".cpp", ".c"} or path == manager:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        forbidden = (
            "VirtualProtect(",
            "WriteProcessMemory(",
            "FlushInstructionCache(",
            "writeInputCodeGuarded(",
            "makeInputTrampoline(",
        )
        hits = [token for token in forbidden if token in text]
        assert not hits, f"Direct executable-write path outside GameEntryManager: {path}: {hits}"

# These first migration candidates construct and enqueue native events; none
# needs to install its own game-image patch.
autoload = (source / "assist/source/autoload_bridge.h").read_text(encoding="utf-8")
assert "nativeMission(" in autoload and "enterEvent(" in autoload
bridge = (source / "assist/source/bridge.cpp").read_text(encoding="utf-8")
assert "static bool repairEvent(" in bridge and "nativeObjectEvent(" in bridge
assert "repairEvent(house,id)" in bridge
assert "Unified game-thread command gateway" in bridge

# First migration wave must continue to use the shared native event gateway.
autoload = (source / "assist/source/autoload_bridge.h").read_text(encoding="utf-8")
assert "return nativeMission(house,infantry,7,0,vehicle,0x34" in autoload
assert "nativeMission(" in autoload and "submitConstructed(" not in autoload
assert "static bool repairEvent(uint32_t house,uint32_t id){return nativeObjectEvent(get<int>(house+0x30),0x15,id);}" in bridge
assert "repairEvent(house,id)" in bridge
ore = (source / "assist/source/external_orders_bridge.h").read_text(encoding="utf-8")
assert "nativeMission(house,o.unit,o.mission,o.target,o.destination,o.destinationTag" in ore
assert "gameentry::patch(" not in autoload
assert "gameentry::patch(" not in ore
print("PASS: no-inline patch gate, sole executable writer, native-event migration candidates")
