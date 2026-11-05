"""Fill final report/source pins and run the local packager; never publishes."""
from pathlib import Path
import argparse,json,shutil,subprocess,sys

HERE=Path(__file__).resolve().parent

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--source-commit',required=True,help='Real remote source-only commit returned by GitHub, never a locally invented commit')
 p.add_argument('--source-root',type=Path,required=True);p.add_argument('--binaries',type=Path,required=True)
 p.add_argument('--build-logs',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
 p.add_argument('--animation-report',type=Path,required=True);p.add_argument('--insignia-report',type=Path,required=True);p.add_argument('--watch-report',type=Path,required=True);p.add_argument('--review-report',type=Path,required=True)
 p.add_argument('--adapter',type=Path)
 a=p.parse_args();inputs=json.loads((HERE/'release-inputs.template.json').read_text(encoding='utf-8-sig'))
 inputs['compiled_source_commit']=a.source_commit
 inputs['compiled_source_sha256']=json.loads((a.build_logs/'source-before.json').read_text(encoding='utf-8-sig'))
 inputs['reports']={'building_animation':str(a.animation_report.resolve()),'insignia_scale':str(a.insignia_report.resolve()),'watch_tracking':str(a.watch_report.resolve()),'review':str(a.review_report.resolve())}
 inputs['ready']=True;inputs.pop('notes',None)
 a.out.mkdir(parents=True,exist_ok=True);input_file=a.out/'release-inputs.json';input_file.write_text(json.dumps(inputs,indent=2)+'\n',encoding='utf-8',newline='\n')
 command=[sys.executable,str(HERE/'package_release.py'),'--source-root',str(a.source_root.resolve()),'--binaries',str(a.binaries.resolve()),'--build-logs',str(a.build_logs.resolve()),'--inputs',str(input_file.resolve()),'--out',str(a.out.resolve())]
 if a.adapter:command+=['--adapter',str(a.adapter.resolve())]
 subprocess.run(command,check=True)
 # These small scripts are repository publication inputs, not release assets.
 for name in ('publish_release.py','package_release.py','prepare_release.py','release-inputs.template.json'):
  shutil.copy2(HERE/name,a.out/name)
 print('Prepared publication folder:',a.out.resolve())
 print('Copy publish-v142-pre1.yml to .github/workflows and the prepared folder allowlist from README into packages/v1.0.42-pre.1 on a child of the source commit.')

if __name__=='__main__':main()
