"""Run production catalogue/ProcessReader/transport paths; optional fixed-base control."""
from pathlib import Path
import argparse, json, subprocess, tempfile, tarfile, io
parser=argparse.ArgumentParser()
parser.add_argument('--out',type=Path,required=True)
parser.add_argument('--baseline-ref')
args=parser.parse_args();args.out.mkdir(parents=True,exist_ok=True)
repo=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='ra2_catalogue_frame_') as tmp:
    tmp=Path(tmp);source=repo/'source'
    if args.baseline_ref:
        archive=subprocess.check_output(['git','archive',args.baseline_ref,'source'],cwd=repo)
        with tarfile.open(fileobj=io.BytesIO(archive)) as tar:tar.extractall(tmp/'baseline',filter='data')
        source=tmp/'baseline/source'
        (source/'assist/tests/catalogue_frame_sampling.cpp').write_bytes(Path(__file__).with_suffix('.cpp').read_bytes())
    driver=(source/'assist/tests/world_frame_core_test.py').read_text()
    driver=driver[:driver.index('with tempfile.TemporaryDirectory')]
    scope={'__file__':str(source/'assist/tests/world_frame_core_test.py')};exec(driver,scope)
    (tmp/'windows.h').write_text(scope['windows'])
    exe=tmp/'catalogue-frame';cmd=['g++','-std=c++17','-O2','-pthread','-DWORLD_FRAME_PLATFORM_FIXTURE','-I'+str(tmp),str(source/'assist/tests/catalogue_frame_sampling.cpp'),'-o',str(exe)]
    compiled=subprocess.run(cmd,capture_output=True,text=True)
    (args.out/'compile.log').write_text(compiled.stdout+compiled.stderr);compiled.check_returncode()
    run=[str(exe)]+(['--expect-periodic-miss'] if args.baseline_ref else [])
    result=subprocess.run(run,capture_output=True,text=True)
    (args.out/'result.log').write_text(result.stdout+result.stderr)
    (args.out/'result.json').write_text(json.dumps({'baseline_ref':args.baseline_ref,'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr,'live_game_tested':False},indent=2)+'\n')
    print(result.stdout+result.stderr,end='');result.check_returncode()
