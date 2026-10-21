"""Check the documented native field stores in privately supplied game EXEs.
This is byte-level evidence, not live game or MOD compatibility validation.
Usage: python unit_status_native_fields.py game1.exe [game2.exe ...]
"""
import hashlib,json,sys
from pathlib import Path
import pefile

STORES={
    'crate_armor':(0x482E79,'dd9958010000'),
    'crate_speed':(0x48306C,'dd9f80050000'),
    'crate_firepower':(0x483248,'dd9960010000'),
    'parasite_constructor_owner':(0x6292F3,'895624'),
    'parasite_constructor_victim':(0x6292F6,'894628'),
    'infect_attacker':(0x62AB27,'8b4e24'),
    'infect_victim_parasite':(0x62AB2B,'898d94060000'),
    'infect_reciprocal_victim':(0x62AB31,'896e28'),
}
results=[]
for name in sys.argv[1:]:
    path=Path(name);pe=pefile.PE(str(path));base=pe.OPTIONAL_HEADER.ImageBase
    for label,(va,expected) in STORES.items():
        actual=pe.get_data(va-base,len(bytes.fromhex(expected))).hex()
        assert actual==expected,(path.name,label,hex(va),actual,expected)
    results.append({'game_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
                    'checked_stores':len(STORES),'status':'PASS'})
assert results,'Supply private game executables; none are distributed here.'
print(json.dumps({'scope':__doc__.split('Usage:')[0].strip(),'results':results},indent=2))
