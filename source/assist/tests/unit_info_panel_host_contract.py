#!/usr/bin/env python3
"""Source-bound contract: expanded unit panel automatically shows cargo."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
host=(root/"source/unit_info_panel_host.h").read_text(encoding="utf-8")
metrics=(root/"source/unit_info_panel_layout.h").read_text(encoding="utf-8")
assert 'int slots()const{return panelmetrics::slots(model_,!folded_,page_);}' in host
assert 'int pages()const{return panelmetrics::pages(model_,!folded_);}' in host
assert 'int entries()const{return panelmetrics::entries(model_,!folded_);}' in host
assert 'if(folded_)return;' in host
assert 'folded_=!folded_;page_=0;focused_=-1;' in host
assert 'details_' not in host and 'expandableCargo' not in host
assert 'page_*panelmetrics::CargoPerPage+slot-2' in host
assert 'panelmetrics::PassengerCameoW,panelmetrics::PassengerCameoH' in host
assert 'x+(CellW-panelmetrics::PassengerCameoW)/2' in host
assert 'if(model_.selected==1)' in host
assert 'label(g,L"i"' not in host
assert 'return m.selected==1?(cargoVisible(m,expanded)?m.passengerCount:0):m.groupCount;' in metrics
assert 'return 2+(cargoVisible(m,expanded)?' in metrics
assert 'PassengerCameoW=44,PassengerCameoH=36' in metrics
print("PASS expanded panel automatically shows cargo; folding hides cargo; passenger Cameos are smaller; redundant info footer removed")
