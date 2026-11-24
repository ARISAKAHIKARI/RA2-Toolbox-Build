#!/usr/bin/env python3
"""Protect native-frame-rate-independent input acceptance without loosening identity gates."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]/"source"
formation=(root/"formation_bridge.h").read_text()
host=(root/"pointer_host.h").read_text()
pointer=(root/"pointer_intent.h").read_text()
construction=(root/"construction_message_bridge.h").read_text()
cooperative=(root/"cooperative_bridge.h").read_text()
cooperative_types=(root/"cooperative_types.h").read_text()
pointer_bridge=(root/"pointer_bridge.h").read_text()
extra=(root/"extra_settings_io.h").read_text()
bridge=(root/"bridge.cpp").read_text()
formation_policy=(root/"formation.h").read_text()
dodge=(root/"dodge_bridge.h").read_text()
dodge_policy=(root/"dodge_compute.h").read_text()
autoload=(root/"autoload_bridge.h").read_text()
autoload_policy=(root/"autoload.h").read_text()
reverse=(root/"reverse_dodge_bridge.h").read_text()
reverse_policy=(root/"reverse_dodge.h").read_text()
assert "int64_t(frame)-snap.frame>6" not in formation
assert "frame<snap.frame" in formation
assert "const uint32_t hostGrace=preview?1000u:formationView.active?600u:150u;" in formation
assert "DWORD(sampleClock(sh)-sh->heartbeat)>hostGrace" in formation
assert "get<uint32_t>(0xA83D4C)!=lastHouse" in formation
assert "get<int>(0xA8B238)!=snap.session" in formation
assert "GetWindowThreadProcessId(GetForegroundWindow(),&foreground)" in formation
assert "formationSelectionMatchesPreview(bool requireRoster=false)" in formation
assert "formationActor(p,formationQuick,false)" in formation
assert "if(indexedFrame!=get<int>(0xA8ED84)||!commandRosterReady())" not in formation
assert "manualVersions.get(u.id)!=u.token" in formation
assert "frameAfter-in.frame>2" not in host
assert "frameAfter<in.frame" in host
assert "cameraX!=in.cameraX||cameraY!=in.cameraY" in host
assert "e.viewportGeneration!=viewport.generation||e.serial!=box.cancelSerial||e.match!=commandMatch()" in host
assert "DWORD(GetTickCount()-in.tick)>150" in host
assert "frame-sourceFrame<=(extended?90:8)" not in pointer
assert "frame>=sourceFrame" in pointer and "now-created" in pointer
assert "int64_t(frame)-snap.frame>6" not in construction and "frame-snap.frame>6" not in construction
assert "frame<snap.frame" in construction and "DWORD(now-policy.tick)>150" in construction
assert "sameInputRequest" in cooperative_types and "cooperative::sameInputRequest" in cooperative
assert "r.frame==frame" not in cooperative and "r.tick=now;r.sources" not in cooperative
assert "if(sh->pointer.request!=sh->pointer.ack)return;" in cooperative
assert "if(shared->pointer.request!=shared->pointer.ack)return;" in extra
assert "if(sh->pointer.request!=sh->pointer.ack)return false;" in bridge
assert "sh->cooperativeRequest.pointerRequest==b.request" in pointer_bridge and "memset(&sh->cooperativeRequest,0" in pointer_bridge
assert "formation::taskExpired(frame,j.frame,now,j.issued,(j.stage==1||j.stage==3)?formation::IssuedTaskTimeoutMs:formation::PendingTaskTimeoutMs)" in formation
assert "PendingTaskTimeoutMs=30000,IssuedTaskTimeoutMs=15000" in formation_policy
assert "frame-j.frame>1800" not in formation
assert "loading::retryDue(now,r.lastSent<0,r.lastSentAt,r.progressAt,r.retryAfterAt,incoming!=0)" in autoload
assert "frame-r.lastSent>=30" not in autoload and "frame-r.progress>=120" not in autoload
assert "RetryIntervalMs=500" in autoload_policy and "NoProgressMs=2000" in autoload_policy
assert "dodgecompute::retentionExpired(now,j.lastSeenAt)" in dodge
assert "frame-j.lastSeen>300" not in dodge
assert "RetentionMs=5000" in dodge_policy
assert "retentionExpired(now,s.sentAt)" in dodge_policy
assert "frame-s.sentFrame>=300" not in dodge_policy
assert "dodgecompute::decide(s,now)" in dodge
assert "reversedodge::withinMergeWindow(now,dodgeJobs[j].reverseAt)" in reverse
assert "reversedodge::processingExpired(now,shot->tick)" in reverse
assert "frame-dodgeJobs[j].reverseFrame" not in reverse
assert "MergeMs=200,ProcessingMs=133,ShotRetentionMs=15000" in reverse_policy
assert "frame-shot->frame>8" not in reverse
print("PASS high-FPS input guard: real-time freshness, click-scoped multi-frame cohort, safe cancellation and wall-clock task lease")
