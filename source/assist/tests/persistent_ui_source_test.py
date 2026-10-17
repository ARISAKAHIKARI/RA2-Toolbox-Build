from pathlib import Path
root=Path(__file__).resolve().parents[1] / 'source'
main=(root/'main.cpp').read_text(encoding='utf-8')
ui=(root/'type_bindings_ui.h').read_text(encoding='utf-8')
model=(root/'type_catalog.h').read_text(encoding='utf-8')
checks={
 'catalogue populated from live game and written to disk':'rememberLiveTypes();' in main and 'motion.update(snap,d.frame)' in main and 'saveCatalogue(&dirty)' in main,
 'catalogue restored prior to UI creation':main.index('loadCatalogue();')<main.index('ui=CreateWindowExW('),
 'offline lists populated at UI creation':'ready=true;fillTech(true);fillTypeBindings(true);' in main,
 'type binding list uses durable cached values':'for(size_t i=0;i<savedTypes.types.size();++i)' in ui and 'CB_SETITEMDATA,pos,i' in ui,
 'tech uses cached values':'auto&t=savedTypes.types[i];' in main and 'static int liveTypeIndex(' in main,
 'runtime select still uses match type indexes':'typekeys::groupMask(typeBindings,binding.key,snap.types,snap.typeCount,mask,MaxTypes)' in (root/'hotkey_host.h').read_text(),
 'game process detaching does not erase catalogue':'savedTypes.types.clear()' not in main and 'savedTypes={}' not in main,
 'manual offline type ID entry':'else if(id==449)' in main,
 'old saved technology IDs survive first upgrade':'for(const auto& exceptions:profileExceptions)' in main,
 'bindings written as one multisz section':'writeSection(L"TypeHotkeys",bindingItems)' in main and 'WritePrivateProfileSectionW(' in main,
 'per-profile tech overrides retained':'chooseTechProfile((id==427)?0:1)' in main and 'profileExceptions[techProfile]=techExceptions' in main,
 'writes report failure':'bool persisted=save();' in ui,
 'persistent catalogue deliberately separated from bridge hooks':'CreateFileMapping' not in model and 'WriteProcessMemory' not in model,
}
for label,ok in checks.items():
 print(('PASS' if ok else 'FAIL')+' '+label)
assert all(checks.values())
