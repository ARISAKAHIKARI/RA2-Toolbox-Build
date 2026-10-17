from pathlib import Path
r=Path(__file__).resolve().parents[1]/'source'
s=(r/'main.cpp').read_text(encoding='utf-8')
assert 'techDirectProduce=value(L"Tech",L"DirectProduce",0)!=0;' in s
assert 'if(!techDirectProduce)notify(' in s
assert 'externalReader.lastOpenError' in s
assert 'externalReader.lastReadError' in s
assert 'externalReader.lastRegionState' in s
reader=(r/'../../external_reader/source/process_reader.h').read_text(encoding='utf-8')
assert 'VirtualQueryEx(' in reader
connection=(r/'../../external_reader/source/process_connection.h').read_text()
assert 'PROCESS_QUERY_LIMITED_INFORMATION|(readable?PROCESS_VM_READ:0)' in connection
assert 'WriteProcessMemory(' not in reader
print('PASS: runtime DirectProduce setting, read-only LBW error diagnostics')

bridge=(r/'bridge.cpp').read_text();world=(r/'external_orders_bridge.h').read_text()
assert bridge.index('lastFrame=frame;snap.frame=frame;')<bridge.index('const bool planningChanged=publishPlanningWorld')
assert '!summaryChanged&&!planningChanged' in bridge
assert 'notifyHost()' not in world.split('static bool publishPlanningWorld')[1].split('static bool rawOreCompatible')[0]
assert s.index('w.frame>snap.frame')<s.index('worldRevision=seq;')
assert 'techVisible=' in s and 'planner reason=' in s
