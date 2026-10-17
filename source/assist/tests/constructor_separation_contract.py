from pathlib import Path
s=Path(__file__).resolve().parents[1]/'source'
b=(s/'bridge.cpp').read_text()
adapter=(s/'constructor_adapter.h').read_text()
intent=(s/'command_intent.h').read_text()
catalog=(s/'vanilla_constructor_catalog.h').read_text()
entry=(s/'entry_gate.h').read_text()
assert 'static bool submitConstructed(' in b
assert b.count('call<void>(0x4C')==0, 'no gameplay call sites embed FillEvent addresses'
assert b.count('return enqueue(e);')==0
assert 'submitConstructed(intent::mega(' in b
assert 'submitConstructed(intent::object(' in b
assert 'decisions::planProduction(view,order)' in b
assert 'submitConstructed(intent::superWeapon(' in b
for addr in ('0x4C6650','0x4C66C0','0x4C6720','0x4C6780','0x4C6970','0x4C6AE0','0x4C6B60'):
 assert addr in catalog
assert 'Exposure::ProbeOnly' in catalog and 'if(constructors::admit(order)' in b
assert 'GameFrame' not in intent and '0x4C6' not in intent
assert 'decisions::shouldRepair(' in b
assert 'ReadProcessMemory' not in adapter and 'OutList' not in adapter
assert 'entrygate::admit(boundary)' in b
assert 'return enqueue(packet);' in b
print('PASS: Reader/Planner/constructor/entry ownership and 7 YRpp constructors')
