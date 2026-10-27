from pathlib import Path
import re

source = Path(__file__).resolve().parents[2]
manager = source / "shared/source/game_entry_manager.cpp"
m = manager.read_text(encoding="utf-8")
api = (source / "shared/source/game_entry_api.h").read_text(encoding="utf-8")
transport = (source / "shared/source/runtime_protocol.h").read_text(encoding="utf-8")
assert "constexpr uint32_t Version=4;" in api
assert "inline constexpr bool CodeWritesAllowed=false;" in api
assert "Version=5," in transport and "RA2Runtime_v5_" in transport
assert "GameCodeWritePolicyInfo[3]={1,0,0}" in m
reject = "if(r->operation==Operation::Patch)return int32_t(Result::PatchDenied);"
assert reject in m
assert m.index(reject) < m.index("const DWORD tid=GetCurrentThreadId();")
assert m.count("Operation::Patch") == 1
for forbidden in ("applyPatch(", "mayApplyPatch(", "patches.commit(", "memcpy(", "memmove("):
    assert forbidden not in m, f"strict manager retains executable writer: {forbidden}"

# Reject known code-write APIs in ALL authored production modules, including
# the common manager. This is a source regression check, not a live .text proof.
roots = [source / name / "source" for name in (
    "assist", "shared", "game_runtime", "external_reader", "cursor", "unit_monitor", "suite")]
forbidden_names = (
    "VirtualProtect", "VirtualProtectEx", "NtProtectVirtualMemory", "ZwProtectVirtualMemory",
    "WriteProcessMemory", "NtWriteVirtualMemory", "ZwWriteVirtualMemory",
    "FlushInstructionCache", "NtFlushInstructionCache", "ZwFlushInstructionCache",
    "writeInputCodeGuarded", "makeInputTrampoline",
)
pattern = re.compile(r"\b(?:" + "|".join(forbidden_names) + r")\b")
# Strip comments but retain string literals used for named dynamic imports.
comment_tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/', re.S)
def without_comments(text):
    return comment_tokens.sub(lambda m: "" if m.group(0).startswith(("//", "/*")) else m.group(0), text)
scanned = 0
for root in roots:
    assert root.is_dir(), f"missing production audit root: {root}"
    for path in root.rglob("*"):
        if path.suffix not in {".h", ".hpp", ".cpp", ".c"}:
            continue
        text = path.read_text(encoding="utf-8", errors="strict")
        # Preserve string literals too: a named dynamic import is not a loophole.
        hits = pattern.findall(without_comments(text))
        assert not hits, f"Forbidden executable-write API in {path}: {hits}"
        scanned += 1

# Preserve the first native-event routes; refusing patches must not replace
# them with direct local mission/target setters or extra event injection.
autoload = (source / "assist/source/autoload_bridge.h").read_text(encoding="utf-8")
bridge = (source / "assist/source/bridge.cpp").read_text(encoding="utf-8")
ore = (source / "assist/source/external_orders_bridge.h").read_text(encoding="utf-8")
assert "return nativeMission(house,infantry,7,0,vehicle,0x34" in autoload
assert "submitConstructed(" not in autoload
assert "static bool repairEvent(uint32_t house,uint32_t id){return nativeObjectEvent(get<int>(house+0x30),0x15,id);}" in bridge
assert "repairEvent(house,id)" in bridge and "Unified game-thread command gateway" in bridge
assert "nativeMission(house,o.unit,o.mission,o.target,o.destination,o.destinationTag" in ore
assert "gameentry::patch(" not in autoload and "gameentry::patch(" not in ore

ui = (source / "assist/source/no_inline_status_ui.h").read_text(encoding="utf-8")
controls = (source / "assist/source/tactical_controls.h").read_text(encoding="utf-8")
assert '#include "no_inline_status_ui.h"' in controls
for marker in ("未实现", "待实测", "待审计", "修改探索数据", "联机有同步风险", "不会写回代码", "不接管自动任务"):
    assert marker in ui, f"missing feature limitation label: {marker}"
assert "tests/mocks" not in (source / "BUILD_ZIG.cmd").read_text(encoding="utf-8")
print(f"PASS: strict no-code writer in {scanned} source files; install AND restore denied; ABI isolated; native routes retained; unfinished UI labelled")
