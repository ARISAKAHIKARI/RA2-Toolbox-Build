#!/usr/bin/env python3
"""Run production coordinate helpers/routes with controlled Win32 boundaries.

The recorded submission sink does not emulate DWM virtualization. The old
logical-caller counterexample proves absence of the new physical-domain
contract, not that every old logical window was visually misplaced.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def function(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def routes(text, baseline):
    body = []
    if not baseline:
        for signature in ("static void coordinateDiagnostic(", "static bool ensureCoordinateWindows(", "static bool cursorMapPoint("):
            body.append(function(text, signature))
    body.append(function(text, "static bool refreshClickRegion("))
    if baseline:
        body.append("static bool productionRefresh(){return refreshClickRegion();}")
        start = text.index(" bool mapPoint=pos.x")
        end = text.index(" if(hint.mode", start)
        statements = "\n".join(line for line in text[start:end].splitlines() if not line.lstrip().startswith("int mapGW="))
        body.append("static bool productionMapPoint(POINT pos,int mapGW,int){POINT org{physicalClient.left,physicalClient.top};RECT rc{0,0,physicalClient.width(),physicalClient.height()};\n" + statements + "\nreturn mapPoint;}")
    else:
        body.append("static bool productionRefresh(){CoordinateScope coordinates;return refreshClickRegion(coordinates);}")
        body.append("static bool productionMapPoint(POINT point,int gw,int gh){return cursorMapPoint(point,physicalClient,gw,gh);}")
    point_start = text.index(" POINT pos;GetCursorPos" if baseline else " POINT pos{};cursorcoordinates::Rect client;")
    point_end = text.index(" // A different game-owned window", point_start)
    geometry = text[point_start:point_end]
    destination = next(line for line in text.splitlines() if "POINT dst=" in line)
    update = re.search(r"UpdateLayeredWindow\(overlay,NULL,&dst,&sz,memdc,&src,0,&bf,ULW_ALPHA\)", text).group()
    placement_start = text.index("[&](){RECT shown{};int x=")
    placement = text[placement_start:text.index("}())", placement_start) + 3]
    prefix = "static void productionDrawRoute(){DWORD fp=pid;\n"
    if baseline:
        creation = "\n".join(line for line in text.splitlines() if line.startswith((" overlay=CreateWindowExW(", " hud=CreateWindowExW(")))
        prefix += "if(!overlay){HINSTANCE inst=GetModuleHandleW(nullptr);\n" + creation + "\n}\n"
    else:
        prefix += "cursorcoordinates::CoordinateScope coordinates(true);\n"
    # The caller/frame/image sampling is outside this focused coordinate
    # fixture. The guards, destination statement and both submit expressions
    # below are extracted verbatim from the actual production source.
    body.append(prefix + geometry +
                "\nif((nativeGameRefresh&&mode==L\"hide\")||noCursorMode)return;\n" +
                ("" if baseline else "const auto cursorOrigin=cursorcoordinates::spriteOrigin({pos.x,pos.y},shx,shy);\n") +
                destination + "\n(void)" + update + ";\n(void)" + placement + ";\n}\n")
    return "\n\n".join(body)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--baseline-ref", default="c092a70e")
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    repo = source.parent
    args.out.mkdir(parents=True, exist_ok=True)
    current = (source / "cursor/source/main.cpp").read_text()
    baseline = subprocess.check_output(["git", "show", args.baseline_ref + ":source/cursor/source/main.cpp"], cwd=repo, text=True)
    rows = []
    for name, text, old in (("current", current, False), ("baseline", baseline, True)):
        subject = args.out / (name + "-routes.inc")
        subject.write_text(routes(text, old))
        binary = args.out / name
        build = [args.cxx, "-std=c++17", "-O0", "-UNDEBUG", "-DRA2_UNIFIED",
                 "-I" + str(source / "assist/tests/cursor_coordinates_winmock"),
                 '-DCURSOR_ROUTES="' + subject.resolve().as_posix() + '"',
                 * (["-DCURSOR_BASELINE"] if old else []),
                 str(source / "assist/tests/cursor_coordinates.cpp"), "-o", str(binary)]
        compiled = subprocess.run(build, text=True, capture_output=True)
        (args.out / (name + "-build.log")).write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise SystemExit(compiled.stdout + compiled.stderr)
        for option in (("",) if old else ("", "--no-context-api", "--no-physical-api")):
            label = name + (option.replace("--", "-") if option else "")
            run = subprocess.run([str(binary), *([option] if option else [])], text=True, capture_output=True, timeout=20)
            (args.out / (label + ".log")).write_text(run.stdout + run.stderr)
            record = json.loads(run.stdout.strip().splitlines()[-1])
            expected = 1 if old else 0
            if run.returncode != expected or (not old and record["failures"]):
                raise SystemExit(run.stdout + run.stderr)
            if old:
                for claim in ("failed point never submits", "pillarbox left black bar", "pillarbox sidebar", "letterbox top bar", "letterbox bottom bar"):
                    assert "FAIL " + claim in run.stdout, "Missing actual old-route counterexample: " + claim
            rows.append({"name": label, "expected_exit": expected, "exit_code": run.returncode, "build": build,
                         "routes_sha256": digest(subject), **record})
    paths = [
        "cursor/source/main.cpp", "cursor/source/cursor_coordinates.h", "cursor/source/cursor_coordinates_windows.h",
        "cursor/source/scale.h", "cursor/source/bridge.h", "cursor/source/hide.cpp",
        "assist/tests/cursor_coordinates.cpp", "assist/tests/cursor_coordinates.py",
        "assist/tests/cursor_coordinates_winmock/windows.h",
    ]
    receipt = {
        "schema": "ra2.cursor-coordinates.pre12.v1", "passed": True,
        "source_sha256": {path: digest(source / path) for path in paths},
        "source_paths_relative_to": "source/", "baseline_revision": args.baseline_ref,
        "baseline_main_sha256": hashlib.sha256(baseline.encode()).hexdigest(), "results": rows,
        "current_checks": sum(row["checks"] for row in rows if row["expected_exit"] == 0),
        "negative_control_failures": sum(row["failures"] for row in rows if row["expected_exit"] == 1),
        "scope": "Production Windows/pure coordinate helpers plus verbatim extracted main client/point guard, sidebar region, map-point classification, overlay-window lifecycle, diagnostics and destination/submit expressions.",
        "limitations": [
            "Win32 APIs and submission sinks are controlled substitutes; no Windows DWM, native compositor, real game or platform was executed.",
            "Old logical-caller failure means the old production route does not enforce the new physical-domain contract; it is not a claim that DWM always misplaces an old logical-context window.",
            "Runtime/protocol and OS pointer position are unchanged. Native image creation is outside this coordinate fixture.",
            "Dormant Cursor Shift5 is not re-enabled or claimed as an executed user input pipeline.",
        ],
    }
    (args.out / "receipt.json").write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"passed": True, "current_checks": receipt["current_checks"], "negative_control_failures": receipt["negative_control_failures"], "results": [{k: row[k] for k in ("name", "cases", "checks", "failures")} for row in rows]}))


if __name__ == "__main__":
    main()
