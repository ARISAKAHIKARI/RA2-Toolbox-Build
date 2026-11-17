"""Source contract: async command capture never secretly scans a live roster.

This is structural admission coverage, NOT in-game FPS or multiplayer proof.
The paired native build and actual worker DLL tests establish compilation.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1] / "source"
bridge = (root / "bridge.cpp").read_text()
worker = (root.parent.parent / "shared/source/world_frame_command_worker_win.h").read_text()
runtime = (root / "world_frame_runtime.h").read_text()
def between(s, left, right):
    assert left in s and right in s
    return s.split(left, 1)[1].split(right, 1)[0]
assert 'static bool prepareWorldIndex(int frame)' in bridge
gate = between(bridge, "static bool prepareWorldIndex(int frame){", '\n#include "../../shared/source/key_gesture.h"')
assert gate.index('worldFrameCommandWorker.enabled()') < gate.index('indexWorld(frame)')
assert 'return refreshWorldIndex(frame,false)' in gate
fallback = between(bridge, "static bool refreshWorldIndex(int frame,bool executionGuards){", "static bool prepareWorldIndex(")
assert 'if(!executionGuards||worldFrameCommandWorker.enabled())' in fallback
assert fallback.index('if(!executionGuards||worldFrameCommandWorker.enabled())') < fallback.rindex('indexWorld(frame)')
for path in ("sticky_bridge.h", "control_object_owner_bridge.h",
             "native_mega_gateway_bridge.h", "private_outlist_bridge.h"):
    body = (root / path).read_text()
    assert "prepareWorldIndex(frame)" in body, path
    assert "indexWorld(frame)" not in body, path
assert 'worldFrameCommandWorker.take(key,graphDemand' in runtime
assert 'if(asyncCommand)' in runtime
assert runtime.index('if(asyncCommand)') < runtime.index('worldframe::CaptureGraph<Live>capture(')
assert 'commandRequestGate.defer(key,graphDemand' in runtime
assert 'commandRequestGate.submitted(key,graphDemand' in runtime
assert runtime.index('commandRequestGate.defer(key,graphDemand') < runtime.index('requests.assign(hostRequests.begin()')
assert 'commandRequestGate.reset()' in runtime
assert 'worldFrameCommandWorker.request(key,graphDemand' in runtime
assert 'CaptureGraph<decltype(checkedRead)>' in worker
assert 'THREAD_PRIORITY_BELOW_NORMAL' in worker
assert 'WaitForSingleObject(thread_,0)' in worker
assert 'latestTicket_.fetch_add(1' in worker
print("PASS async command graph and four native Hook paths never sync-scan the full live roster; worker expiration is nonblocking")
