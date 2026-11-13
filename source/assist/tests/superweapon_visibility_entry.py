"""Execute the production service entry with the independent panel boundary held."""
from pathlib import Path
import argparse,subprocess,tempfile
from tactical_frame_sampling import function
p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2]
current=(root/'assist/source/main.cpp').read_text()
prefix=r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
static bool visible=true,battlefield=true,enabled=true,focused=true,chat=false;
static int shown=0,hidden=0;static void*game=nullptr;static uint32_t pid=7;
static struct {bool chat=false;}snap;static void*shared=nullptr;
namespace overlayvisibility {bool visible(){return ::visible;}}
static bool constructionDisplayVisible(){return enabled&&focused&&!snap.chat&&battlefield;}
static bool constructionDisplayFresh(){return constructionDisplayVisible();}
static bool foregroundGame(){return focused;}static uint32_t GetTickCount(){return 1000;}
static bool commandFresh(uint32_t){return true;}static uint32_t commandMatch(){return 1;}
static struct {bool superweaponPanel=true;}extra;
namespace swsidebar {struct Options {bool enabled=false;};}
static swsidebar::Options superweaponOptions;
namespace nativeviewhost {struct Transform{bool active=false;struct {int x=0,w=640;}physical;};bool sampleTransform(void*,Transform&){return true;}bool stable(void*,const Transform&){return true;}}
static struct {bool readFresh(uint32_t,void*,size_t){return true;}}externalReader;
namespace buildreserve {int occupiedRect(){return 0;}}
static constexpr int SelectSuper=1;static void send(int,int,int,int,bool,uint32_t){}static void changedSetting(){}
static struct {void hide(){++hidden;}template<class...Args>void update(Args...){++shown;}}superweaponPanel;
'''
test=r'''
int main(){
 serviceSuperweaponPanel();assert(shown==1&&hidden==0);
 battlefield=false;serviceSuperweaponPanel();assert(shown==1&&hidden==1);
 battlefield=true;serviceSuperweaponPanel();assert(shown==2&&hidden==1);
 visible=false;serviceSuperweaponPanel();assert(shown==2&&hidden==2);
 visible=true;snap.chat=true;serviceSuperweaponPanel();assert(shown==2&&hidden==3);
 snap.chat=false;focused=false;serviceSuperweaponPanel();assert(shown==2&&hidden==4);
 focused=true;enabled=false;serviceSuperweaponPanel();assert(shown==2&&hidden==5);
}
'''
with tempfile.TemporaryDirectory(prefix='ra2_sw_visibility_') as tmp:
 for name,text,expected in [('current',current,0)]+([('baseline',a.baseline.read_text(),-1)] if a.baseline else []):
  source=Path(tmp)/(name+'.cpp');exe=Path(tmp)/name;source.write_text(prefix+function(text,'static void serviceSuperweaponPanel()')+test)
  subprocess.run(['g++','-std=c++17','-O2',str(source),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True)
  assert (result.returncode==0)==(expected==0),(name,result.returncode,result.stderr)
  print('PASS',name,'menu/global-layer/chat/focus/disabled entry' if not expected else 'reopens on hidden battlefield (negative control)')
