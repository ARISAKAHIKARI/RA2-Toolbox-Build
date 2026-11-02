namespace placementdrawing {
struct Drawing {nativeplacement::Preview preview{};int cameraX=0,cameraY=0;bool active=false;std::wstring name;};
static Drawing capture(){
 static nativeplacement::Preview complete{};static uint32_t process=0;
 Drawing out;if(!shared||!foregroundGame()||snap.chat||process!=pid){complete={};process=pid;if(!shared||!foregroundGame()||snap.chat)return out;}
 long seq=shared->placementPreviewSeq;MemoryBarrier();
 if(!(seq&1)){auto candidate=shared->placementPreview;MemoryBarrier();if(seq==shared->placementPreviewSeq)complete=candidate;}
 if(!complete.active||complete.match!=commandMatch()||DWORD(GetTickCount()-complete.tick)>500)return out;
 out.preview=complete;out.cameraX=complete.cameraX;out.cameraY=complete.cameraY;out.active=true;
 const int slot=complete.offer.slot;if(slot>=0&&slot<snap.typeCount)out.name=reinterpret_cast<const wchar_t*>(snap.types[slot].name);return out;
}
static bool same(const Drawing&a,const Drawing&b){
 if(a.active!=b.active)return false;if(!a.active)return true;
 const auto&x=a.preview;const auto&y=b.preview;
 return a.cameraX==b.cameraX&&a.cameraY==b.cameraY&&a.name==b.name&&nativeplacement::sameIdentity(x.offer,y.offer)&&
  x.status==y.status&&x.x==y.x&&x.y==y.y&&x.z==y.z&&x.width==y.width&&x.height==y.height&&x.zScale==y.zScale&&x.viewportX==y.viewportX&&x.viewportY==y.viewportY;
}
static void draw(Graphics&g,const Drawing&d){
 if(!d.active)return;const auto&p=d.preview;
 if((p.status==1||p.status==2)&&p.width>0&&p.height>0&&p.width<=32&&p.height<=32){
  auto point=[&](int x,int y){return PointF(float((int64_t(x)-y)*30/256-d.cameraX+p.viewportX),float((int64_t(x)+y)*15/256-external::screenHeight(p.z,p.zScale)-d.cameraY+p.viewportY));};
  Pen edge(p.status==2?Color(235,255,100,90):Color(235,105,220,255),1.5f);SolidBrush fill(p.status==2?Color(55,235,65,50):Color(55,65,180,235));
  for(int y=0;y<p.height;++y)for(int x=0;x<p.width;++x){const int wx=p.x+x*256,wy=p.y+y*256;PointF corners[]={point(wx,wy),point(wx+256,wy),point(wx+256,wy+256),point(wx,wy+256)};g.FillPolygon(&fill,corners,4);g.DrawPolygon(&edge,corners,4);}
 }
 std::wstring text=std::wstring(p.offer.tab?L"防御栏 · ":L"建筑栏 · ")+d.name+L" · 左键放置 · 右键 / Esc 取消";
 if(p.status==0)text+=L" · 此格暂不可读取，移动鼠标可恢复";else if(p.status==2)text+=L" · 超出可用区域，移动鼠标继续预览";else text+=L" · 落地合法性由游戏判定";
 drawText(g,text,18,88,12,Color(245,130,230,255));
}
}
