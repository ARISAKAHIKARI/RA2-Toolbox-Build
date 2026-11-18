#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>
#include "../../shared/source/world_frame_page_sort.h"
// Producer-side request preparation. A Host-only display boundary does not
// consume the command producer's field leases or sort Host visual pages.
// The Host VisualCache independently resolves its own page subscriptions.
namespace assist { namespace capturepolicy {
template<class Leases>void prepareRequestedPages(
 bool boundaryOnly,uint32_t tick,const std::vector<uint32_t>&host,
 const std::vector<uint32_t>&runtime,Leases&leases,std::vector<uint32_t>&out,
 std::vector<uint32_t>*sortScratch=nullptr){
 out.clear();
 if(boundaryOnly)return; // O(1); preserve pending command dependencies.
 out.assign(host.begin(),host.end());
 out.insert(out.end(),runtime.begin(),runtime.end());
 for(uint32_t page:runtime)leases.retain(page,tick);
 leases.append(tick,out);
 if(sortScratch)worldframe::sortUniquePageAddresses(out,*sortScratch);
 else {std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());}
}
}}
