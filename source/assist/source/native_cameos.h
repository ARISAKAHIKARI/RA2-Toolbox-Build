#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>
namespace assist { namespace construction {
// YRpp TabDataStruct + native SelectClass gadgets. Read only; no AddCameo.
struct NativeCell {int index=0,x=0,y=0,w=0,h=0;};
struct NativeFrame {int tab=-1,top=0,width=0,height=0;std::vector<NativeCell> cells;
 int hit(int x,int y)const{for(const auto&c:cells)if(x>=c.x&&y>=c.y&&x<c.x+c.w&&y<c.y+c.h)return c.index;return -1;}
};
struct NativeView {int tab=-1,top=0,width=0,height=0,count=0;NativeCell cells[60]{};
 int hit(int x,int y)const{for(int n=0;n<count;++n){const auto&c=cells[n];if(x>=c.x&&y>=c.y&&x<c.x+c.w&&y<c.y+c.h)return c.index;}return -1;}
};
template<class Read>bool readNativeView(Read read,NativeView&out){
 out={};auto get=[&](uint32_t at,int&v){return read(at,&v,4);};
 int tab=-1,width=0,height=0,sidebarY=0,sidebarH=0,topY=0;uint32_t session=0;
 if(!get(0x87F7E8+0x539C,tab)||tab<0||tab>1||!get(0x8A00A4,width)||!get(0x8A00A8,height)||width<640||height<400||width>32768||height>32768)return false;
 if(!get(0x886F94,sidebarY)||!get(0x886F9C,sidebarH)||!get(0xB0B4F8,topY)||!read(0xA8B230,&session,4)||session<0x10000)return false;
 int small=0;if(!get(session+0x34B8,small))return false;
 // Same visible row count as SidebarClass::GetVisibleCameos (0x6AC430).
 int rows=std::clamp((sidebarY+sidebarH-topY-(small?18:26)-7)/50,0,30);
 uint32_t strip=0x87F7E8+0x1544+tab*3988;uint8_t header[88]{},after[88]{},gadgets[60*56]{};int entries[75*13]{};
 if(!read(strip,header,sizeof header)||!header[0x1C]||header[0x3F])return false;
 int top=0,count=0,index=0;memcpy(&top,header+0x44,4);memcpy(&count,header+0x54,4);memcpy(&index,header+0x38,4);
 if(index!=tab||top<0||top>37||count<0||count>75||!rows)return false;
 if(count&&!read(strip+88,entries,size_t(count)*52))return false;
 if(!read(0xB07E80+tab*60*56,gadgets,size_t(rows)*2*56))return false;
 NativeView frame;frame.tab=tab;frame.top=top;frame.width=width;frame.height=height;
 for(int i=0;i<rows*2&&top*2+i<count;++i){int entry=top*2+i;const auto*b=gadgets+i*56;int box[4]{};memcpy(box,b+0xC,sizeof box);
  if(entries[entry*13+1]!=7||entries[entry*13]<0||box[0]<0||box[1]<0||box[2]!=60||box[3]!=48||box[0]>width-60||box[1]>height-48)continue;
  frame.cells[frame.count++]={entries[entry*13],box[0],box[1],box[2],box[3]};
 }
 int tabAfter=-1,wAfter=0,hAfter=0;
 if(!read(strip,after,sizeof after)||memcmp(header+0x1C,after+0x1C,88-0x1C)||!get(0x87F7E8+0x539C,tabAfter)||tabAfter!=tab||!get(0x8A00A4,wAfter)||!get(0x8A00A8,hAfter)||wAfter!=width||hAfter!=height)return false;
 out=frame;return true;
}
template<class Read>bool readNative(Read read,NativeFrame&out){
 NativeView view;if(!readNativeView(read,view)){out={};return false;}
 out.tab=view.tab;out.top=view.top;out.width=view.width;out.height=view.height;out.cells.assign(view.cells,view.cells+view.count);return true;
}
struct NativeBadge {int x=0,y=0,w=0,h=0,positions=0;bool operator==(const NativeBadge&o)const{return x==o.x&&y==o.y&&w==o.w&&h==o.h&&positions==o.positions;}};
}}

