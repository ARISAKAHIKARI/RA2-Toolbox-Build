"""Extract the exact Runtime scope implementation for synthetic I/O fixtures."""
from pathlib import Path


def function(text, needle):
    start = text.index(needle)
    brace = text.index("{", start)
    end, depth = brace + 1, 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def write_scope(out):
    source = Path(__file__).resolve().parent.parent / "source/world_frame_runtime.h"
    raw = source.read_text(encoding="utf-8")
    # Only synthetic sealed-frame tests run here. The true read-only
    # cross-frame candidate is tested separately by the native worker DLL.
    # Keep the extracted production read/scope bodies unchanged; supply
    # the missing outer globals and a deliberately unavailable candidate.
    scope = ("static worldframe::DemandLeases runtimeDemandLeases;\n"
             "static bool worldFrameBoundaryOnly=false;\n"
             "static worldframe::Snapshot worldFrameCandidate;\n"
             "static const worldframe::Snapshot* sampleWorldSource=nullptr;\n"
             "static uint32_t sampleScopeTick=0;\n"
             "static uint32_t GetTickCount(){return clockNow;}\n"
             "static bool observationalFrameReady(){return false;}\n"
             "static void rememberMissingWorldSpan(uint32_t,size_t){}\n"
             + "\n".join(function(raw, name) +
                 (";" if name.startswith("class ") else "")
                 for name in (
        "static void requestWorldRange(", "static bool readWorldSample(",
        "class SampleReadScope", "class ObservationReadScope")) + "\n")
    (out / "fixture_sample_scope_production.h").write_text(scope, encoding="utf-8")
    return scope
