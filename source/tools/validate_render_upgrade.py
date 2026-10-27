"""Feature wiring checks, not proof of live-game visual equivalence."""
from pathlib import Path
import hashlib
import re
root = Path(__file__).resolve().parents[1]
new = ('shared/source/overlay_composition.h','shared/source/overlay_compositor.h',
       'external_reader/source/main_viewport.h','assist/source/render_upgrade_host.h')
for name in new:
    text = (root/name).read_text('utf-8')
    code = re.sub(r'//[^\n]*|/\*.*?\*/', '', text, flags=re.S)
    for forbidden in ('WriteProcessMemory','VirtualProtect','SetWindowsHookEx','gameentry::patch','RevealMap','QueueMission','ForceMission'):
        assert forbidden not in code, (name,forbidden)
main = (root/'assist/source/main.cpp').read_text('utf-8')
stats = (root/'unit_monitor/source/main.cpp').read_text('utf-8')
assert 'renderupgrade::update(origin,r,battlefield)' in main
assert 'renderupgrade::shutdown()' in main
assert 'overlaycompositor::host().publish({overlaycompositor::World' in main
assert 'overlaycompositor::host().publish({overlaycompositor::Statistics' in stats
assert 'renderedFrame->request.snap->tick-changedAt' in stats
settings = (root/'assist/source/extra_settings_io.h').read_text('utf-8')
for key in ('RenderUpgrade','UnifiedOverlay','RenderUpgradeLayer'):
    assert settings.count('L"'+key+'"') == 2, key
ui = (root/'assist/source/tactical_controls.h').read_text('utf-8')
for key in ('9300','9301','9302','9303','尚未合并','验证未完成'):
    assert key in ui, key
host = (root/'shared/source/overlay_compositor.h').read_text('utf-8')
assert 'WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE' in host
assert host.count('CreateWindowExW(') == 1
assert 'context.cameraX!=current.cameraX' in host
assert 'Context{}' in host and 'nodes.clear()' in host
# The rendering branch must not quietly inherit strict-branch Runtime changes.
expected = {
    'game_runtime/source/runtime.cpp': '951287cc102d55c120c31e6eecf884f0c74770ff04e306f1d62861caa93c1e81',
    'assist/source/bridge.cpp': '5ddd57a41e65d8311ab72ab1e2f4fb92a2236cc83a666dd197020c4c2380d9d4',
    'shared/source/game_entry_manager.cpp': '4f5480d228cfe31b2adc6d71e06513752ce11e77f79aafe1666f9b7b0623b105',
    'shared/source/game_entry_api.h': 'a113072ab7abe73d1667b311c5b90d99e2571aa70cff665a8a90645fdabc243b',
    'shared/source/runtime_protocol.h': 'eb480104be375885b5370692c3c90d76409fd32f2d7f95415e7df6344693a56b',
}
for name, digest in expected.items():
    data=(root/name).read_bytes().replace(b'\r\n',b'\n')
    assert hashlib.sha256(data).hexdigest()==digest, 'Unexpected Runtime change: '+name
print('PASS: render-only changes; one shared passive HWND; world/stats wired; settings persistent; unmerged functions labelled; feature-line Runtime unchanged')
