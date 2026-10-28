#!/usr/bin/env python3
"""Rendering upgrade source contracts. Not proof of game visual equivalence."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
source=(root/'external_reader/source/pixel_upgrade.h').read_text(encoding='utf-8')
decoder=(root/'external_reader/source/asset_decode.h').read_text(encoding='utf-8')
assets=(root/'external_reader/source/camera_assets.h').read_text(encoding='utf-8')
canvas=(root/'external_reader/source/native_canvas.h').read_text(encoding='utf-8')
scene=(root/'external_reader/source/radar_scene.h').read_text(encoding='utf-8')
worker=(root/'assist/source/render_upgrade_host.h').read_text(encoding='utf-8')
settings=(root/'assist/source/extra_settings_io.h').read_text(encoding='utf-8')
ui=(root/'assist/source/tactical_controls.h').read_text(encoding='utf-8')
main=(root/'assist/source/main.cpp').read_text(encoding='utf-8')
for path in ('external_reader/source/pixel_upgrade.h',
             'assist/source/render_upgrade_host.h'):
    code=re.sub(r'/\*.*?\*/|//[^\n]*','',(root/path).read_text(encoding='utf-8'),flags=re.S)
    for token in ('WriteProcessMemory','VirtualProtect(','CreateRemoteThread','gameentry::patch',
                  'SetWindowsHookEx','QueueMission(','ForceMission(','RevealMap('):
        assert token not in code,(path,token)
assert 'OriginalDAC' in source and 'FullRGB' in source
assert '(c*255u+31u)/63u' in source and 'c*4u' in source
assert 'result[0]=0;out=result;return true;' in source
assert 'enum class TwoX' in source and 'sample2x(' in source
assert 'PaletteRange range=pixelupgrade::PaletteRange::OriginalDAC' in decoder
assert 'paletteRange_=pixelupgrade::PaletteRange::OriginalDAC' in assets
assert 'pixelScaleMode==1?' in canvas and 'sx==.5&&sy==.5' in canvas
assert 'pixelScaleMode!=pixelScaleMode_' in scene
assert 'result->request.paletteRange==request.paletteRange' in worker
assert 'result->request.pixelScaleMode==request.pixelScaleMode' in worker
assert 'assets.presentationPalette(r.paletteRange' in worker
for key in ('RenderPaletteRange','RenderPixelScale'):
    assert settings.count('L"'+key+'"')==2,key
for key in ('9304','9305','renderPaletteRange','renderPixelScale'):
    assert key in ui or key in main,key
assert '24-bit RGB' in ui and '2× 轮廓插值' in ui
assert 'xBRZ 尚未集成' in ui
assert 'pixel_upgrade.cpp' in (root.parent/'.github/workflows/validate-render-upgrade.yml').read_text(encoding='utf-8') or 'pixel_upgrade' in (root.parent/'.github/workflows/validate-render-upgrade.yml').read_text(encoding='utf-8')
print('PASS: indexed color 6-to-8bit, optional native 2x scaler, worker and settings wiring; no game patch APIs in new code')
