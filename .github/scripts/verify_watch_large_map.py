from pathlib import Path
import hashlib,json
root=Path('.')
paths=['source/assist/source/watch_scene.h','source/external_reader/source/radar_scene.h','source/external_reader/source/world_scene_pipeline.h']
before={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in paths}
p=Path('source/assist/tests/watch_world_performance.cpp');s=p.read_text(encoding='utf-8')
s=s.replace('memory(0x2000000)','memory(0x8000000)')
s=s.replace('makeTerrain(){','makeTerrain(int dimension=64){')
s=s.replace('g.width=g.height=g.offsetX=g.offsetY=64;g.rw=g.rh=g.rawWidth=g.rawHeight=128;','g.width=g.height=g.offsetX=g.offsetY=dimension;g.rw=g.rh=g.rawWidth=g.rawHeight=dimension*2;')
s=s.replace('g.bounds={64,64,0,0,64,64}','g.bounds={dimension,dimension,0,0,dimension,dimension}')
s=s.replace('std::array<int,6>{{64,64,0,0,64,64}}','std::array<int,6>{{dimension,dimension,0,0,dimension,dimension}}')
s=s.replace('(48+int(i%32))','(t->geo.width-16+int(i%32))').replace('(48+int(i/32)%32)','(t->geo.height-16+int(i/32)%32)')
s=s.replace('old.epoch=current.epoch=9;','old.epoch=current.epoch=9+t->geo.width;').replace('sceneBank().publish(7,9,','sceneBank().publish(7,9+t->geo.width,')
s=s.replace('static void watchRun(const std::shared_ptr<radar::Terrain>&t){','static void watchRun(const std::shared_ptr<radar::Terrain>&t){\n std::printf("Watch map %dx%d\\n",t->geo.width,t->geo.height);')
needle=' std::puts("PASS actual Renderer stale-bank regression:'
start=s.index(needle);end=s.index('\n}',start)
s=s[:end]+r'''
 if(t->geo.width>=128){
  const int dim=t->geo.width;const int points[3][2]={{dim/2,dim+16},{dim,dim},{dim+48,dim/2+16}};
  for(int i=0;i<3;++i){assert(t->geo.legal(points[i][0],points[i][1]));const auto&m=bank->markers[360+i*35];put(m.object+0x9C,std::array<int,3>{{points[i][0]*256,points[i][1]*256,0}});}
  bool ready=false;for(int warm=0;warm<64&&!ready;++warm){assert(old.update(211)&&current.update(211));ready=true;for(int i=0;i<3;++i)ready=ready&&old.views[i].frame.center.x==points[i][0]*256&&old.views[i].frame.center.y==points[i][1]*256&&current.views[i].frame.center.x==points[i][0]*256&&current.views[i].frame.center.y==points[i][1]*256;}
  assert(ready);for(int i=0;i<3;++i){assert(old.views[i].scene&&current.views[i].scene);assert(old.views[i].scene->pixels==current.views[i].scene->pixels);assert(old.views[i].frame.cameraX==current.views[i].frame.cameraX&&old.views[i].frame.cameraY==current.views[i].frame.cameraY);}
  std::puts("PASS large-map disjoint three-camera union: native terrain and unit pixels equal full-map preparation after camera relocation");
 }
'''+s[end:]
s=s.replace('watchRun(t);anchorCases(t);','watchRun(t);anchorCases(t);auto large=makeTerrain(128);watchRun(large);')
assert 'makeTerrain(128)' in s and 'g.width=g.height=g.offsetX=g.offsetY=dimension' in s
p.write_text(s,encoding='utf-8')
assert before=={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in paths}
Path('large-production-inputs.json').write_text(json.dumps(before,indent=2)+'\n')
print('Unmodified production rendering inputs:',json.dumps(before))
