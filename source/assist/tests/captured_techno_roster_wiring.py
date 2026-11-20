"""Bind the production native ID index to the exact CaptureGraph roster.

This regression prevents the optimization from silently becoming dead code,
or from borrowing a previous match/frame/roster after a failed sample.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]/"source"
bridge=(root/"bridge.cpp").read_text()
capture=(root.parent.parent/"shared"/"source"/"world_frame_capture.h").read_text()
assert "scratch_->technoRoster.swap(list)" in capture
assert "scratch_->technoKey=frame_.key" in capture
assert "scratch_->technoItems=capturedItems" in capture
assert "scratch_->technoCount=int(scratch_->technoRoster.size())" in capture
assert "technoKey.sameFrame(key)" in capture
assert "technoKey.serial!=key.serial" in capture
assert "technoKey.tick!=key.tick" in capture
worker=(root.parent.parent/"shared"/"source"/"world_frame_command_worker_win.h").read_text()
index=(root.parent.parent/"shared"/"source"/"world_frame_command_index.h").read_text()
refresh=bridge.split("static bool refreshWorldIndex(int frame){",1)[1].split("// Identity-index publication",1)[0]
assert "self->roster_.build(sample,job.globalVehicles,scratch.list)" in worker
assert "objects->build(sample,globalVehicles,scratch)" in index
assert "worldFrameCommandRoster.current(worldFrame.key)" in refresh
assert "worldFrameCommandRoster.objects" in refresh
assert "worldObjects.bind(worldFrameCommandRoster.objects.get()" in refresh
assert "indexWorld(" not in refresh and "CaptureGraph" not in refresh
assert "static void indexWorld(" not in bridge
assert "readLiveWorld(pointer+0x10" in bridge, "Candidate UID must be checked against live identity"
assert "manualVersions.get(id)" in bridge
print("PASS native identity projection: Worker builds immutable UID index; Runtime binds strict/candidate provenance and performs live candidate UID proof; no game-thread roster scan")
