"""Check shipped reservation layout, painter rectangles and window placement.

The point under a stationary mouse must keep resolving to the same candidate
while the queue grows/shrinks or the tab changes. Win32 window I/O is a double;
this is a geometry regression, not a live-game screenshot or DPI runtime test.
"""
from pathlib import Path
import argparse, hashlib, json, subprocess, tempfile
from tactical_frame_sampling import function

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path)
parser.add_argument('--policy-source', type=Path)
parser.add_argument('--host-source', type=Path)
parser.add_argument('--expect-regression', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
temporary = tempfile.TemporaryDirectory() if args.output is None else None
out = args.output or Path(temporary.name)
out.mkdir(parents=True, exist_ok=True)
policy_path = args.policy_source or root / 'assist/source/construction_policy.h'
host_path = args.host_source or root / 'assist/source/construction_host.h'
policy, host = policy_path.read_text(), host_path.read_text()
rect = function(host, 'RECT rect(int x,int y,int r,int bottom)')
position = function(host, 'static void resizePanel()')
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (policy_path, host_path)}
# Bind the existing viewport helper from the repository; keep the Layout body
# and both extracted production functions unchanged.
policy = policy.replace('#pragma once\n', '').replace(
    '#include "../../shared/source/viewport_scale.h"',
    '#include "' + str(root / 'shared/source/viewport_scale.h') + '"')
prefix = r'''
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
using namespace assist;
struct RECT {int left=0,top=0,right=0,bottom=0;};
struct POINT {int x=0,y=0;};
struct Window {RECT bounds;bool visible=false;};
using HWND=Window*;
static Window gameObject,windowObject;static HWND game=&gameObject,window=&windowObject;
static construction::Layout paintedLayout;static bool geometryReady=true,paintReady=true;
static POINT gameOrigin{101,-73};
constexpr int SWP_NOACTIVATE=1,SWP_NOOWNERZORDER=2,SWP_NOCOPYBITS=4,SWP_NOZORDER=8,SWP_SHOWWINDOW=16;
static bool ClientToScreen(HWND,POINT*p){*p=gameOrigin;return true;}
static bool IsWindowVisible(HWND w){return w->visible;}
static bool GetWindowRect(HWND w,RECT*r){*r=w->bounds;return true;}
namespace overlaystack {
enum class Layer {Construction};
static void place(HWND w,Layer,int x,int y,int width,int height,int flags){
 w->bounds={x,y,x+width,y+height};if(flags&SWP_SHOWWINDOW)w->visible=true;
}
}
'''
program = policy + prefix + position + '\nstruct Painter {construction::Layout layout;\n' + rect + '\n};\n' + r'''
static uint64_t checks=0,states=0,drifts=0,hitChanges=0,downwardGrowth=0,rowChanges=0;
static int maxBoundaryDrift=0;static std::vector<int> defaultBoundaries;
static void check(bool value){++checks;assert(value);}
static void place(const construction::Layout&layout){paintedLayout=layout;resizePanel();}
static int screenY(int local){return window->bounds.top+local;}
int main(){
 const std::vector<std::array<int,4>> transforms={
  {2560,1440,2560,1440},{2560,1440,3840,2160},{1920,1080,3840,2160},
  {1280,720,1920,1080},{1920,1080,2560,1440},{640,480,640,480},
  {640,400,320,200},{640,400,641,401},{800,600,800,600},
  {1024,768,1365,1024},{1365,767,2048,1151},{3840,2160,2560,1440},
  {640,400,853,533}
 };
 const std::vector<int> percentages={50,67,75,100,125,150,175,200,250};
 std::vector<int> queues;for(int n=0;n<=construction::UnitReservationCapacity;++n)queues.push_back(n);
 for(int n=construction::UnitReservationCapacity-1;n>=0;--n)queues.push_back(n);
 for(const auto&s:transforms)for(int pct:percentages){
  construction::Layout layout;check(layout.set(s[0],s[1],s[2],s[3],pct));
  layout.content(0,50);place(layout);Painter initial{layout};
  const int initialBoundary=screenY(initial.rect(0,layout.headerHeight(),144,layout.headerHeight()).top);
  const int initialRows=layout.rows;
  std::vector<std::array<int,2>> fixedPoints;
  for(int n=0;n<initialRows*2;++n){auto r=initial.rect(n%2*72,layout.headerHeight()+n/2*66,(n%2+1)*72,layout.headerHeight()+(n/2+1)*66);
   fixedPoints.push_back({window->bounds.left+(r.left+r.right)/2,screenY((r.top+r.bottom)/2)});
  }
  int previousTop=window->bounds.top,previousCount=0;
  for(int queued:queues){
   layout.content(queued,50);place(layout);Painter painter{layout};
   if(queued==previousCount+1&&window->bounds.top>previousTop)++downwardGrowth;
   previousTop=window->bounds.top;previousCount=queued;
   if(layout.rows!=initialRows)++rowChanges;
   if(s==transforms.front()&&pct==100&&queued<6&&defaultBoundaries.size()<6)
    defaultBoundaries.push_back(screenY(painter.rect(0,layout.headerHeight(),144,layout.headerHeight()).top)-gameOrigin.y);
   // Different tab contents, including no candidates, must share the boundary.
   for(int candidates:{0,1,2,3,5,12,13,50}){
    layout.content(queued,candidates);place(layout);painter.layout=layout;++states;
    const int boundary=screenY(painter.rect(0,layout.headerHeight(),144,layout.headerHeight()).top);
    const int drift=std::abs(boundary-initialBoundary);if(drift)++drifts;
    maxBoundaryDrift=std::max(maxBoundaryDrift,drift);
    check(window->bounds.top>=gameOrigin.y&&window->bounds.left>=gameOrigin.x);
    check(window->bounds.bottom<=gameOrigin.y+s[3]&&window->bounds.right<=gameOrigin.x+s[2]);
    check(layout.rows>=0&&layout.rows<=6&&layout.rows<=(candidates+1)/2&&((layout.rows==0)==(candidates==0)));
    for(int n=0;n<std::min({candidates,layout.rows*2,int(fixedPoints.size())});++n){
     const auto&point=fixedPoints[n];
     if(layout.choiceAt(point[0]-window->bounds.left,point[1]-window->bounds.top,0,candidates)!=n)++hitChanges;
     auto r=painter.rect(n%2*72,layout.headerHeight()+n/2*66,(n%2+1)*72,layout.headerHeight()+(n/2+1)*66);
     check(layout.choiceAt((r.left+r.right)/2,(r.top+r.bottom)/2,0,candidates)==n);
     check(layout.choiceAt((r.left+r.right)/2,(r.top+r.bottom)/2,2,50)==n+2);
    }
    for(int n=0;n<std::min(queued,construction::VisibleCapacity);++n){
     auto r=painter.rect(10,construction::StackTop+n*construction::StackStep+9,11,construction::StackTop+n*construction::StackStep+10);
     check(layout.queueAt(r.left,r.top,queued)==n);
    }
    const int header=painter.rect(0,layout.headerHeight(),144,layout.headerHeight()).top;
    check(layout.choiceAt(layout.x(36),header-1,0,candidates)==-1);
    check(layout.choiceAt(layout.width(),header,0,candidates)==-1);
    check(layout.choiceAt(layout.x(36),layout.height(),0,candidates)==-1);
    check(layout.queueAt(layout.x(10),layout.height(),queued)==-1);
   }
  }
 }
 std::cout<<"{\"transforms\":"<<transforms.size()*percentages.size()<<",\"states\":"<<states<<",\"checks\":"<<checks<<",\"boundary_drift_states\":"<<drifts<<",\"stationary_click_changes\":"<<hitChanges<<",\"queue_growth_moved_window_down\":"<<downwardGrowth<<",\"queue_dependent_row_changes\":"<<rowChanges<<",\"max_boundary_drift_pixels\":"<<maxBoundaryDrift<<",\"default_boundary_by_queue_0_to_5\":[";
 for(size_t n=0;n<defaultBoundaries.size();++n){if(n)std::cout<<',';std::cout<<defaultBoundaries[n];}
 std::cout<<"],\"passed\":"<<(EXPECT_REGRESSION?(drifts&&hitChanges):(!drifts&&!hitChanges&&!downwardGrowth&&!rowChanges))<<",\"windows_runtime_executed\":false}\n";
 return (EXPECT_REGRESSION?(drifts&&hitChanges):(!drifts&&!hitChanges&&!downwardGrowth&&!rowChanges))?0:1;
}
'''
cpp = out / 'layout.cpp'
cpp.write_text(program)
command = ['g++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined', '-UNDEBUG',
           '-DEXPECT_REGRESSION=' + str(int(args.expect_regression)), str(cpp), '-o', str(out / 'layout')]
built = subprocess.run(command, capture_output=True, text=True)
report = {'scope': __doc__, 'source_sha256': hashes, 'compile_command': command,
          'compile_exit': built.returncode, 'compile_stderr': built.stderr,
          'expect_regression': args.expect_regression}
if built.returncode == 0:
    import os
    ran = subprocess.run([str(out / 'layout')], capture_output=True, text=True,
                         env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
    report.update(run_exit=ran.returncode, stdout=ran.stdout, stderr=ran.stderr)
    if ran.returncode == 0:
        report['result'] = json.loads(ran.stdout)
report['source_hash_stable'] = all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in hashes.items())
(out / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(report, ensure_ascii=False, indent=2))
raise SystemExit(report.get('run_exit', built.returncode))
