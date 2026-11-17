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

def maybe_replace(content, old, new, name):
    # Git's CRLF-renormalized merge may already have integrated the feature
    # but wrapped the call differently. Check the actual semantics, not spacing.
    present = {
        "autoload_enter_signature": "const executionproof::Ticket&proof",
        "autoload_enter_dispatch": "return submitProvenMission(house,infantry",
        "autoload_initial": "enterEvent(house,p.id,v.id,proof,&ticket)",
        "autoload_token": "job.manualToken=tokens[i]",
        "autoload_retry": "enterEvent(house,r.id,r.vehicleID,proof,&ticket)",
        "autoload_takeover": "manualVersions.get(r.id)!=r.manualToken",
    }
    if new in content or (name in present and present[name] in content):
        print(f"PRESERVED_MERGED {name}")
        return content
    return must_replace(content,old,new,name)

def resolve(path, source):
    path = Path(path)
    text = path.read_text(encoding="utf-8")
    # The pre14 Ctrl+D implementation has a complete missing-page tri-state
    # admission protocol. Retain it as an atomic unit: mixing its sorted
    # iterator with the async branch's chained-hash iterator produces broken
    # roster walks and can dispatch incomplete passenger sets.
    if str(path) == "source/assist/source/native_orders_bridge.h":
        stable = git("show",":2:"+str(path)).replace("\r\n","\n")
        if "unloadCarrierKnowledge" not in stable or "unloadPassengerKnowledge" not in stable:
            raise RuntimeError("pre14 complete Ctrl+D knowledge state machine missing")
        path.write_text(stable, encoding="utf-8", newline="\n")
        print("RESOLVED_SAFE pre14 atomic Ctrl+D tri-state admission (defer async hash optimization)")
        return
    blocks = PATTERN.findall(text)
    if not blocks:
        raise RuntimeError(f"{path}: missing conflict markers")
    candidates = {
        "source/assist/source/autoload.h": 1,
        "source/assist/source/autoload_bridge.h": 1,
        "source/assist/source/bridge.cpp": 4,
        "source/assist/source/compute_policy.h": 1,
        "source/assist/source/main.cpp": 1,
        "source/assist/source/native_orders_bridge.h": 4,
        "source/assist/source/world_frame_runtime.h": 6,
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
        print("WORLD_CAPTURE_FUSION",{
            "worker":text.count("static WorldFrameCommandWorker worldFrameCommandWorker"),
            "shadow":text.count("worldFrameShadow"),
            "async_branch":text.count("if(asyncCommand)"),
            "source_revision":text.count("runtimeDemandRevision"),
            "capture_definition":text.count("template<class Live>static bool captureWorldFrame(")
        })
        if "static WorldFrameCommandWorker worldFrameCommandWorker;" not in text:
            text = upgrade_world_capture(text, source)
    path.write_text(text, encoding="utf-8", newline="\n")
    print(f"RESOLVED {path} ({len(blocks)} conflict sections)")

def upgrade_autoload(text):
    text = maybe_replace(
        text,
        "static bool enterEvent(uint32_t house,uint32_t infantry,uint32_t vehicle,uint32_t* receiptID=nullptr){",
        "static bool enterEvent(uint32_t house,uint32_t infantry,uint32_t vehicle,\n"
        " const executionproof::Ticket&proof,uint32_t* receiptID=nullptr){",
        "autoload_enter_signature")
    text = maybe_replace(
        text,
        "return nativeMission(house,infantry,7,0,vehicle,0x34,0,0,taskarbiter::Owner::AutoLoad,receiptID);",
        "return submitProvenMission(house,infantry,7,0,vehicle,0x34,taskarbiter::Owner::AutoLoad,proof,receiptID);",
        "autoload_enter_dispatch")
    text = maybe_replace(
        text,
        "  cancelUnloadPassenger(p.object);uint32_t ticket=0;bool sent=outgoingRoom()>0&&enterEvent(house,p.id,v.id,&ticket);",
        "  const executionproof::Ticket proof{match,house,sampleClock(sh),{p.id,p.object,tokens[i]}, {},{v.id,v.object}};\n"
        "  cancelUnloadPassenger(p.object);uint32_t ticket=0;bool sent=outgoingRoom()>0&&enterEvent(house,p.id,v.id,proof,&ticket);",
        "autoload_initial")
    text = maybe_replace(text,
        "job.kind=p.kind;job.ticket=ticket;",
        "job.kind=p.kind;job.manualToken=tokens[i];job.ticket=ticket;",
        "autoload_token")
    text = maybe_replace(text,
        "   if(enterEvent(house,r.id,r.vehicleID,&ticket)){",
        "   // Persist original human-order token across native retries.\n"
        "   const executionproof::Ticket proof{match,house,0,{r.id,r.object,r.manualToken},{},{r.vehicleID,r.vehicle}};\n"
        "   if(enterEvent(house,r.id,r.vehicleID,proof,&ticket)){",
        "autoload_retry")
    if "manualVersions.get(r.id)!=r.manualToken" not in text:
        anchor = "for(int i=0;i<loadLedger.count;){auto&r=loadLedger.records[i];"
        insert = anchor + "\n  // A player order cancels the original AutoLoad permission.\n" \
          "  if(manualVersions.get(r.id)!=r.manualToken){finishLoadingReservation(r,match,frame,false);loadLedger.erase(i);continue;}"
        text = must_replace(text, anchor, insert, "autoload_takeover")
    return text

def upgrade_world_capture(text, async_source):
    # Renormalized three-way merge retained both producer branches and the
    # stable missing-page recovery. Only the declarations surrounding the
    # worker were in an overlapping conflict section. Do NOT append a second
    # captureWorldFrame implementation or remove the stable recovery.
    if "if(asyncCommand)" not in text or "worldFramePublication.recovered(" not in text:
        raise RuntimeError("world capture lost either async branch or stable field recovery")
    if "worldFrameCommandWorker.take(" not in text:
        raise RuntimeError("world capture lost worker handoff")
    worker = async_source.split("// The game frame keeps execution authority.",1)[1].split("static uint32_t worldFrameSerial",1)[0]
    worker = "// The game frame keeps execution authority." + worker
    if "static WorldFrameCommandWorker worldFrameCommandWorker;" not in text:
        text = must_replace(text,
            "static WorldFramePublication worldFramePublication;\n",
            "static WorldFramePublication worldFramePublication;\n" + worker,
            "capture_worker_defs")
    # The stable function signature/identity check survived the merge; its
    # adjacent async-demand declarations were inside the pre14 conflict block.
    if "const bool asyncCommand=" not in text:
        anchor = "const bool boundaryOnly=permitDisplayBoundary&&demand==uint32_t(worldframe::Base|worldframe::HostDisplayOnly);"
        declaration = (anchor + "\n"
          " const bool asyncCommand=!boundaryOnly&&worldFrameCommandWorker.enabled();\n"
          " bool globalVehicles=false;\n"
          "#ifndef WORLD_FRAME_PLATFORM_FIXTURE\n"
          " if(asyncCommand)globalVehicles=sh&&sh->magnetronGlobal;\n"
          "#endif\n"
          " const uint32_t graphDemand=demand&~uint32_t(worldframe::HostDisplayOnly);")
        text = must_replace(text,anchor,declaration,"capture_async_demands")
    fields = (
        ("static uint32_t worldFrameCapturedDemand=0;","static uint32_t worldFrameSerial=0;"),
        ("static uint64_t worldFrameCapturedRevision=0;","static uint32_t worldFrameCapturedDemand=0;"),
        ("static uint32_t runtimeDemandRevision=1;","static uint64_t worldFrameCapturedRevision=0;")
    )
    for definition, anchor in fields:
        if definition not in text:
            text = must_replace(text,anchor,anchor+definition,"capture_fields")
    if "static void retireWorldFrame(){worldFrameCommandWorker.invalidate();commandRequestGate.reset();" not in text:
        text = must_replace(text,
            "static void retireWorldFrame(){worldFrameShadow.invalidate();",
            "static void retireWorldFrame(){worldFrameCommandWorker.invalidate();commandRequestGate.reset();worldFrameCommandRoster.invalidate();worldFrameShadow.invalidate();",
            "capture_retire")
    if "worldFrameCapturedDemand=0;worldFrameCapturedRevision=0;" not in text:
        text = must_replace(text,
            "worldFrameShadow.invalidate();worldFramePublication.invalidate();",
            "worldFrameShadow.invalidate();worldFrameCapturedDemand=0;worldFrameCapturedRevision=0;worldFramePublication.invalidate();",
            "capture_retire_revision")
    print("WORLD_CAPTURE_FUSED",{
        "worker":text.count("static WorldFrameCommandWorker worldFrameCommandWorker;"),
        "asyncBranch":text.count("if(asyncCommand)"),
        "stableRecovery":text.count("worldFramePublication.recovered("),
        "stableShadow":text.count("worldFrameShadow")
    })
    return text

def main():
    paths = git("diff","--name-only","--diff-filter=U").splitlines()
    if not paths:
        raise RuntimeError("No merge conflicts found; do not apply stale resolution")
    source = git("show",":3:source/assist/source/world_frame_runtime.h").replace("\r\n","\n")
    for path in paths:
        resolve(path, source)
    # pre14's generated fixture scope defines this state itself; the parallel
    # async-branch fixture must not define it a second time.
    fixture = Path("source/assist/tests/fixtures/tactical_frame/runtime_sample_fixture.h")
    fixture_text = fixture.read_text(encoding="utf-8")
    state = "static bool worldFrameBoundaryOnly=false;\n"
    if state in fixture_text:
        if fixture_text.count(state) != 1:
            raise RuntimeError("unexpected fixture worldFrameBoundaryOnly declarations")
        fixture.write_text(fixture_text.replace(state, ""), encoding="utf-8", newline="\n")
        print("FIXED fixture duplicate worldFrameBoundaryOnly")
    subprocess.run(["git","add","--",*paths,str(fixture)], check=True)
    if git("diff","--name-only","--diff-filter=U").strip():
        raise RuntimeError("Unmerged files remain")
    print(f"RESOLVED_ALL {len(paths)}")

if __name__ == "__main__":
    main()
