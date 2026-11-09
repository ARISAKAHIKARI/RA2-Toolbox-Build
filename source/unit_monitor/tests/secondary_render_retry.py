#!/usr/bin/env python3
"""Evaluate the production submission predicate for rejected secondary frames."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--output", required=True)
parser.add_argument("--label", default="current")
args = parser.parse_args()
source = Path(__file__).resolve().parents[1] / "source/main.cpp"
text = source.read_text()
start = text.index("if(secondaryDirty||") + 3
end = text.index("{", start) - 1
predicate = text[start:end]
out = Path(args.output)
out.mkdir(parents=True, exist_ok=True)
cpp = out / ("secondary_render_retry_" + args.label + ".cpp")
code = """#include <cstdint>
#include <memory>
#include <cassert>
#include <iostream>
using DWORD=uint32_t;
struct Request{DWORD tick;uint64_t serial;}; struct Frame{Request request;};
static bool due(bool secondaryDirty, std::shared_ptr<Frame> secondaryRenderedFrame, DWORD now, DWORD secondaryLastSubmit,uint64_t secondarySubmitSerial){return """ + predicate + r""";}
static std::shared_ptr<Frame> accepted(DWORD tick,uint64_t serial){return std::make_shared<Frame>(Frame{{tick,serial}});}
int main(){
 assert(due(false,accepted(50,1),500,100,2)); // rejected replacement with an older accepted frame must retry
 assert(!due(false,accepted(50,1),349,100,2)); // keep existing retry spacing
 assert(!due(false,accepted(100,2),10000,100,2)); // unchanged accepted output must not redraw
 assert(due(true,accepted(50,1),100,100,2)); // content edits still submit immediately
 assert(due(false,nullptr,350,100,2)); // initial allocation/paint failure
 assert(!due(false,nullptr,349,100,2));
 assert(due(false,accepted(0xfffffe00,1),0x100,0xffffff00,2)); // uint32 timer wrap
 assert(due(false,accepted(100,1),350,100,2)); // two submissions can share one millisecond clock tick
 std::cout<<"PASS secondary retry: 8 submission and recovery cases\n";
}
"""
cpp.write_text(code)
exe = cpp.with_suffix("")
commands = [["g++", "-std=c++17", "-D_GLIBCXX_ASSERTIONS", "-fsanitize=undefined", "-fno-sanitize-recover=all", str(cpp), "-o", str(exe)], [str(exe)]]
receipts = []
for i, command in enumerate(commands):
    result = subprocess.run(command, capture_output=True, text=True)
    log = result.stdout + result.stderr
    (out / ("secondary_render_retry_" + args.label + "_" + str(i) + ".log")).write_text(log)
    receipts.append({"command": command, "exit_code": result.returncode, "stdout": result.stdout, "stderr": result.stderr})
    print(log, end="")
    if result.returncode:
        break
record = {"source": str(source), "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(), "test_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), "predicate": predicate, "verification_level": "controlled_execution_of_extracted_production_predicate", "not_verified": ["Win32 allocation failure injection", "actual monitor drawing"], "commands": receipts}
(out / ("secondary_render_retry_" + args.label + ".receipt.json")).write_text(json.dumps(record, indent=2) + "\n")
raise SystemExit(0 if all(r["exit_code"] == 0 for r in receipts) else 1)
