"""Guard the production precision/resolution wiring; pixel behavior has C++ tests."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
paths = ('external_reader/source/color_precision.h', 'external_reader/source/palette_recovery.h',
         'external_reader/source/resolution_sampling.h', 'external_reader/source/camera_raster.h',
         'external_reader/source/camera_assets.h', 'assist/source/render_upgrade_host.h')
for path in paths:
    code = re.sub(r'//[^\n]*|/\*.*?\*/', '', (root/path).read_text(encoding='utf-8'), flags=re.S)
    for forbidden in ('WriteProcessMemory', 'VirtualProtect(', 'ChangeDisplaySettings',
                      'CreateRemoteThread', 'QueueMission(', 'ForceMission(', 'SendInput('):
        assert forbidden not in code, (path, forbidden)
host = (root/'assist/source/render_upgrade_host.h').read_text(encoding='utf-8')
assert 'presentationResolution(r.view.width,r.view.height,r.width,r.height)' in host
assert host.index('assets.presentationResolution(') < host.index('const uint64_t assetRevision=assets.revision()')
assert 'presentationColorPrecision(r.colorPrecision!=0)' in host
policy=(root/'assist/source/render_presentation.h').read_text(encoding='utf-8')
assert 'renderpresentation::scrollUsable(*result,request,GetTickCount(),shift)' in host
assert 'a.colorPrecision==b.colorPrecision' in policy
assert 'true,r.colorPrecision!=0)' in host
scene = (root/'external_reader/source/radar_scene.h').read_text(encoding='utf-8')
assert 'if(precision!=precision_){reset();precision_=precision;}' in scene
assert 'pending_.precision(precision_)' in scene
assert re.search(r'detailKey_,\s*precision_,\s*pixelScaleMode,', scene)
assert 'baseScene_->pixels.empty()?gpuBaseTiles_:nullptr,cpuBaseFallback_' in scene
viewport = (root/'external_reader/source/viewport_canvas.h').read_text(encoding='utf-8')
# The selected precision reaches CPU composition and GPU recording, including
# raw native terrain plans whose color pixels are generated only on fallback.
assert 'surface_.precision(precision)' in viewport
assert 'gpuRecord_.begin(w,h,std::move(basePixels),baseDepth_,precision,' in viewport
assert '!cpuBaseFallback||!cpuBaseFallback(generatedBase)' in viewport
canvas = (root/'external_reader/source/native_canvas.h').read_text(encoding='utf-8')
assert 'surface.precision(precision_)' in canvas
settings = (root/'assist/source/extra_settings_io.h').read_text(encoding='utf-8')
assert settings.count('L"RenderColorPrecision"') == 2
assets = (root/'external_reader/source/camera_assets.h').read_text(encoding='utf-8')
assert 'presentationRasterScale()' in assets and 'voxel-presentation-resolution-unsupported' in assets
assert 'if(!colorPrecision_||paletteRange_!=pixelupgrade::PaletteRange::FullRGB)return;' in assets
assert 'external::paletterecovery::promote(live,candidate)' in assets
ci = (root.parent/'.github/workflows/validate-render-upgrade.yml').read_text(encoding='utf-8')
for test in ('color_precision', 'color_scene', 'palette_recovery', 'resolution_sampling'):
    assert test in ci
print('PASS: live request precision, terrain/object propagation, source palette gate, observed resolution and CI wiring; no new game-write APIs')
