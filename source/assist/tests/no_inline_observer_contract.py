from pathlib import Path
import re

source = Path(__file__).resolve().parents[2]
bridge = (source / 'assist/source/native_mega_gateway_bridge.h').read_text(encoding='utf-8')
code = re.sub(r'//[^\n]*|/\*.*?\*/', '', bridge, flags=re.S)
for forbidden in ('installRiskSite', 'gameentry::patch', 'cancelNative', 'nativeMission',
                  'submitConstructed', 'ForceMission', 'QueueMission', 'VirtualProtect',
                  'WriteProcessMemory', 'SetWindowsHookEx', 'actorLeaseArbiter', 'manualVersions'):
    assert forbidden not in code, f'observer may not contain {forbidden}'
assert '__declspec(naked)' not in code
assert '0x646F4E' not in code
assert 'snap.queueSent' in code and 'nativeMegaObserver.observe(sample)' in code
assert 'sampleNativeOutList' in code and 'sameRing(s.ring,after)' in code
controls = (source / 'assist/source/tactical_controls.h').read_text(encoding='utf-8')
assert '#include "no_inline_status_ui.h"' in controls
assert '原生命令只读观察（不接管自动任务）' in controls
ui = (source / 'assist/source/no_inline_status_ui.h').read_text(encoding='utf-8')
assert 'snap.hookInstalled||snap.hookConflicts' in ui
assert 'SetWindowsHookEx' not in ui
print('PASS: observer has no inline patch/command/cancellation path; diagnostic UI and conflict preservation')
