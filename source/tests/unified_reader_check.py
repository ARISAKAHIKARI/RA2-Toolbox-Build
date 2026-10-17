from pathlib import Path
import subprocess,tempfile,os
r=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as tmp:
 exe=Path(tmp)/'unified-reader'
 subprocess.run(['g++','-std=c++17','-pthread','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(r/'winmock'),str(r/'unified_reader_check.cpp'),str(r/'unified_reader_other.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,timeout=20,env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0"}) # LeakSanitizer cannot enumerate threads in this container; handle balance is asserted.
# Guard the application integrations, not only the standalone service.
root=r.parent
for module in ['assist','unit_monitor','cursor']:
 text=(root/module/'source/main.cpp').read_text()
 assert 'external::ReadSession' in text,module
 assert 'ReadScope' not in text and 'readHub()' not in text,module
 assert 'ReadProcessMemory(' not in text,module
 for source in (root/module/'source').glob('*'):
  if source.suffix in ('.h','.cpp'):
   content=source.read_text()
   assert 'ReadScope' not in content and 'readHub()' not in content,str(source)
cursor=(root/'cursor/source/main.cpp').read_text()
assert 'cursorReader.readFresh(a,out,n)' in cursor
assert 'cursorReader.imagePath' in cursor and 'readRestricted=!cursorReader.canRead()' in cursor
print('PASS all three host modules use unified read sessions; no direct RPM or batch mutex')
