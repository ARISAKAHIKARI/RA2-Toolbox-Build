#!/usr/bin/env python3
"""Real file/cache-worker recovery after an initial unavailable game directory."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument("--output",required=True)
parser.add_argument("--label",default="current")
args=parser.parse_args()
out=Path(args.output);out.mkdir(parents=True,exist_ok=True)
source=Path(__file__).resolve().parents[1]/"source/icon_cache.h"
text=source.read_text()
# Access synchronization state only to deterministically wait for the failed
# scope result. Worker methods and conditionals are otherwise byte-for-byte.
assert text.count("class IconCache {\n")==1
header=out/("icon_cache_"+args.label+".h")
header.write_text(text.replace("class IconCache {\n","class IconCache {\npublic:\n",1))
cpp=out/("icon_cache_reopen_"+args.label+".cpp")
cpp.write_text('#include "'+header.name+'"\n'+r"""
#include <cassert>
#include <iostream>
#include <cstring>
#include <thread>
using namespace std::chrono_literals;
template<class Pred> bool until(Pred pred,int ms){auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(ms);do{if(pred())return true;std::this_thread::sleep_for(5ms);}while(std::chrono::steady_clock::now()<end);return pred();}
int main(int argc,char**argv){
 assert(argc==2);std::filesystem::path root=argv[1],game=root/"initially-missing"/"gamemd.exe",disk=root/"icons";
 um::IconCache cache;cache.open(game,disk);
 bool failed=until([&]{std::lock_guard<std::mutex> lock(cache.mutex);return cache.file.filename()=="0.bin"&&!cache.ready;},1500);
 assert(failed); // The first failed scope operation has actually completed.
 std::filesystem::create_directories(game.parent_path());std::ofstream(game)<<"synthetic game path, no executable";
 cache.open(game,disk); // Same identity must be allowed to retry after failure.
 um::Type t{};t.category=0;std::strcpy(t.id,"SYNTH");std::strcpy(t.cameo,"synthetic.shp");t.iconReady=1;t.icon[0]=7;
 um::Snapshot snap{};snap.paletteReady=1;snap.palette[7]=0xffaabbcc;
 assert(cache.remember(t,snap));
 bool ready=until([&]{std::lock_guard<std::mutex> lock(cache.mutex);return cache.ready&&cache.scopeHash!=0;},1500);
 if(!ready){std::cerr<<"FAIL same-path open never retries after failed scope\n";return 3;}
 assert(until([&]{std::lock_guard<std::mutex> lock(cache.mutex);return std::filesystem::is_regular_file(cache.file);},1500));
 um::Type recovered=t;recovered.iconReady=0;recovered.icon[0]=0;um::Snapshot recoveredSnapshot{};
 assert(cache.restore(recovered,recoveredSnapshot));assert(recovered.icon[0]==7&&recoveredSnapshot.palette[7]==0xffaabbcc);
 uint64_t generation=0;{std::lock_guard<std::mutex> lock(cache.mutex);generation=cache.generation;}
 cache.open(game,disk);{std::lock_guard<std::mutex> lock(cache.mutex);assert(generation==cache.generation);}
 std::cout<<"PASS failed scope -> same-path reconnect -> cache persistence/restore; healthy reconnect keeps generation\n";
}
""")
exe=cpp.with_suffix("")
commands=[["g++","-std=c++17","-O1","-pthread","-D_GLIBCXX_ASSERTIONS","-I",str(source.parent),str(cpp),"-o",str(exe)]]
with tempfile.TemporaryDirectory(prefix="icon-reopen-",dir=out) as fixture:
    commands.append([str(exe),fixture]);receipts=[]
    for i,command in enumerate(commands):
        r=subprocess.run(command,capture_output=True,text=True);receipts.append({"command":command,"exit_code":r.returncode,"stdout":r.stdout,"stderr":r.stderr});(out/("icon_cache_reopen_"+args.label+"_"+str(i)+".log")).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end="")
        if r.returncode:break
record={"source":str(source),"source_sha256":hashlib.sha256(source.read_bytes()).hexdigest(),"test_sha256":hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),"verification_level":"Linux real production cache worker and filesystem; access visibility changed only for deterministic state observation","not_verified":["Windows MoveFileEx behavior","actual game asset caches"],"commands":receipts}
(out/("icon_cache_reopen_"+args.label+".receipt.json")).write_text(json.dumps(record,indent=2)+"\n")
raise SystemExit(0 if all(r["exit_code"]==0 for r in receipts) else 1)
