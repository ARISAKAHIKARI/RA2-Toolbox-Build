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
    scope = "static worldframe::DemandLeases runtimeDemandLeases;\n" + "\n".join(function(raw, name) for name in (
        "static void requestWorldRange(", "static bool readWorldSample(", "class SampleReadScope")) + ";\n"
    (out / "fixture_sample_scope_production.h").write_text(scope, encoding="utf-8")
    return scope
