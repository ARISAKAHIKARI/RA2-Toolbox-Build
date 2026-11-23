#!/usr/bin/env python3
"""Protect native-frame-rate-independent input acceptance without loosening identity gates."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]/"source"
formation=(root/"formation_bridge.h").read_text()
host=(root/"pointer_host.h").read_text()
pointer=(root/"pointer_intent.h").read_text()
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
print("PASS high-FPS input guard: no fixed frame-span expiry; native epoch, UID, input serial, viewport and real-time bounds retained")
