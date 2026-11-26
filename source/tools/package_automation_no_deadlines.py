#!/usr/bin/env python3
"""Release a matched, checksummed, build-only Update; never claim Full resources."""
import argparse,hashlib,json,re,zipfile
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument("--root",required=True,type=Path)
parser.add_argument("--sha",required=True)
parser.add_argument("--output-name",default="RA2Toolbox-PR71-NoFixedDeadline-x64-Update.zip")
parser.add_argument("--label",default="PR71 status-backed automation")
args=parser.parse_args()
if Path(args.output_name).name!=args.output_name or not args.output_name.endswith(".zip"):
 raise SystemExit("Output name must be a relative ZIP filename")
root=args.root.resolve()
files=["RA2Toolbox.exe","GameRuntime.dll","GameRuntimeHost32.exe",
       "WebView2Loader.dll","BUILD_TARGET.json","WINDOWS_BUILD_ARTIFACTS.json"]
for name in files:
 f=root/name
 if not f.is_file() or not f.stat().st_size:
  raise SystemExit(f"Missing paired build component: {f}")
identity=json.loads((root/"BUILD_TARGET.json").read_text(encoding="utf-8-sig"))
# Keep the package contract tied to the checked-out source, not stale release constants.
protocol_header=Path(__file__).resolve().parents[1]/"assist"/"source"/"protocol.h"
protocol_matches=re.findall(r"\bconstexpr\s+uint32_t\s+Magic\s*=\s*0x[0-9a-fA-F]+\s*,\s*Version\s*=\s*(\d+)\s*;",protocol_header.read_text(encoding="utf-8-sig"))
if len(protocol_matches)!=1:
 raise SystemExit("Cannot uniquely resolve Assist protocol version from source")
expected_protocol=int(protocol_matches[0])
for key,value in {"host":"x64","runtime":"x86","broker":"x86","assist_protocol":expected_protocol}.items():
 if identity.get(key)!=value:
  raise SystemExit(f"Paired build identity mismatch {key}: {identity.get(key)!r} != {value!r}")
for leaf in ("licenses/WebView2/LICENSE.txt","licenses/WebView2/NOTICE.txt"):
 if (root/leaf).is_file():files.append(leaf)
notes=(
 f"RA2 Toolbox {args.label} candidate Update\n"
 f"Compiled commit: {args.sha}\n"
 f"x64 Host, x86 GameRuntime, x86 Broker; Assist IPC v{expected_protocol}.\n"
 "Windows compile and PE/ABI checks ran in the same workflow.\n"
 "Actual RA2, JJ, CnCNet gameplay NOT tested.\n"
 "Close game and Toolbox. Back up Assist.ini, then replace ALL EXE/DLL.\n"
 "Assets from an existing Full installation are required.\n"
)
manifest={"commit":args.sha,"host":"x64","runtime":"x86","broker":"x86",
          "assist_protocol":expected_protocol,"game_tested":False,"sha256":{}}
for name in files:manifest["sha256"][name]=hashlib.sha256((root/name).read_bytes()).hexdigest()
zip_path=root/args.output_name
with zipfile.ZipFile(zip_path,"w",zipfile.ZIP_DEFLATED,compresslevel=9) as dest:
 for name in files:dest.write(root/name,name)
 dest.writestr("UPDATE_VALIDATION.txt",notes)
 dest.writestr("UPDATE_SHA256_MANIFEST.json",json.dumps(manifest,indent=2)+"\n")
with zipfile.ZipFile(zip_path) as z:
 assert z.testzip() is None
 assert all(n in z.namelist() for n in files)
print("PACKED",zip_path,"bytes",zip_path.stat().st_size,
      "sha256",hashlib.sha256(zip_path.read_bytes()).hexdigest())
