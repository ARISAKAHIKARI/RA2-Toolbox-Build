"""Exercise shipped Host tactical functions with the real ProcessReader and IPC.

The existing world-frame platform fixture replaces Win32 I/O only. Route and
construction bodies are extracted unchanged; presentation/keyboard/queue are
explicit seams. This is a CPU regression, not a Windows or game-process test.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SRC = HERE.parent / "source"
SOURCE = SRC.parents[1]


def function(text, signature):
    start = text.index(signature)
    while "(" in signature:
        param = text.index("(", start)
        end, depth = param + 1, 1
        while depth:
            depth += (text[end] == "(") - (text[end] == ")")
            end += 1
        if not text[end:].lstrip().startswith(";"):
            break
        start = text.index(signature, end)
    brace = text.index("{", end if "(" in signature else start)
    end, depth = brace + 1, 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def generated():
    core = (HERE / "world_frame_core.cpp").read_text()
    memory = core[core.index("namespace fixture {\nsize_t checks"):core.index("void overlap()")]
    platform = core[core.index("#define WINMOCK_IMAGE_FILES"):core.index("\nnamespace assist {\nstruct FixtureShared")]
    platform = platform.replace('#include "../../external_reader/source/process_reader.h"',
                                '#include "' + str(SOURCE / "external_reader/source/process_reader.h") + '"')
    platform = platform.replace('#include "../source/world_lifecycle.h"', '')
    windows = (SOURCE / "tests/winmock/windows.h").read_text()
    driver = (HERE / "world_frame_core_test.py").read_text()
    windows += driver.split('windows += r"""', 1)[1].split('"""', 1)[0]

    air = (SRC / "air_path_host.h").read_text()
    route = []
    for name in ("struct Point", "struct Job", "enum class AircraftState"):
        route.append(function(air, name) + ";")
    route.append("static std::vector<Job> jobs;static size_t cursor=0;static bool ended=false;static DWORD sampled=0;")
    route.append("static bool active(){return routeActive;}static bool held(){if(++heldCalls==2&&duringHeld){auto callback=std::move(duringHeld);duringHeld={};callback();}return routeHeld;}")
    for name in ("static void reset(", "template<class T>static T get(", "static void cancel(",
                 "static bool crossed(", "static AircraftState currentAircraft(", "static void service("):
        route.append(function(air, name))

    build = (SRC / "construction_host.h").read_text()
    declarations = "\n".join(function(build, name) + ";" for name in ("struct Choice", "struct PaintRequest", "struct Painted"))
    panel = []
    for name in ("template<class T>static T get(", "static int encode(", "static int absKind(",
                 "static int typeIndex(", "static uint32_t typeArray(", "static uint32_t typePointer(",
                 "static uint32_t factory(", "static bool placed(", "static int typeSlot(",
                 "static bool technologyRequested(", "template<class C>static bool loadForecast(",
                 "static void collectChoices(", "static bool samePaintWorld(", "static void resetPaint(",
                 "static PaintRequest capturePaint(", "static void requestPaint("):
        panel.append(function(build, name))
    return {
        "windows.h": windows,
        "host_platform.h": memory + "}\n" + platform,
        "host_air_production.h": "\n\n".join(route),
        "host_panel_declarations.h": declarations,
        "host_panel_production.h": "\n\n".join(panel),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    out = args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix="host-tactical-frame-"))
    out.mkdir(parents=True, exist_ok=True)
    fragments = generated()
    for name, text in fragments.items():
        (out / name).write_text(text)
    binary = out / "host-tactical-frame-sampling"
    flags = ["-std=c++17", "-O2", "-UNDEBUG", "-pthread", "-I", str(out), "-I", str(SRC)]
    build = subprocess.run([args.cxx, *flags, str(HERE / "host_tactical_frame_sampling.cpp"), "-o", str(binary)], capture_output=True, text=True)
    (out / "build.log").write_text(build.stdout + build.stderr)
    print(build.stdout + build.stderr, end="")
    build.check_returncode()
    run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
    (out / "run.log").write_text(run.stdout + run.stderr)
    print(run.stdout + run.stderr, end="")
    run.check_returncode()
    paths = [SRC / name for name in ("air_path_host.h", "construction_host.h", "construction_catalog.h", "construction_policy.h", "protocol.h")]
    paths += [SOURCE / "external_reader/source/process_reader.h", SOURCE / "shared/source/world_frame.h",
              SOURCE / "shared/source/world_frame_transport.h", HERE / "world_frame_core.cpp", HERE / "world_frame_core_test.py",
              HERE / "host_tactical_frame_sampling.cpp", Path(__file__).resolve()]
    result = {"pass": True, "scope": __doc__, "flags": flags, "output": run.stdout,
              "windows_executed": False, "game_process_tested": False, "multiplayer_tested": False,
              "extracted_sha256": {name: hashlib.sha256(text.encode()).hexdigest() for name, text in fragments.items()},
              "source_sha256": {str(p.relative_to(SOURCE)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
              "executable_sha256": hashlib.sha256(binary.read_bytes()).hexdigest()}
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
