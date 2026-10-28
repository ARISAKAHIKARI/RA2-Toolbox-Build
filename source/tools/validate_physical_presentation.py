#!/usr/bin/env python3
"""Read-only native 4K output integration policy, not proof of GPU presentation."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
paths=[
    "shared/source/physical_presentation.h",
    "shared/source/physical_window.h",
    "external_reader/source/pixel_upgrade.h",
    "external_reader/source/native_canvas.h",
    "assist/source/render_upgrade_host.h",
]
for path in paths:
    code=(root/path).read_text(encoding="utf-8")
    code=re.sub(r"//[^\n]*|/\*.*?\*/","",code,flags=re.S)
    for forbidden in (
        "WriteProcessMemory", "VirtualProtect(", "SetWindowsHookEx",
        "ChangeDisplaySettings", "SetWindowPos(game", "MoveWindow(game",
        "SetProcessDpiAwareness", "SetThreadDpiAwarenessContext",
        "QueueMission(", "ForceMission(", "PresentEx(", "DirectDrawCreate",
    ):
        assert forbidden not in code,(path,forbidden)
geometry=(root/"shared/source/physical_presentation.h").read_text(encoding="utf-8")
probe=(root/"shared/source/physical_window.h").read_text(encoding="utf-8")
canvas=(root/"external_reader/source/native_canvas.h").read_text(encoding="utf-8")
samples=(root/"external_reader/source/pixel_upgrade.h").read_text(encoding="utf-8")
host=(root/"assist/source/render_upgrade_host.h").read_text(encoding="utf-8")
main=(root/"assist/source/main.cpp").read_text(encoding="utf-8")
settings=(root/"assist/source/extra_settings_io.h").read_text(encoding="utf-8")
ui=(root/"assist/source/tactical_controls.h").read_text(encoding="utf-8")
ci=(root.parent/".github/workflows/validate-render-upgrade.yml").read_text(encoding="utf-8")
assert "Status::SuspiciousModeSwitch" in geometry
assert "Status::AmbiguousDpi" in geometry and "Status::WindowOnly" in geometry
assert "EnumDisplaySettingsExW" in probe and "MonitorFromWindow" in probe
assert "GetMonitorInfoW" in probe and "coversMonitor" in probe
assert "sampleAdaptive(" in samples and "adaptivePixel(" in samples
assert "pixelScaleMode==3" in canvas and "pixelupgrade::sampleAdaptive(" in canvas
assert "renderPhysicalOutput" in host
assert "physical.status==physicalpresentation::Status::Ready" in host
assert "if(physicalBlocked)visible=false" not in host
assert "physicalpresentation::renderPath(physical,renderPhysicalOutput)" in host
assert "renderPath==physicalpresentation::RenderPath::EnhancedPhysicalPixels" in host
assert "疑似独占/显示模式切换：覆盖层保持" in host
assert "仅停用 4K 增强" in host
assert "const bool physicalBlocked" not in host
assert "LegacyClientPixels" in geometry and "EnhancedPhysicalPixels" in geometry
assert "keepExistingOverlay(" in geometry
assert "physicalpresentation::inspect(game" in host
assert "renderPixelScale==0?3:renderPixelScale" in host
assert "4K 物理重绘已停用" in host and "疑似独占全屏" in host
assert "9306" in main and "renderPhysicalOutput=on" in main
assert settings.count('L"RenderPhysicalOutput"')==2
assert "renderPixelScale=std::clamp" in settings and ",0,3)" in settings
assert "9306" in ui and "自适应 1.25–2×" in ui
assert "不使用 NTSC/Bloom" in ui
assert "physical_presentation_windows" in ci
assert "physical_presentation;" in ci
print("PASS: physical 1.5x output requires matched monitor; exclusive/DPI switch retains client-sized overlays; no monitor/game-code mutation; CI wired")
