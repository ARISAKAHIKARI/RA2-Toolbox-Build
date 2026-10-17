from pathlib import Path
r=Path(__file__).resolve().parents[1]/'source'
root=r.parents[1]
code=(r/'bridge.cpp').read_text()
lease=(r/'unit_task_arbiter.h').read_text()
protocol=(r/'protocol.h').read_text()
assert 'Version=34' in protocol and 'RA2Assist_v040_' in code
assert 'sessionTracker.update(true,house,sessionMode,frame)' in code
assert 'actorLeaseArbiter.canIssue(' in code and 'actorLeaseArbiter.record(' in code
assert 'releaseAutoOwner' in code and 'cancelReceiptTicket' in code
assert 'LeaseDurationFrames=45' in lease and 'Owner::Magnetron' in lease
assert 'missionObserver.dismissTicket(' in (r/'autoload_bridge.h').read_text()
assert 'o.purpose==3?taskarbiter::Owner::Magnetron' in (r/'external_orders_bridge.h').read_text()
assert 'writeInputCodeGuarded' in (r/'input_patch.h').read_text()
for filename in ('auto_cs_bridge.h','placement_bridge.h','native_input_hooks.h','cooperative_bridge.h'):
 s=(r/filename).read_text()
 assert 'writeInputCodeGuarded' in s, filename
for filename in ('Start-Frame.cmd','Start-Message.cmd'):
 s=(root/filename).read_text()
 assert 'BUILD-VERIFIED-v0713.tag' in s and 'BUILD-0.7-ON-WINDOWS.cmd' in s
assert 'BUILD-VERIFIED-v0713.tag' in (root/'tools/verify_windows_outputs.py').read_text()
print('PASS: v0.5.8 session epoch, cross-frame leases, owner receipts, byte-guarded hooks and synchronized launch/build markers')
