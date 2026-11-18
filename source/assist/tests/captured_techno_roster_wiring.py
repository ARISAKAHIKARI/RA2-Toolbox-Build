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
assert "sampleReadDepth&&worldFrameReady&&!worldFrameBoundaryOnly" in bridge
assert "worldFrameCaptureScratch.capturedTechnoRoster(worldFrame.key,all.items,all.count)" in bridge
assert "if(all.count&&!read(all.items,pointers,size_t(all.count)*4))" in bridge, "Missing fallback"
assert "for(int i=0;i<all.count;++i){const uint32_t p=roster[i]" in bridge
assert "if(!read(p+0x10,&id,4)){complete=false;continue;}" in bridge
assert "if(!complete||(sampleReadDepth&&!sampleReadComplete)){unknown();return;}" in bridge
print("PASS native identity projection: same-frame sealed roster reused, stale/incomplete samples fall back, all per-UID checks unchanged")
