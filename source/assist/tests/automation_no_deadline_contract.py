#!/usr/bin/env python3
"""High FPS automation safety contracts: status/identity, not snapshot age."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]/"assist"/"source"
fresh=(root/"plan_freshness.h").read_text()
assert "sourceMatch==currentMatch" in fresh and "frame>=sourceFrame" in fresh
assert "now-sourceTick" not in fresh and "frame)-sourceFrame<=" not in fresh
proof=(root/"execution_proof.h").read_text()
for needle in ("version(t.actor.id)!=t.actor.manualVersion",
               "lookup(t.actor.id)!=t.actor.pointer","t.house!=now.house"):
 assert needle in proof
assert "now.now-t.createdTick" not in proof
load=(root/"autoload_bridge.h").read_text()
assert "frame-r.frame>900" not in load and "finish(-7)" not in load
assert "manualVersions.get(r.id)!=r.manualToken" in load
assert "canEnter(r.object,r.vehicle)" in load
unload=(root/"unload_compute_bridge.h").read_text()
assert "frame-r.frame>900" not in unload
assert "manualVersions.get(r.id)!=s.token" in unload
assert "unloadSourceCurrent(r,s,house,frame)" in unload
native=(root/"native_orders_bridge.h").read_text()
# Three formerly live infantry enumerations now consume worker-published frame pages.
assert "Vec inf=vec(0xA83DE8)" not in native
assert native.count("sampleRoster(0xA83DE8,inf)") == 4
assert "int stagedCount=recentPassengerCount;" in native
assert "if(!sample||!collect()||!sample.complete())return;" in native
assert "if(!sample||sample->complete()){" in native
assert "if(complete&&(!sample||sample->complete())){" in native

for old in ("frame-r.frame>900","frame-r.frame<=900",
            "DWORD(now-queuedUnload.lastProgress)>12000",
            "task.attempted&&DWORD(now-task.attemptedAt)>3500"):
 assert old not in native
assert "manualVersions.get(r.id)!=r.manualToken" in native
print("PASS snapshot-age-independent automation with native UID/manual/house proof")
