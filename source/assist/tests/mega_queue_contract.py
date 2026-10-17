from pathlib import Path
r=Path(__file__).resolve().parents[1]/'source'
b=(r/'bridge.cpp').read_text()
c=(r/'cooperative_bridge.h').read_text()
cs=(r/'auto_cs_bridge.h').read_text()
h=(r/'protocol.h').read_text()
u=(r/'main.cpp').read_text()
assert 'Version=42' in h and 'megaQueueMode;' in h
assert b.count('static bool submitMegaMission(')==1
assert b.count('if(!submitMegaMission(house,id,mission,')==1
assert 'submitConstructed(intent::mega(' in b
assert 'if(!megaqueue::appended(beforeCount,beforeTail,afterCount,afterTail))' in b and '++snap.megaQueueAccepted' in b
assert '++nativeMegaQueueCallDepth' in b and '--nativeMegaQueueCallDepth' in b
assert 'csObserver' not in cs and 'csAllowedFor' in cs
assert 'return nativeMission(lastHouse,diskID' in c and 'submitMegaMission(' not in c
assert 'MegaMissionBackend' in u and 'megaQueueCombo' in u
assert 'RA2Assist_v042_' in u and 'RA2Assist_v042_' in b
assert 'vanilla::Domain::MegaMission' in (r/'constructor_adapter.h').read_text() and '0x646E90' in b
print('PASS: one MegaMission delivery choice, UI setting, no double enqueue, own input-hook bypass')
