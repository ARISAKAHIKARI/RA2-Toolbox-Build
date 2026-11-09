"""Exercise the production cold scan and optionally compare a frozen baseline."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--out', required=True, type=Path)
parser.add_argument('--baseline')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)

def block(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

receipt = {'baseline': args.baseline, 'phases': {}, 'windows_executed': False, 'game_executed': False,
           'boundary': 'Exact production scan block, vec/item and catalogue-level function; synthetic arrays and admitted world/signature state. Operation counts, not timings.'}
for phase in (('baseline', 'current') if args.baseline else ('current',)):
    work = out / phase
    work.mkdir(exist_ok=True)
    def source(path):
        return (subprocess.check_output(['git', 'show', args.baseline + ':' + path], cwd=repo).decode()
                if phase == 'baseline' else (repo / path).read_text())
    bridge = source('source/assist/source/bridge.cpp')
    exact = source('source/assist/source/tech_ui_breakpoint_bridge.h')
    subject = '\n'.join([block(bridge, 'static Vec vec('), block(bridge, 'static uint32_t item('),
                          block(exact, 'static int techEnhancementCatalogueLevel('),
                          'static void executeOriginalScanBlock(uint32_t now){using namespace techparameters;\n' +
                          block(exact, 'if(!enhancementParameterScan||') + '\n}'])
    (work / 'tech_capture_subject.inc').write_text(subject)
    command = ['g++', '-std=c++17', '-O2', '-fsanitize=undefined', '-fno-sanitize-recover=all',
               '-I' + str(repo / 'source/assist/source'), '-I' + str(work),
               str(Path(__file__).with_suffix('.cpp')), '-o', str(work / 'tech-capture')]
    build = subprocess.run(command, text=True, capture_output=True)
    (work / 'build.log').write_text(build.stdout + build.stderr)
    build.check_returncode()
    run = subprocess.run([str(work / 'tech-capture')], text=True, capture_output=True)
    (work / 'run.log').write_text(run.stdout + run.stderr)
    print(phase, run.stdout + run.stderr, end='')
    run.check_returncode()
    receipt['phases'][phase] = {'command': command, 'subject_sha256': hashlib.sha256(subject.encode()).hexdigest(),
                              'rows': [json.loads(line) for line in run.stdout.splitlines()]}
if args.baseline:
    before, after = (receipt['phases'][p]['rows'] for p in ('baseline', 'current'))
    assert [(r['case'], r.get('identity')) for r in before] == [(r['case'], r.get('identity')) for r in after]
    receipt['identities_equal'] = True
paths = ['source/assist/source/tech_ui_breakpoint_bridge.h', 'source/assist/source/tech_enhancement.h',
         'source/assist/source/tech_parameter_lookup.h', 'source/assist/tests/tech_enhancement_capture.cpp',
         'source/assist/tests/tech_enhancement_capture.py']
receipt['source_sha256'] = {p: hashlib.sha256((repo / p).read_bytes()).hexdigest() for p in paths}
receipt['passed'] = True
(out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
