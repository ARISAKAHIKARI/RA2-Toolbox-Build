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
policy=(root/"shared/source/feature_availability.h").read_text()
assert "VariantSelection=true" in policy
assert "EnableWindow(extraControls[VariantKey],featureavailability::VariantSelection)" in tactical
assert 'label(L"按当前武器选择同类单位",20,306)' in tactical
assert "当前武器变体尚未接入统一采样" not in tactical
assert "featureavailability::VariantSelection&&LOBYTE(extra.variantKey)" in (root/"assist/source/main.cpp").read_text()
assert "if(wp==1700){if(!featureavailability::VariantSelection)return;" in (root/"assist/source/hotkey_host.h").read_text()
bindings=(root/"assist/source/type_bindings_ui.h").read_text()
assert bindings.count("extra.variantKey")==bindings.count("featureavailability::VariantSelection&&")==2
app=(root/"suite/frontend/app.js").read_text()
shell=(root/"suite/source/modern_console_main.h").read_text()
assert "message.pageIndex" in app and "store.navigate(target.module,target.category,index)" in app
assert 'L"PageIndex"' in shell and 'result.category=result.pageIndex==8?2:result.pageIndex-2' in shell
print("PASS UI entrance recovery, unique passengers, restored active variant hotkey, persisted navigation")
