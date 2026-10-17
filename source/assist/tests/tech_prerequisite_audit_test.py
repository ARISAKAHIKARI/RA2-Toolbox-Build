import importlib.util, tempfile
from pathlib import Path
p=Path(__file__).parents[2]/'tools/tech_prerequisite_audit.py'
spec=importlib.util.spec_from_file_location('ra2tech',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
with tempfile.TemporaryDirectory() as tmp:
 f=Path(tmp)/'rules.ini'
 f.write_text('[E1]\nPrerequisite=BARRACKS\nOwner=Americans,French\nTechLevel=5\n[HIDDEN]\nTechLevel=-1\n; comment\n',encoding='utf-8')
 g=m.read_rules(f)
 r=list(m.rows(g,['E1','HIDDEN','NOTTHERE']))
 assert len(r)==3 and r[0]['Prerequisite']=='BARRACKS'
 assert r[0]['Owner']=='Americans,French'
 assert 'TechLevel=-1' in r[1]['Conclusion']
 assert r[2]['Found']=='no'
print('PASS: prerequisite rule parser, owner and missing type')
