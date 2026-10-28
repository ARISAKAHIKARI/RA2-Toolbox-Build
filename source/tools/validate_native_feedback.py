from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
paths=['external_reader/source/native_feedback.h','external_reader/source/feedback_selection.h','external_reader/source/feedback_cursor.h','external_reader/source/feedback_scene.h','external_reader/source/feedback_orders.h','assist/source/render_feedback_host.h','assist/source/render_upgrade_host.h']
for path in paths:
    text=(root/path).read_text(encoding='utf-8')
    text=re.sub(r'/\*.*?\*/|//[^\n]*','',text,flags=re.S)
    for token in ['WriteProcessMemory','VirtualProtect','CreateRemoteThread','gameentry::','SendInput(','QueueMission(','ForceMission(','SetWindowsHookEx','ShowCursor(','HideCursor(','SetCursor(','RandomClass','srand(','rand(']:
        assert token not in text,(path,token)
core=(root/paths[0]).read_text(encoding='utf-8')
host=(root/'assist/source/render_feedback_host.h').read_text(encoding='utf-8')
render=(root/'assist/source/render_upgrade_host.h').read_text(encoding='utf-8')
compositor=(root/'shared/source/overlay_compositor.h').read_text(encoding='utf-8')
scene=(root/'external_reader/source/radar_scene.h').read_text(encoding='utf-8')
orders=(root/'external_reader/source/feedback_orders.h').read_text(encoding='utf-8')
assert 'nf::actionLines(read,r.view' in host and 'foundationStencil.capture(read,r.view' in host
assert '0x843108' in orders and '0xB0EA80' in orders and 'actionColor(read,8,attack)' in orders and 'actionColor(read,3,move)' in orders
assert '0x8A03FC' in orders and 'copy?0x04:0x02' in orders and 'image.pixels[' in orders
assert 'nativeCutouts' in host and 'paintInteractions(primitives,strokes' in host
assert '0xD90' in core and '0x11CF' in core
assert 'current.same(cursor.mouse)' in host and 'cursorCache.capture(cursorRead' in host
assert 'cursorWorker' not in host and 'cursorCalls>2048' in host
assert 'GetCursorInfo' in host and 'externalCursor()' in host
assert 'bool replaceGroup(' in compositor and 'std::lock_guard<std::mutex>lock(mutex);' in compositor
assert 'bool feedbackBounds(' in scene and 'bool feedbackVisibility(' in scene
assert 'layers.push_back({SelectionLayer,200' in host and 'layers.push_back({CursorLayer,300' in host
assert 'replaceGroupAt(request.context,{overlaycompositor::RenderUpgrade,feedback::SelectionLayer,feedback::CursorLayer}' in render
assert 'if(fresh&&feedbackReady)' in render and 'layers.clear();' in render
assert '待补' in render and '近似' in render and '实机' in render
print('PASS: native feedback is read-only; exact mouse metadata/hotspot; atomic scene/input group; fallback and unfinished labels')
