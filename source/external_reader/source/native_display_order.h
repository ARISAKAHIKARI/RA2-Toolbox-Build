#pragma once
#include "game_view.h"
#include <array>
#include <map>
namespace external {namespace nativedisplay {
// Tactical6D8F19..6D8F43: five DynamicVectorClass<Object*> headers at
//8A0360, stride24, items+4/count+10. Preserve its tie and custom-YSort order;
// a second coordinate sort is not equivalent to the engine's Display list.
template<class Read,class Markers>bool capture(Read read,Markers&markers,int frame){
 GameView g(read);if(frame<0||g.template get<int>(0xA8ED84,-1)!=frame)return false;
 std::array<std::vector<uint32_t>,5>lists;std::map<uint32_t,std::pair<int,int>>rank;
 for(unsigned layer=0;layer<lists.size();++layer){if(!g.vecChecked(0x8A0360+layer*24,lists[layer],65536))return false;for(size_t n=0;n<lists[layer].size();++n){uint32_t p=lists[layer][n];if(p<0x10000||!rank.emplace(p,std::make_pair(int(layer),int(n))).second)return false;}}
 for(unsigned layer=0;layer<lists.size();++layer){std::vector<uint32_t>again;if(!g.vecChecked(0x8A0360+layer*24,again,65536)||again!=lists[layer])return false;}
 if(g.template get<int>(0xA8ED84,-1)!=frame)return false;
 for(auto&m:markers){auto found=rank.find(m.object);if(found!=rank.end()&&g.template get<uint32_t>(m.object+0x10)!=m.id)return false;}
 for(auto&m:markers){auto found=rank.find(m.object);if(found!=rank.end()){m.layer=found->second.first;m.displayOrder=found->second.second;}else m.displayOrder=-1;}return true;
}
}}
