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


def runtime_routes(text):
    """Execute real ownership/dispatch code; GDI allocation and HWND facts are mocked."""
    body = [function(text, signature) for signature in (
        "static void nativeRelease(){", "static void menuBalanceRelease(){",
        "static void nativeMenuRefresh(){", "static void nativeRefresh(){",
    )]
    dispatch = function(text, "  if(ev->message==WM_SETCURSOR){")
    body.append("static void post(const CWPRETSTRUCT& event){const CWPRETSTRUCT* ev=&event;\n" + dispatch + "\n}")
    return "namespace cursor_runtime_fixture {\n" + "\n\n".join(body) + "\n}"


def routes(text, baseline, runtime=None):
    presentation = "static cursorpresentation::Input cursorPresentationInput(" in text
    body = []
    if presentation:
        body.append(function(text, "static bool nativeCursorPresentation(){"))
    body.append(function(text, "static void leaveGame(){").replace("static void leaveGame()", "static void ownerLeaveGame()", 1))
    body.append("static void leaveGame(){++leaves;hideHeldWhenLeaving=pairedHide;ownerLeaveGame();}")
    if not baseline:
        for signature in ("static void coordinateDiagnostic(", "static bool ensureCoordinateWindows(", "static bool cursorMapPoint("):
            body.append(function(text, signature))
    if presentation:
        for signature in ("static cursorpresentation::Input cursorPresentationInput(",
                          "static void applyCursorPresentation(", "static void presentationDiagnostic(",
                          "static void enterNativeMenu(){", "static void exitNativeMenu(){"):
            body.append(function(text, signature))
        start = text.index(" const auto delivery=", text.index("static void hookStart("))
        end = text.index(" shared->sampleIntervalMs", start)
        body.append("static void productionStartPresentation(){\n" + text[start:end] + "\n}")
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
    if presentation:
        prefix += function(text, " if((nativeGameRefresh||mappedCursorPresentation)") + "\n"
    # The caller/frame/image sampling is outside this focused coordinate
    # fixture. The guards, destination statement and both submit expressions
    # below are extracted verbatim from the actual production source.
    route = prefix + geometry
    if presentation:
        # Keep the production order: acquire actual dimensions, resolve/apply,
        # ensure the selected presentation, then re-arm Hide and publish flags.
        # The frame/animation sampler is outside this focused fixture.
        start = text.index(" int gw=clickGameWidth,gh=clickGameHeight;", text.index("static void draw(){"))
        end = text.index(" refreshGameCursor();", start)
        route += text[start:end]
        route += next(line for line in text.splitlines() if line.strip()=="presentationDiagnostic(presentationInput,presentation);") + "\n"
        route += function(text, " if(nativeCursorPresentation()&&mode==L\"hide\"&&shared&&!nativeMenuHold&&!shared->menuSuspend){") + "\n"
        start = text.index(" int mapGW=presentation.mapped?")
        end = text.index(" if(hint.mode", start)
        route += "const int sampleGW=0,sampleGH=0;\n" + text[start:end]
        route += "observedMapPoint=mapPoint;observedMapWidth=mapGW;observedMapHeight=mapGH;\n"
        route += next(line for line in text.splitlines() if "const auto cursorOrigin=" in line) + "\n"
        route += destination + "\nbool packetSent=true;std::vector<uint32_t> raster(size_t(dw)*dh,0xffeeddcc);uint32_t* pixels=raster.data();\n"
        signature = "  if(nativeCursorPresentation()&&mode==L\"hide\"&&shared){"
        native = function(text, signature)
        end = text.index(signature) + len(native)
        tail_end = text.index(";", end) + 1
        tail = text[end:tail_end]
        assert "else if(!UpdateLayeredWindow" in tail, "Native/overlay publication shape changed"
        route += native + tail + "\n"
        signature = " if(!manualCursorPause&&!nativeMenuHold){"
        visibility = function(text, signature)
        end = text.index(signature) + len(visibility)
        tail = text[end:text.index(";", end) + 1]
        assert "else ShowWindow(overlay,SW_HIDE)" in tail, "Visibility release shape changed"
        route += visibility + tail + "\n}\n"
        body.append(route)
        body.append(runtime_routes(runtime))
    else:
        # Old revisions retain their own destination expression. Do not replace
        # a current production transform with a fixture-owned physical formula.
        route += function(text, " if(!wasInside){") + "\nif((nativeGameRefresh&&mode==L\"hide\")||noCursorMode)return;\n"
        if not baseline:
            route += next(line for line in text.splitlines() if "const auto cursorOrigin=" in line) + "\n"
        body.append(route + destination + "\n(void)" + update + ";\n(void)" + placement + ";\n}\n")
    return "\n\n".join(body)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--baseline-ref", default="c092a70e")
    parser.add_argument("--recovery-baseline-ref", default="2c8367ae1b0f672da7caca82260d70c7d9dddadc")
    parser.add_argument("--presentation-baseline-ref", default="00266f7")
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    repo = source.parent
    args.out.mkdir(parents=True, exist_ok=True)
    current = (source / "cursor/source/main.cpp").read_text()
    runtime = (source / "cursor/source/hide.cpp").read_text()
    baseline = subprocess.check_output(["git", "show", args.baseline_ref + ":source/cursor/source/main.cpp"], cwd=repo, text=True)
    recovery_baseline = subprocess.check_output(["git", "show", args.recovery_baseline_ref + ":source/cursor/source/main.cpp"], cwd=repo, text=True)
    presentation_baseline = subprocess.check_output(["git", "show", args.presentation_baseline_ref + ":source/cursor/source/main.cpp"], cwd=repo, text=True)
    rows = []
    for name, text, old in (("current", current, False), ("baseline", baseline, True),
                            ("interim", recovery_baseline, False), ("pre14", presentation_baseline, False)):
        presentation = "static cursorpresentation::Input cursorPresentationInput(" in text
        subject = args.out / (name + "-routes.inc")
        subject.write_text(routes(text, old, runtime))
        binary = args.out / name
        build = [args.cxx, "-std=c++17", "-O0", "-UNDEBUG", "-DRA2_UNIFIED",
                 "-I" + str(source / "assist/tests/cursor_coordinates_winmock"),
                 '-DCURSOR_ROUTES="' + subject.resolve().as_posix() + '"',
                 * (["-DCURSOR_BASELINE"] if old else []),
                 * (["-DCURSOR_PRESENTATION"] if presentation else []),
                 str(source / "assist/tests/cursor_coordinates.cpp"), "-o", str(binary)]
        compiled = subprocess.run(build, text=True, capture_output=True)
        (args.out / (name + "-build.log")).write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise SystemExit(compiled.stdout + compiled.stderr)
        options = ("",) if old else (("--owner-recovery-only",) if name == "interim" else
                  ("--presentation-geometry-only",) if name == "pre14" else ("", "--no-context-api", "--no-physical-api"))
        for option in options:
            label = name + (option.replace("--", "-") if option else "")
            run = subprocess.run([str(binary), *([option] if option else [])], text=True, capture_output=True, timeout=20)
            (args.out / (label + ".log")).write_text(run.stdout + run.stderr)
            record = json.loads(run.stdout.strip().splitlines()[-1])
            expected = 1 if name != "current" else 0
            if run.returncode != expected or (not expected and record["failures"]):
                raise SystemExit(run.stdout + run.stderr)
            if old:
                for claim in ("failed point never submits", "pillarbox left black bar", "pillarbox sidebar", "letterbox top bar", "letterbox bottom bar"):
                    assert "FAIL " + claim in run.stdout, "Missing actual old-route counterexample: " + claim
            if name == "interim":
                for claim in ("failed external window releases paired Hide before leaving", "unavailable physical domain releases paired Hide before leaving"):
                    assert "FAIL " + claim in run.stdout, "Missing interim production recovery counterexample: " + claim
            if name == "pre14":
                for claim in ("explicit canvas reaches displayed game midpoint", "automatic canvas clip reaches displayed game midpoint",
                              "mapped native preference uses external submission"):
                    assert "FAIL " + claim in run.stdout, "Missing pre14 production presentation counterexample: " + claim
            rows.append({"name": label, "expected_exit": expected, "exit_code": run.returncode, "build": build,
                         "routes_sha256": digest(subject), **record})
    paths = [
        "cursor/source/main.cpp", "cursor/source/cursor_coordinates.h", "cursor/source/cursor_coordinates_windows.h",
        "cursor/source/scale.h", "cursor/source/bridge.h", "cursor/source/hide.cpp",
        "cursor/source/cursor_presentation.h", "cursor/source/cursor_presentation_route.h", "cursor/source/sequence.h",
        "assist/tests/cursor_coordinates.cpp", "assist/tests/cursor_coordinates.py",
        "assist/tests/cursor_coordinates_winmock/windows.h",
    ]
    receipt = {
        "schema": "ra2.cursor-coordinates.pre15.v1", "passed": True,
        "source_sha256": {path: digest(source / path) for path in paths},
        "source_paths_relative_to": "source/", "baseline_revision": args.baseline_ref,
        "baseline_main_sha256": hashlib.sha256(baseline.encode()).hexdigest(), "results": rows,
        "recovery_baseline_revision": args.recovery_baseline_ref,
        "recovery_baseline_main_sha256": hashlib.sha256(recovery_baseline.encode()).hexdigest(),
        "presentation_baseline_revision": args.presentation_baseline_ref,
        "presentation_baseline_main_sha256": hashlib.sha256(presentation_baseline.encode()).hexdigest(),
        "current_checks": sum(row["checks"] for row in rows if row["expected_exit"] == 0),
        "negative_control_failures": sum(row["failures"] for row in rows if row["expected_exit"] == 1),
        "scope": "Production coordinate/presentation helpers; extracted main input observations, delivery/menu updates, native packet and overlay submission branches, point/client guards, window recovery and diagnostics; extracted Runtime nativeRelease/nativeMenuRefresh/nativeRefresh and WM_SETCURSOR dispatch branches.",
        "limitations": [
            "Win32 APIs and submission sinks are controlled substitutes; no Windows DWM, native compositor, real game or platform was executed.",
            "Old logical-caller failure means the old production route does not enforce the new physical-domain contract; it is not a claim that DWM always misplaces an old logical-context window.",
            "GetClipCursor and renderer geometry are supplied evidence, not emulated wrapper behavior or automatic proof of input semantics.",
            "Native image creation, HWND/dialog classification, callback/SEH entry and the Windows cursor compositor are boundary substitutes; the actual extracted ownership and dispatch code executes against them.",
            "Hook stop/start are subscription boundary substitutes. Owner-side release and recovery are executed, but actual Runtime Hide restoration/acknowledgement is not simulated or claimed.",
            "Dormant Cursor Shift5 is not re-enabled or claimed as an executed user input pipeline.",
        ],
    }
    (args.out / "receipt.json").write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"passed": True, "current_checks": receipt["current_checks"], "negative_control_failures": receipt["negative_control_failures"], "results": [{k: row[k] for k in ("name", "cases", "checks", "failures")} for row in rows]}))


if __name__ == "__main__":
    main()
