"""Compile source-extracted Enter seat accounting against deterministic synthetic memory.

The full nativeMission/Windows order router is validated in its own test suite;
this narrowly tests the real three capacity functions without outdated API
stubs from historical MO fixtures. No running game is used.
"""
from pathlib import Path
import argparse,hashlib,subprocess,tempfile
HERE=Path(__file__).resolve().parent
SRC=HERE.parent/"source"
def function(source,signature):
 start=source.index(signature)
 brace=source.index("{",start)
 end=brace+1
 depth=1
 while depth:
  depth+=(source[end]=="{")-(source[end]=="}")
  end+=1
 return source[start:end]
def main():
 p=argparse.ArgumentParser();p.add_argument("--cxx",default="g++")
 p.add_argument("--zig",type=Path);p.add_argument("--out",type=Path)
 a=p.parse_args()
 dest=a.out or Path(tempfile.mkdtemp(prefix="shared-enter-capacity-"))
 dest.mkdir(parents=True,exist_ok=True)
 production=(SRC/"cooperative_bridge.h").read_text(encoding="utf-8-sig")
 signatures=("static bool cooperativePendingActor(","static int cooperativePendingSlots(","static int entryReserved(")
 selected="\n".join(function(production,s) for s in signatures)
 before=(HERE/"shared_enter_capacity.cpp").read_text(encoding="utf-8")
 assert before.count("/* INSERT_EXTRACTED */")==1
 code=before.replace("/* INSERT_EXTRACTED */",selected)
 (dest/"shared_enter_capacity.generated.cpp").write_text(code,encoding="utf-8")
 targets=(("x86_64","x86") if a.zig else ("portable",))
 for target in targets:
  exe=dest/("shared-seat-"+target+(".exe" if a.zig else ""))
  build=([str(a.zig.resolve()),"c++","-target",target+"-windows-gnu"] if a.zig else [a.cxx])
  build+=["-std=c++17","-O1","-UNDEBUG","-I",str(SRC),str(dest/"shared_enter_capacity.generated.cpp"),"-o",str(exe)]
  result=subprocess.run(build,capture_output=True,text=True)
  (dest/(target+"-build.log")).write_text(result.stdout+result.stderr,encoding="utf-8")
  print(result.stdout+result.stderr,end="")
  result.check_returncode()
  run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=45)
  (dest/(target+"-run.log")).write_text(run.stdout+run.stderr,encoding="utf-8")
  print(run.stdout+run.stderr,end="")
  run.check_returncode()
 print("PASS source SHA256",hashlib.sha256(production.encode()).hexdigest())
if __name__=="__main__":
 main()
