#!/usr/bin/env python3
"""Source-extracted callback coverage. Synthetic callers are not JJ validation."""
import argparse,hashlib,json,subprocess
from pathlib import Path
from tech_parameter_lookup import block
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--before-ref',default='250dffe')
a=p.parse_args();here=Path(__file__).resolve().parent;repo=here.parents[2];src=here.parent/'source'
a.out.mkdir(parents=True,exist_ok=True)
bridge='source/assist/source/tech_ui_breakpoint_bridge.h'
current=(repo/bridge).read_text()
previous=subprocess.check_output(['git','show',a.before_ref+':'+bridge],cwd=repo,text=True)
fixture=(here/'tech_parameter_lookup.cpp').read_text()
cases=(here/'tech_production_compatibility.cpp').read_text()
flags=(src/'tech_enhancement_parameter_policy.h').read_text()
oracle=block(flags,'inline constexpr uint32_t compareFlags(').replace('compareFlags(','originalCompareFlags(',1).replace('~ArithmeticFlags','~techenhanceparams::ArithmeticFlags')
report={'baseline_commit':subprocess.check_output(['git','rev-parse',a.before_ref],cwd=repo,text=True).strip(),
        'live_JJ_tested':False,'live_multiplayer_tested':False,'windows_executed':False,
        'scope':'Actual source-extracted permission callback and guards; synthetic memory/stack, no native game or DR execution.'}
for phase,source,extra,expected in [('baseline-default',previous,False,0),('baseline-new-contract',previous,True,1),('current',current,True,0)]:
 out=a.out/phase;out.mkdir(exist_ok=True)
 pieces=[block(current,'struct '+n)+';' for n in ('TypeIdentity','EnhancedIdentity','Policy')]
 pieces+=['static Policy policy;',block(source,'inline unsigned hash('),
   block(source,'static bool world(').replace('world()','worldImpl()',1),
   'static bool world(){++worldCalls;inWorld=true;const bool ok=worldImpl();inWorld=false;return ok;}',
   block(source,'static bool nativeCompatibleHot(').replace('nativeCompatibleHot()','nativeCompatibleHotImpl()',1),
   'static bool nativeCompatibleHot(){++nativeChecks;return nativeCompatibleHotImpl();}',
   block(source,'static bool enhancementCompatibleHot(').replace('enhancementCompatibleHot()','enhancementCompatibleHotImpl()',1),
   'static bool enhancementCompatibleHot(){++enhancementChecks;return enhancementCompatibleHotImpl();}',
   block(source,'static const TypeIdentity* fullIdentity('),block(source,'static bool fullType('),block(source,'static bool captureWorld(')]
 full=block(source,'if(tag==0){').replace('reinterpret_cast<uint32_t*>(uintptr_t(context.Esp))','fullStack')
 callback=block(source,'extern "C" int __cdecl AssistTechUiParameter(')
 enhance=callback[callback.index('if(!policy.enhanced'):].rsplit('}',1)[0]
 pieces+=['static int fullDispatch(Context&context){const unsigned tag=0;'+full+'return 0;}',
          'static int enhancementDispatch(Context&context,unsigned tag){'+enhance+'}']
 (out/'subject.inc').write_text('\n\n'.join(pieces));(out/'flags-oracle.inc').write_text(oracle)
 text=fixture.replace('int main(){',cases+'\nint main(){') if extra else fixture
 if extra:text=text.replace('indexChecks();flags();','indexChecks();flags();productionCompatibility();')
 (out/'test.cpp').write_text(text)
 exe=out/'test'
 cmd=['g++','-std=c++17','-O1','-g','-UNDEBUG','-DCURRENT_PHASE=1','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(src),'-I'+str(out),str(out/'test.cpp'),'-o',str(exe)]
 build=subprocess.run(cmd,text=True,capture_output=True);(out/'compile.log').write_text(build.stdout+build.stderr)
 if build.returncode:raise RuntimeError(phase+' compile failure: '+build.stderr)
 run=subprocess.run([str(exe.resolve())],text=True,capture_output=True);(out/'run.log').write_text(run.stdout+run.stderr)
 assert run.returncode==expected,(phase,run.returncode,run.stdout,run.stderr)
 if expected:assert 'compatibility admits an additional local-house caller' in run.stderr
 report[phase]={'returncode':run.returncode,'expected_returncode':expected,'stdout':run.stdout.strip(),'stderr':run.stderr.strip()}
 print(phase,run.stdout.strip() or run.stderr.strip())
paths=[bridge,'source/assist/tests/tech_parameter_lookup.cpp','source/assist/tests/tech_parameter_lookup.py','source/assist/tests/tech_production_compatibility.cpp','source/assist/tests/tech_production_compatibility.py']
report['sha256']={path:hashlib.sha256((repo/path).read_bytes()).hexdigest()for path in paths}
report['passed']=True;(a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
