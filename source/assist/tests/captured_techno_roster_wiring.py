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
refreshStart=bridge.index("static bool refreshWorldIndex(int frame")
refreshBody=bridge.index("{",refreshStart)+1
refresh=bridge[refreshBody:bridge.index("// Identity-index publication",refreshBody)]
workerRefresh=refresh.split("if(worldFrameCommandWorker.enabled()){",1)[1].split("\n const uint32_t now",1)[0]
assert "self->stagingRoster_.build(self->staging_" in worker
assert "snapshot.read(pointer+0x10,&id,4)" in index
assert "objects->add(pointer,id)" in index
assert "commandRosterReady()" in workerRefresh
assert "worldFrameCommandRoster.objects" in workerRefresh
assert "worldObjects.bind(&worldFrameCommandRoster.objects,commandRosterReady)" in workerRefresh
assert "indexWorld(" not in workerRefresh and "CaptureGraph" not in workerRefresh
# The shipped default is Runtime-synchronous; only the explicit worker mode
# binds the worker-owned immutable roster and forbids a second live scan.
assert "static void indexWorld(int frame)" in bridge
assert "SampleReadScope sample;indexWorld(frame);complete=sample.complete();" in refresh
assert "if(worldFrameCommandWorker.enabled())return refreshWorldIndex(frame,false);" in bridge
assert "indexWorld(frame);" in bridge
assert "readLiveWorld(pointer+0x10" in bridge, "Candidate UID must be checked against live identity"
assert "manualVersions.get(id)" in bridge
print("PASS native identity projection: Worker builds immutable UID index; Runtime binds strict/candidate provenance and performs live candidate UID proof; no game-thread roster scan")
