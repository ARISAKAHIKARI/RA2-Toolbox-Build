#!/usr/bin/env python3
"""Compile and exercise the current production drag helpers, without Win32 UI stubs."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--output", required=True)
args = parser.parse_args()
out = Path(args.output)
out.mkdir(parents=True, exist_ok=True)
source = Path(__file__).resolve().parents[1] / "source/main.cpp"
text = source.read_text()
first = text.index("static int secondaryTravelUnit(")
last = text.index("static SecondaryDrag secondaryDrag;", first)
unit = """#include <algorithm>
#include <cstdint>
#include <iostream>
#include <cassert>
using LONG=int32_t;
struct POINT { LONG x,y; };
struct RECT { LONG left,top,right,bottom; };
""" + text[first:last] + r"""
static void expect(SecondaryDrag d, POINT cursor, int x, int y) {
 auto result=secondaryDragSettings(d,cursor);
 assert(result.x==x && result.y==y);
}
int main(){
 SecondaryDrag d{};d.work={1920,0,2720,600};d.origin={2000,20};d.cursor={2000,20};d.width=1200;d.height=800;
 expect(d,{2100,30},0,0); // oversized old panel after monitor change
 d.width=400;d.height=200;d.origin={1920,0};d.cursor={100,100};
 expect(d,{300,300},5000,5000); // normal center
 expect(d,{-100000,-100000},0,0);
 expect(d,{100000,100000},10000,10000);
 d.work={-1920,-1080,0,0};d.width=960;d.height=540;d.origin={-1920,-1080};
 expect(d,{580,370},5000,5000); // negative-coordinate monitor
 d.work={0,0,400,200};d.width=400;d.height=200;d.origin={0,0};
 expect(d,{99999,99999},0,0); // exact-fit panel: zero travel
 d.x=6789;d.y=4321;d.work={10,10,10,20};
 expect(d,{99999,99999},6789,4321); // invalid work area preserves preference
 unsigned cases=0;
 for(int width:{1,100,400,401,1200}) for(int height:{1,100,200,201,800})
 for(int cx:{-100000,-500,0,100,500,100000}) for(int cy:{-100000,-500,0,100,500,100000}) {
  d.work={-400,-200,0,0};d.origin={-350,-150};d.cursor={0,0};d.width=width;d.height=height;
  auto result=secondaryDragSettings(d,{cx,cy});
  assert(result.x>=0&&result.x<=10000&&result.y>=0&&result.y<=10000);
  if(width>=400)assert(result.x==0);
  if(height>=200)assert(result.y==0);
  ++cases;
 }
 std::cout<<"PASS secondary drag: 7 boundary cases + "<<cases<<" range cases\n";
}
"""
cpp = out / "secondary_drag_current.cpp"
cpp.write_text(unit)
exe = out / "secondary_drag_current"
commands = [["g++", "-std=c++17", "-D_GLIBCXX_ASSERTIONS", "-fsanitize=undefined", "-fno-sanitize-recover=all", str(cpp), "-o", str(exe)], [str(exe)]]
receipts = []
for i, command in enumerate(commands):
    result = subprocess.run(command, capture_output=True, text=True)
    log = result.stdout + result.stderr
    (out / f"secondary_drag_current_{i}.log").write_text(log)
    receipts.append({"command": command, "exit_code": result.returncode, "stdout": result.stdout, "stderr": result.stderr})
    print(log, end="")
    if result.returncode:
        raise SystemExit(result.returncode)
record = {"source": str(source), "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(), "test_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), "generated_cpp_sha256": hashlib.sha256(cpp.read_bytes()).hexdigest(), "verification_level": "controlled_execution_of_extracted_production_functions", "not_verified": ["Windows event sequence", "actual multi-monitor DPI layout"], "commands": receipts}
(out / "secondary_drag_current.receipt.json").write_text(json.dumps(record, indent=2) + "\n")
