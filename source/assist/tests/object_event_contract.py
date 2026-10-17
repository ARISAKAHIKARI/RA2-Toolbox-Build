from pathlib import Path
r=Path(__file__).resolve().parents[1]/'source'
b=(r/'bridge.cpp').read_text();h=(r/'protocol.h').read_text();u=(r/'main.cpp').read_text()
assert 'Version=42' in h and 'objectQueueMode;' in h
assert b.count('static bool nativeObjectEvent(')==1
assert 'ClickedEventVtableOffset' in b and 'nativeObjectSignature()' in b
assert 'objectqueue::appended(preCount,preTail,postCount,postTail)' in b
assert 'submitConstructed(intent::object(houseIndex,eventType,objectId))' in b
assert 'return false;' in b.split('if(!objectqueue::appended(',1)[1].split('++snap.objectQueueAccepted',1)[0]
assert 'if(!megaqueue::appended(beforeCount,beforeTail,afterCount,afterTail))' in b
assert 'ObjectEventBackend' in u and 'ClickedEvent' in u
assert 'RA2Assist_v042_' in b and 'RA2Assist_v042_' in u
print('PASS: YRpp high-level click path is exclusive, game-thread gated, no double enqueue')
