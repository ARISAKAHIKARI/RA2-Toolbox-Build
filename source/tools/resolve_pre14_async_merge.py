#!/usr/bin/env python3
"""Deterministic conflict resolution for the pre.14-lineage + async command merge.

Run only in an uncommitted Git merge. Preserve the stable v1.0.51 (which
contains pre.14) paths, then explicitly transplant frozen-provenance and
opt-in worker behavior. Never silently resolve a new/unrecognized conflict.
"""
from pathlib import Path
import re
import subprocess

PATTERN = re.compile(r"^<<<<<<< HEAD\n(.*?)^=======\n(.*?)^>>>>>>> [^\n]*(?:\n|$)", re.MULTILINE | re.DOTALL)

def git(*args):
    return subprocess.run(["git", *args], text=True, check=True, capture_output=True).stdout

def must_replace(content, old, new, name):
    if content.count(old) != 1:
        raise RuntimeError(f"{name}: expected one anchor, got {content.count(old)}: {old[:120]!r}")
    return content.replace(old, new, 1)

def resolve(path, source):
    path = Path(path)
    text = path.read_text(encoding="utf-8")
    blocks = PATTERN.findall(text)
    if not blocks:
        raise RuntimeError(f"{path}: missing conflict markers")
    candidates = {
        "source/assist/source/autoload.h": 1,
        "source/assist/source/autoload_bridge.h": 1,
        "source/assist/source/bridge.cpp": 4,
        "source/assist/source/compute_policy.h": 1,
        "source/assist/source/main.cpp": 1,
        "source/assist/source/native_orders_bridge.h": 3,
        "source/assist/source/world_frame_runtime.h": 1,
        "source/assist/tests/ore_harvest_handoff.cpp": 1,
        "source/assist/tests/unload_loaded_admission.py": 1,
        "source/assist/tests/world_frame_async_dll.cpp": 1,
        "source/assist/tests/world_frame_async_host.cpp": 1,
        "source/assist/tests/world_frame_core.cpp": 3,
        "source/shared/source/release_version.h": 1,
        "source/shared/source/world_frame.h": 3,
    }
    if str(path) not in candidates or len(blocks) != candidates[str(path)]:
        raise RuntimeError(f"{path}: unexpected conflict count {len(blocks)}")
    counter = [0]
    def pick(match):
        i = counter[0]
        counter[0] += 1
        stable, async_side = match.group(1), match.group(2)
        if str(path).endswith("/bridge.cpp"):
            if i == 0:
                return stable + "\n" + async_side
            if i == 1:
                return " if(!featureRestorationComplete()||!worldFramePublication.stop()||!worldFrameShadow.stop()||!worldFrameCommandWorker.stop())return;\n"
            return stable
        if str(path).endswith("/world_frame.h"):
            return async_side if i in (0, 1) else stable
        if str(path).endswith("/world_frame_async_dll.cpp"):
            # Independent native exports: retain stable low-memory probe and
            # async publisher restart diagnostic.
            return stable + "\n" + async_side
        return stable
    text = PATTERN.sub(pick, text)
    if "<<<<<<<" in text or ">>>>>>>" in text:
        raise RuntimeError(f"{path}: unresolved conflict marker")
    if str(path).endswith("/autoload.h"):
        text = must_replace(text, "Observation initial{};bool enterObserved=false;int kind=1;uint32_t ticket=0,queuedAt=0;};",
                            "Observation initial{};bool enterObserved=false;int kind=1;uint32_t ticket=0,queuedAt=0,manualToken=0;};", str(path))
    if str(path).endswith("/autoload_bridge.h"):
        text = upgrade_autoload(text)
    if str(path).endswith("/world_frame_runtime.h"):
        text = upgrade_world_capture(text, source)
    path.write_text(text, encoding="utf-8", newline="\n")
    print(f"RESOLVED {path} ({len(blocks)} conflict sections)")

def upgrade_autoload(text):
    text = must_replace(
        text,
        "static bool enterEvent(uint32_t house,uint32_t infantry,uint32_t vehicle,uint32_t* receiptID=nullptr){\n"
        " // Ordinary Enter is just a vanilla MegaMission with a destination object.\n"
        " // Use exactly the same constructor and enqueue entry as ore/attack/unload.\n"
        " return nativeMission(house,infantry,7,0,vehicle,0x34,0,0,taskarbiter::Owner::AutoLoad,receiptID);\n}",
        "static bool enterEvent(uint32_t house,uint32_t infantry,uint32_t vehicle,\n"
        " const executionproof::Ticket&proof,uint32_t* receiptID=nullptr){\n"
        " // Capture-time passenger, carrier and manual revision are verified\n"
        " // immediately before the original native event is queued.\n"
        " return submitProvenMission(house,infantry,7,0,vehicle,0x34,\n"
        "   taskarbiter::Owner::AutoLoad,proof,receiptID);\n}",
        "autoload_enter")
    text = must_replace(
        text,
        "  cancelUnloadPassenger(p.object);uint32_t ticket=0;bool sent=outgoingRoom()>0&&enterEvent(house,p.id,v.id,&ticket);",
        "  const executionproof::Ticket proof{match,house,sampleClock(sh),{p.id,p.object,tokens[i]}, {},{v.id,v.object}};\n"
        "  cancelUnloadPassenger(p.object);uint32_t ticket=0;bool sent=outgoingRoom()>0&&enterEvent(house,p.id,v.id,proof,&ticket);",
        "autoload_initial")
    text = must_replace(text,
        "job.kind=p.kind;job.ticket=ticket;",
        "job.kind=p.kind;job.manualToken=tokens[i];job.ticket=ticket;",
        "autoload_token")
    text = must_replace(text,
        "   if(enterEvent(house,r.id,r.vehicleID,&ticket)){",
        "   // Persist original human-order token across native retries.\n"
        "   const executionproof::Ticket proof{match,house,0,{r.id,r.object,r.manualToken},{},{r.vehicleID,r.vehicle}};\n"
        "   if(enterEvent(house,r.id,r.vehicleID,proof,&ticket)){",
        "autoload_retry")
    text = must_replace(text,
        "   if(view.state==loading::ReservationState::Unknown){++i;continue;}",
        "   if(manualVersions.get(r.id)!=r.manualToken){finishLoadingReservation(r,match,frame,false);loadLedger.erase(i);continue;}\n"
        "   if(view.state==loading::ReservationState::Unknown){++i;continue;}",
        "autoload_takeover")
    return text

def upgrade_world_capture(text, async_source):
    worker = async_source.split("// The game frame keeps execution authority.",1)[1].split("static uint32_t worldFrameSerial",1)[0]
    worker = "// The game frame keeps execution authority." + worker
    text = must_replace(text,
        "static WorldFramePublication worldFramePublication;\n",
        "static WorldFramePublication worldFramePublication;\n" + worker,
        "capture_worker_defs")
    text = must_replace(text,
        "static uint32_t worldFrameSerial=0;static unsigned sampleReadDepth=0;",
        "static uint32_t worldFrameSerial=0;static uint32_t worldFrameCapturedDemand=0;\n"
        "static uint64_t worldFrameCapturedRevision=0;static uint32_t runtimeDemandRevision=1;\n"
        "static unsigned sampleReadDepth=0;",
        "capture_revisions")
    text = must_replace(text,
        "runtimeWorldDemands.push_back(page);runtimeDemandLeases.retain(page,worldFrame.key.tick);",
        "runtimeWorldDemands.push_back(page);if(!++runtimeDemandRevision)++runtimeDemandRevision;runtimeDemandLeases.retain(page,worldFrame.key.tick);",
        "capture_demand_revision")
    text = must_replace(text,
        "static void retireWorldFrame(){worldFrameShadow.invalidate();",
        "static void retireWorldFrame(){worldFrameCommandWorker.invalidate();commandRequestGate.reset();worldFrameCommandRoster.invalidate();"
        "worldFrameCapturedDemand=0;worldFrameCapturedRevision=0;worldFrameShadow.invalidate();",
        "capture_retire")
    text = must_replace(text,
        "runtimeDemandBits.fill(0);runtimeMissingSpans.fill({});lastRecoverySubmitAt=0;}",
        "runtimeDemandBits.fill(0);if(!++runtimeDemandRevision)++runtimeDemandRevision;runtimeMissingSpans.fill({});lastRecoverySubmitAt=0;}",
        "capture_retire_revision")
    old = "template<class Live>static bool captureWorldFrame(Live live,const worldlife::Sample&before,uint32_t house,DWORD now,uint32_t demand,bool permitDisplayBoundary=false){"
    new = (
        "template<class Live>static bool captureAsyncCommandWorldFrame(Live live,const worldlife::Sample&before,uint32_t house,DWORD now,uint32_t demand,bool permitDisplayBoundary);\n"
        + old + "\n"
        " // The tested stable retry/recovery path remains the default. Only the\n"
        " // explicitly enabled experimental worker uses the async acquisition.\n"
        " if(worldFrameCommandWorker.enabled()&&\n"
        "    !(permitDisplayBoundary&&demand==uint32_t(worldframe::Base|worldframe::HostDisplayOnly)))\n"
        "  return captureAsyncCommandWorldFrame(live,before,house,now,demand,permitDisplayBoundary);"
    )
    text = must_replace(text,old,new,"capture_forwarding")
    a = async_source.index("template<class Live>static bool captureWorldFrame(")
    async_fn = async_source[a:]
    async_fn = async_fn.replace("template<class Live>static bool captureWorldFrame(",
                                "template<class Live>static bool captureAsyncCommandWorldFrame(",1)
    text += "\n// Isolated experimental command capture lane; never replaces the stable\n"
    text += "// verified missing-page recovery or read-only shadow diagnostics.\n"
    text += async_fn
    return text

def main():
    paths = git("diff","--name-only","--diff-filter=U").splitlines()
    if not paths:
        raise RuntimeError("No merge conflicts found; do not apply stale resolution")
    source = git("show",":3:source/assist/source/world_frame_runtime.h").replace("\r\n","\n")
    for path in paths:
        resolve(path, source)
    subprocess.run(["git","add","--",*paths], check=True)
    if git("diff","--name-only","--diff-filter=U").strip():
        raise RuntimeError("Unmerged files remain")
    print(f"RESOLVED_ALL {len(paths)}")

if __name__ == "__main__":
    main()
