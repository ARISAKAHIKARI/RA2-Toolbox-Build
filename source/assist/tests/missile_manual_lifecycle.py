#!/usr/bin/env python3
"""Execute the real missile manual-ownership helpers with controlled raw reads.

Only platform/world observations are replaced. This does not execute a game,
native command constructor, Windows hook, or multiplayer event transport.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
source = repo / "source/assist/source/missile_bridge.h"
raw = source.read_bytes()
text = raw.decode()
begin = text.index("struct MissileSend ")
end = text.index("static int missileRecord", begin)
actual = text[begin:end]
fixture = r'''
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
struct Shared {bool missileEnabled=true;};static Shared shared;static Shared*sh=&shared;
static uint32_t lastHouse=70000;static int clockFrame=100;
struct {struct {uint32_t match=1;int frame=100;}key;}worldFrame;
static bool sampleReady=true,sampleComplete=true,rosterReadable=true;
static std::unordered_map<uint32_t,uint32_t> memory;
static std::vector<uint32_t> roster;
struct Vec{int count=0;};
static bool read(uint32_t at,void*out,size_t size){
 if(at==0xA8ED84&&size==4){std::memcpy(out,&clockFrame,4);return true;}
 auto it=memory.find(at);if(it==memory.end()){sampleComplete=false;return false;}
 std::memcpy(out,&it->second,size);return true;
}
template<class T>T get(uint32_t at){T value{};read(at,&value,sizeof value);return value;}
struct SampleReadScope{
 SampleReadScope(){sampleComplete=sampleReady;}
 explicit operator bool()const{return sampleReady;}
 bool complete()const{return sampleReady&&sampleComplete;}
};
static bool sampleRoster(uint32_t,Vec&v,int=16384){if(!rosterReadable){sampleComplete=false;return false;}v.count=int(roster.size());return true;}
static uint32_t item(const Vec&,int i){return roster[size_t(i)];}
static uint32_t worldByID(uint32_t id){for(uint32_t p:roster)if(get<uint32_t>(p+0x10)==id)return p;return 0;}
'''
cases = r'''
static int checks=0,failures=0;
static void check(bool ok,const char*label){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<label<<'\n';}}
static uint32_t unit(uint32_t id,int mission=2,uint32_t target=90000){
 uint32_t p=100000+id*1024;memory[p+0x10]=id;memory[p+0x6C]=100;memory[p+0x21C]=lastHouse;
 memory[p+0xAC]=uint32_t(mission);memory[p+0xB4]=uint32_t(-1);memory[p+0x2B4]=target;memory[p+0x5A4]=mission==2?91000:0;
 roster.push_back(p);return p;
}
static void reset(){memory.clear();roster.clear();missileManualCount=missileSendCount=0;missileManualBlocked=false;missileManualPruneMatch=0;missileManualPruneFrame=-1;++worldFrame.key.match;clockFrame=worldFrame.key.frame=100;sampleReady=sampleComplete=rosterReadable=true;shared.missileEnabled=true;}
static void fill(){for(uint32_t id=1;id<=1024;++id)pauseMissileForManual(id,unit(id));}
int main(){
 reset();fill();check(missileManualCount==1024,"fill reaches existing bounded capacity");
 roster.clear();clockFrame=worldFrame.key.frame=101;uint32_t fresh=unit(2000);
 pauseMissileForManual(2000,fresh);
 check(missileManuallyOwned(fresh),"dead historical roster entries cannot drop a later manual command");
 check(missileManualCount==1,"complete next-frame absence reclaims dead records");
 std::cout<<"dead_history count="<<missileManualCount<<" new_owned="<<missileManuallyOwned(fresh)<<'\n';

 reset();fill();clockFrame=worldFrame.key.frame=112;
 for(int id=1;id<=1024;++id){uint32_t p=100000+uint32_t(id)*1024;memory[p+0xAC]=5;memory[p+0x2B4]=memory[p+0x5A4]=0;}
 fresh=unit(2000);pauseMissileForManual(2000,fresh);
 check(missileManuallyOwned(fresh),"completed idle tasks outside the next candidate scan release capacity");
 check(missileManualCount==1,"idle completion keeps only the fresh manual task");

 reset();fill();clockFrame=worldFrame.key.frame=112;
 roster.erase(roster.begin());fresh=unit(2000);pauseMissileForManual(2000,fresh);
 check(missileManuallyOwned(fresh),"one proven dead task makes one slot for a new command");
 check(missileManualCount==1024,"reclamation does not evict any active task");
 int kept=0;for(uint32_t id=2;id<=1024;++id)kept+=missileManuallyOwned(100000+id*1024);
 check(kept==1023,"all surviving moving manual tasks stay protected");

 reset();fill();clockFrame=worldFrame.key.frame=101;roster.clear();rosterReadable=false;
 fresh=unit(2000);pauseMissileForManual(2000,fresh);
 check(missileManualCount==1024,"unreadable roster is not proof of retirement");

 reset();fill();clockFrame=worldFrame.key.frame=101;memory.erase(roster[12]+0x10);
 fresh=unit(2000);pauseMissileForManual(2000,fresh);
 check(missileManualCount==1024,"partial roster UID read does not retire valid manual records");

 reset();fill();roster.clear();clockFrame=101;worldFrame.key.frame=100;
 fresh=unit(2000);pauseMissileForManual(2000,fresh);
 check(missileManualCount==1024,"same-frame sealed roster cannot retire a later captured manual cohort");

 reset();fill();clockFrame=worldFrame.key.frame=111;
 for(int id=1;id<=1024;++id){uint32_t p=100000+uint32_t(id)*1024;memory[p+0xAC]=5;memory[p+0x2B4]=memory[p+0x5A4]=0;}
 fresh=unit(2000);pauseMissileForManual(2000,fresh);
 check(missileManualCount==1024,"idle protection does not end before the existing twelve-frame boundary");

 reset();uint32_t p=unit(1);pauseMissileForManual(1,p);clockFrame=worldFrame.key.frame=112;
 check(missileManuallyOwned(p),"elapsed time alone does not cancel a moving manual order");
 memory[p+0xAC]=5;memory[p+0x2B4]=memory[p+0x5A4]=0;
 check(!missileManuallyOwned(p,false)&&missileManualCount==1,"read-only probe preserves the original observe=false contract");
 check(!missileManuallyOwned(p)&&missileManualCount==0,"observed idle terminal retires its record");
 shared.missileEnabled=false;pauseMissileForManual(1,p);check(!missileManualCount,"disabled feature does not create manual tracking");
 std::cout<<(failures?"FAIL: ":"PASS: ")<<checks<<" checks, "<<failures<<" failures; extracted production manual-ownership helpers\n";
 return failures?1:0;
}
'''
args.output.mkdir(parents=True, exist_ok=True)
generated = args.output / "missile_manual_lifecycle.cpp"
generated.write_text(fixture + "\n// BEGIN EXACT PRODUCTION EXTRACT\n" + actual + "\n// END EXACT PRODUCTION EXTRACT\n" + cases)
binary = args.output / "missile_manual_lifecycle"
commands = [
    ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", str(generated), "-o", str(binary)],
    [str(binary)],
]
records = []
for index, command in enumerate(commands):
    completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    log = args.output / ("build.log" if index == 0 else "run.log")
    log.write_text(completed.stdout)
    records.append({"argv": command, "cwd": str(Path.cwd()), "exit_code": completed.returncode,
                    "log": str(log), "log_sha256": hashlib.sha256(log.read_bytes()).hexdigest()})
    print(json.dumps({"step": index, "exit_code": completed.returncode, "output": completed.stdout}, ensure_ascii=False))
    if index == 0 and completed.returncode:
        break
summary = {"verification_level": "controlled_execution", "source": str(source),
           "source_sha256": hashlib.sha256(raw).hexdigest(),
           "test_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
           "generated_sha256": hashlib.sha256(generated.read_bytes()).hexdigest(),
           "extract_sha256": hashlib.sha256(actual.encode()).hexdigest(), "commands": records}
(args.output / "result.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
raise SystemExit(records[-1]["exit_code"])
