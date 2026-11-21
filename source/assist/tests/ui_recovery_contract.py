#!/usr/bin/env python3
"""Static guard for published UI settings, suspended hotkeys and stable nav."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
video=(root/"suite/source/video_settings_ui.h").read_text()
for o in ["RenderProcessing","RenderPalette","RenderPrecision","RenderDetail","RenderXbrz","RenderScale"]:
 assert "{"+o+",VideoGroup::Render" in video,o
monitor=(root/"unit_monitor/source/display_settings_ui.h").read_text()
assert "toggle(suite::PassengerBubbles" not in monitor
assert "toggle(suite::DangerPassengers" not in monitor
tactical=(root/"assist/source/tactical_controls.h").read_text()
assert "EnableWindow(extraControls[VariantKey],FALSE)" in tactical
assert "暂不可用" in tactical
assert "registerKey(1700" not in (root/"assist/source/main.cpp").read_text()
assert "if(wp==1700)return" in (root/"assist/source/hotkey_host.h").read_text()
assert "extra.variantKey" not in (root/"assist/source/type_bindings_ui.h").read_text()
app=(root/"suite/frontend/app.js").read_text()
for s in ["navGroups={0:'显示',3:'对局工具',5:'高级'}","pageStorageKey","rememberedPage()","rememberPage(index)","querySelectorAll('.nav-button')"]:
 assert s in app,s
print("PASS UI entrance recovery, unique passengers, disabled variant hotkey, persisted navigation")
