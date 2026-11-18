"""Source contract for saved, parent-gated native-unit reservation mode.

The dynamic host test separately exercises the actual collectChoices, 128
reservation FIFO, native FactoryClass capacity, and event submission path.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]/"source"
additional=(root/"additional_settings.h").read_text()
settings=(root/"extra_settings_io.h").read_text()
ui=(root/"tactical_controls.h").read_text()
handler=(root/"auto_settings_ui.h").read_text()
dependencies=(root/"settings_dependencies.h").read_text()
host=(root/"construction_host.h").read_text()
assert "bool constructionShowNativeUnits=false" in additional
assert 'extra.constructionShowNativeUnits=value(L"Production",L"ReserveShowAvailableUnits",0)!=0;' in settings
assert 'write(L"Production",L"ReserveShowAvailableUnits",extra.constructionShowNativeUnits);' in settings
assert 'toggle(9796,L"显示已可生产单位（批量预定）",extra.constructionShowNativeUnits)' in ui
assert "if(id==9796&&notification==BN_CLICKED)" in handler
assert "changedSetting();return true;" in handler[handler.index("if(id==9796&&notification==BN_CLICKED)"):][:220]
assert "case 817:case 9796:return require(s,{Need::Construction});" in dependencies
assert "9180,895,817,9796,9096" in dependencies
assert "construction::showReservationChoice(tab,native,queued,future,busyBuilding,extra.constructionShowNativeUnits)" in host
assert "if(!source.complete())choices=std::move(previous);" in host
assert "if(!nativeReady){choices=std::move(previous);return;}" in host
print("PASS saved unit reservation filter: default off, INI round-trip, UI auto-save, parent dependency, native-read failure retention")
