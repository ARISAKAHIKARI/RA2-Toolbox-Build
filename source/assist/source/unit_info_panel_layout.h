#pragma once
#include <algorithm>
#include "../../external_reader/source/unit_info_panel_model.h"

// Pure layout rules for the bottom-left unit panel. Collapsed single-unit
// view shows only the unit cameo and HP tile; cargo is an explicit expansion.
namespace unitpanelhost::panelmetrics {
inline constexpr int CellW=60,CellH=48,PassengerCameoW=44,PassengerCameoH=36;
inline constexpr int CargoPerPage=7,GroupPerPage=9;
inline bool expandable(const external::unitpanel::Model&m){
 return m.selected==1&&m.passengerCount>0;
}
inline bool cargoVisible(const external::unitpanel::Model&m,bool expanded){
 return expanded&&expandable(m);
}
inline int entries(const external::unitpanel::Model&m,bool expanded){
 return m.selected==1?(cargoVisible(m,expanded)?m.passengerCount:0):m.groupCount;
}
inline int perPage(const external::unitpanel::Model&m){
 return m.selected==1?CargoPerPage:GroupPerPage;
}
inline int pages(const external::unitpanel::Model&m,bool expanded){
 return external::unitpanel::pageCount(entries(m,expanded),perPage(m));
}
inline int slots(const external::unitpanel::Model&m,bool expanded,int page){
 if(m.selected==1)return 2+(cargoVisible(m,expanded)?
   std::min(CargoPerPage,std::max(0,m.passengerCount-page*CargoPerPage)):0);
 return std::min(GroupPerPage,std::max(0,m.groupCount-page*GroupPerPage));
}
inline int cols(int slots){return slots<=1?1:slots<=4?2:3;}
inline int rows(int slots){return std::max(1,(slots+cols(slots)-1)/cols(slots));}
static_assert(PassengerCameoW<CellW&&PassengerCameoH<CellH,
              "Passenger cameo must be distinctly smaller than vehicle cameo");
}
