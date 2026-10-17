from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as temp:
 exe=Path(temp)/'winmock_check'
 subprocess.run(['g++','-std=c++17','-pthread','-I'+str(root/'winmock'),str(root/'winmock_check.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: process snapshot/fallback, lazy Win32 region queries, permission-denial isolation')
