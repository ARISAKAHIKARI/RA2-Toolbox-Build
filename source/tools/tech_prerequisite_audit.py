#!/usr/bin/env python3
"""Read-only RA2/YR technology requirement audit. Does not change game files.
Usage: python tools/tech_prerequisite_audit.py rulesmd.ini --types HTNK,E1,GAPOWR
       python tools/tech_prerequisite_audit.py rulesmd.ini --all --output tech_audit.csv
Output is an analysis, NOT a safe-to-enable multiplayer patch.
"""
from __future__ import annotations
import argparse,csv,re,sys
from pathlib import Path

KEYS=('Prerequisite','PrerequisiteOverride','Owner','RequiredHouses','ForbiddenHouses','TechLevel',
      'BuildLimit','SecretHouses','StolenTech','Naval','Category','Factory','AIBuildThis')

def read_rules(path:Path):
    raw=path.read_bytes();txt=None
    for enc in ('utf-8-sig','gb18030','cp1252','utf-16'):
        try:txt=raw.decode(enc);break
        except UnicodeError:continue
    if txt is None:raise ValueError('Cannot decode rules file')
    groups={};section=None
    for line in txt.splitlines():
        line=line.split(';',1)[0].strip()
        if not line:continue
        if line.startswith('[') and ']' in line:
            section=line[1:line.index(']')].strip().upper()
            groups.setdefault(section,{})
        elif section and '=' in line:
            k,v=line.split('=',1);groups[section][k.strip().lower()]=v.strip()
    return groups

def rows(groups,filters):
    names=filters if filters is not None else list(groups)
    for typ in names:
        values=groups.get(typ.upper(),{})
        if not values:yield {'ID':typ,'Found':'no',**{k:'' for k in KEYS},'Conclusion':'type not found'};continue
        fields={k:values.get(k.lower(),'') for k in KEYS}
        limits=[]
        for key in ('Prerequisite','PrerequisiteOverride','Owner','RequiredHouses','ForbiddenHouses','TechLevel','BuildLimit','StolenTech'):
            if fields[key]:limits.append(key)
        if fields['TechLevel'].strip()=='-1':limits.insert(0,'TechLevel=-1')
        yield {'ID':typ,'Found':'yes',**fields,'Conclusion':', '.join(dict.fromkeys(limits)) or 'no obvious restriction in scanned fields'}

def main(argv=None):
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('rules',type=Path)
    g=ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--types',help='comma/space separated type IDs')
    g.add_argument('--all',action='store_true')
    ap.add_argument('--output',type=Path)
    args=ap.parse_args(argv)
    selected=None if args.all else [x.upper() for x in re.split(r'[,\s]+',args.types) if x]
    data=list(rows(read_rules(args.rules),selected))
    cols=['ID','Found']+list(KEYS)+['Conclusion']
    if args.output:
        with args.output.open('w',encoding='utf-8-sig',newline='') as f:
            w=csv.DictWriter(f,fieldnames=cols);w.writeheader();w.writerows(data)
        print(f'Wrote {len(data)} records to {args.output}')
    else:
        for r in data:print(r['ID'],r['Found'],r['Conclusion'])
    print('WARNING: a prerequisite is only one production gate; do not mutate multiplayer rules on only one client.',file=sys.stderr)

if __name__=='__main__':main()
