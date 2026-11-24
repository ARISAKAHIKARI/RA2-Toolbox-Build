"""Source-level guard, not a proof of transitive or live patch ownership."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1] / "source"
ordinary = (
    "formation_bridge.h", "cooperative_bridge.h", "air_path_events_bridge.h",
    "mo_orders_bridge.h", "autoload_bridge.h", "dodge_bridge.h",
    "reverse_dodge_bridge.h", "type_select_bridge.h", "native_orders_bridge.h",
    "pointer_bridge.h", "external_orders_bridge.h", "attack_ground_bridge.h",
    "sticky_bridge.h", "auto_cs_bridge.h", "native_input_hooks.h",
    "bomb_board_bridge.h", "construction_message_bridge.h",
)
install = re.compile(r"\b(?:installRiskSite|VirtualProtect|WriteProcessMemory|SetWindowsHookEx[AW]?|gameentry::patch|installHook|codeHookRegistry\.(?:install|claim)|directGroupInstall|installDirect)\s*\(")
for name in ordinary:
    source = (root / name).read_text(encoding="utf-8-sig")
    assert not install.search(source), name
policy = (root / "hook_policy.h").read_text()
for name in ("needsCSInputPatch", "needsNativeInputPatch"):
    assert f"{name}(uint32_t){{return false;}}" in policy
settings = (root / "extra_settings_io.h").read_text()
assert "extra.privateOutMode=0;" in settings
assert "extra.nativeMegaGateway=false;" in settings
assert "shared->nativeMegaGateway=0;shared->privateOutMode=0;" in settings
ui = (root / "tactical_controls.h").read_text(encoding="utf-8-sig")
assert "EnableWindow(GetDlgItem(ui,9750),FALSE)" in ui
assert "EnableWindow(GetDlgItem(ui,9191),FALSE)" in ui
print(f"PASS: {len(ordinary)} ordinary command source files contain no direct patch installer; legacy command intercept settings retired")
