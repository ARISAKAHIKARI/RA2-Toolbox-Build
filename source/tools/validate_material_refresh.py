"""Guard the external rendering extension; behavioral tests are separate."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
for path in ('external_reader/source/material_motion.h','external_reader/source/material_blend.h',
             'external_reader/source/viewport_canvas.h','assist/source/render_upgrade_host.h'):
    code=re.sub(r'//[^\n]*|/\*.*?\*/','',(root/path).read_text(encoding='utf-8'),flags=re.S)
    for token in ('WriteProcessMemory','VirtualProtect','CreateRemoteThread','gameentry::','QueueMission(',
                  'ForceMission(','SendInput(','SetWindowsHookEx','RandomClass','srand(','rand('):
        assert token not in code,(path,token)
host=(root/'assist/source/render_upgrade_host.h').read_text(encoding='utf-8')
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
assets=(root/'external_reader/source/camera_assets.h').read_text(encoding='utf-8')
assert 'fineRadians(pose.bodyFine' in assets and 'detail_?2:1' in assets
print('PASS: observed material timeline and device-pixel detail integrated; 5–500 Hz target, persistent options, epoch invalidation, final visibility mask; no game-write APIs')
