#!/usr/bin/env python3
"""Source-bound contract: cargo expander must actually change the unit panel."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
host=(root/"source/unit_info_panel_host.h").read_text(encoding="utf-8")
metrics=(root/"source/unit_info_panel_layout.h").read_text(encoding="utf-8")
assert 'bool expandableCargo()const{return panelmetrics::expandable(model_);}' in host
assert 'int slots()const{return panelmetrics::slots(model_,details_,page_);}' in host
assert 'int pages()const{return panelmetrics::pages(model_,details_);}' in host
assert 'if(expandableCargo()){' in host
assert 'details_=!details_;page_=0;focused_=-1;' in host
assert 'if(!expandableCargo())details_=false;' in host
assert 'const bool differentUnit=' in host and 'details_=false;' in host
assert 'page_*panelmetrics::CargoPerPage+slot-2' in host
assert 'panelmetrics::PassengerCameoW,panelmetrics::PassengerCameoH' in host
assert 'x+(CellW-panelmetrics::PassengerCameoW)/2' in host
assert 'if(model_.selected==1)' in host
assert 'if(right||(!folded_&&y<foldY-2))' not in host
assert 'label(g,L"i"' not in host
assert 'if(details_){' not in host
assert 'return Pad*2+rows()*CellH+(rows()-1)*Gap+(pages()>1?22:0)+6;' in host
assert 'if(folded_)details_=false;' in host
assert 'return m.selected==1?(cargoVisible(m,expanded)?m.passengerCount:0):m.groupCount;' in metrics
print("PASS unit panel cargo button is actionable, cargo hidden by default, passengers use smaller Cameos, useless info footer removed")
