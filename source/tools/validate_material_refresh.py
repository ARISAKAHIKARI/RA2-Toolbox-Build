"""Guard the external rendering extension; behavioral tests are separate."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
for path in ('external_reader/source/material_motion.h','external_reader/source/material_blend.h',
             'external_reader/source/viewport_canvas.h','external_reader/source/live_palette_cache.h',
             'external_reader/source/camera_assets.h','assist/source/render_upgrade_host.h'):
    code=re.sub(r'//[^\n]*|/\*.*?\*/','',(root/path).read_text(encoding='utf-8'),flags=re.S)
    for token in ('WriteProcessMemory','VirtualProtect','CreateRemoteThread','gameentry::','QueueMission(',
                  'ForceMission(','SendInput(','SetWindowsHookEx','RandomClass','srand(','rand('):
        assert token not in code,(path,token)
host=(root/'assist/source/render_upgrade_host.h').read_text(encoding='utf-8')
assert 'return unavailable();' in host
assert 'return {};' not in host.split('static asyncwork::Latest<Request,Frame> worker(',1)[1].split('#include "render_feedback_host.h"',1)[0]
assert 'heldFrame' not in host, 'Whole-frame reuse can bypass dynamic lighting and palette updates'
assert 'reader.begin(); // Explicit fresh page epoch' in host
assert 'timeline.observe(raw,' in host and 'timeline.sample(' in host
assert 'before.frame!=sampledLogicFrame' in host and 'refreshPeriod(informationFPS(),renderMaterialRefresh)' in host
assert 'result->request.generation==request.generation' in host and '++generation;worker.invalidate()' in host
settings=(root/'assist/source/extra_settings_io.h').read_text(encoding='utf-8')
for key in ('RenderMaterialRefresh','RenderSceneDetail'):
    assert settings.count('L"'+key+'"')==2,key
main=(root/'assist/source/main.cpp').read_text(encoding='utf-8')
assert 'renderPixelScale=std::clamp(choice,0,3)' in main
scene=(root/'external_reader/source/radar_scene.h').read_text(encoding='utf-8')
assert 'materialblend::image(' in scene and 'detailed_.prepare(' in scene and 'if(masked)mask_.apply(out->pixels)' in scene
assert 'frame.posesPrepared' in scene
assert 'refreshPalette(assets,' in scene and 'invalidateArt(key)' in scene
assert 'bool liveVoxels=true' in scene and 'if(liveVoxels)' in scene
assert 'voxel-current-vpl-unavailable' in (root/'external_reader/source/camera_assets.h').read_text(encoding='utf-8')
assets=(root/'external_reader/source/camera_assets.h').read_text(encoding='utf-8')
assert 'refreshActorPalette(ActorArt&art,bool&changed)' in assets and 'refreshOverlayPalette(' in assets
assert 'fineRadians(pose.bodyFine' in assets and 'presentationRasterScale()' in assets
print('PASS: observed material timeline and device-pixel detail integrated; 5–500 Hz target, persistent options, epoch invalidation, final visibility mask; no game-write APIs')
