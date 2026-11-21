#!/usr/bin/env python3
"""Release a matched, checksummed, build-only Update; never claim Full resources."""
import argparse,hashlib,json,zipfile
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument("--root",required=True,type=Path)
parser.add_argument("--sha",required=True)
args=parser.parse_args()
root=args.root.resolve()
files=["RA2Toolbox.exe","GameRuntime.dll","GameRuntimeHost32.exe",
       "WebView2Loader.dll","BUILD_TARGET.json","WINDOWS_BUILD_ARTIFACTS.json"]
for name in files:
 f=root/name
 if not f.is_file() or not f.stat().st_size:
  raise SystemExit(f"Missing paired build component: {f}")
identity=json.loads((root/"BUILD_TARGET.json").read_text(encoding="utf-8-sig"))
for key,value in {"host":"x64","runtime":"x86","broker":"x86","assist_protocol":112}.items():
 if identity.get(key)!=value:
  raise SystemExit(f"Paired build identity mismatch {key}: {identity.get(key)!r} != {value!r}")
for leaf in ("licenses/WebView2/LICENSE.txt","licenses/WebView2/NOTICE.txt"):
 if (root/leaf).is_file():files.append(leaf)
notes=(
 "RA2 Toolbox PR71 status-backed automation candidate Update\n"
 f"Compiled commit: {args.sha}\n"
 "x64 Host, x86 GameRuntime, x86 Broker; Assist IPC v112.\n"
 "Windows compile and PE/ABI checks ran in the same workflow.\n"
 "Actual RA2, JJ, CnCNet gameplay NOT tested.\n"
 "Close game and Toolbox. Back up Assist.ini, then replace ALL EXE/DLL.\n"
 "Assets from an existing Full installation are required.\n"
)
manifest={"commit":args.sha,"host":"x64","runtime":"x86","broker":"x86",
          "assist_protocol":112,"game_tested":False,"sha256":{}}
for name in files:manifest["sha256"][name]=hashlib.sha256((root/name).read_bytes()).hexdigest()
zip_path=root/"RA2Toolbox-PR71-NoFixedDeadline-x64-Update.zip"
with zipfile.ZipFile(zip_path,"w",zipfile.ZIP_DEFLATED,compresslevel=9) as dest:
 for name in files:dest.write(root/name,name)
 dest.writestr("UPDATE_VALIDATION.txt",notes)
 dest.writestr("UPDATE_SHA256_MANIFEST.json",json.dumps(manifest,indent=2)+"\n")
with zipfile.ZipFile(zip_path) as z:
 assert z.testzip() is None
 assert all(n in z.namelist() for n in files)
print("PACKED",zip_path,"bytes",zip_path.stat().st_size,
      "sha256",hashlib.sha256(zip_path.read_bytes()).hexdigest())
