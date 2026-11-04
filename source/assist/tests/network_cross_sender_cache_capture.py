"""Native MegaMission dictionary collision across two senders and a third decoder.
Offline synthetic senders/receiver. Foreign House preservation and forced
encoder cache misses are modeled using the prior probe's interceptions.
No DLL, socket, task effects, or actual multiplayer clients are executed.
"""
from pathlib import Path
PROBE_SCOPE=__doc__
source=Path(__file__).with_name('network_house_cache_probe.py')
__file__=str(source)
exec(compile(source.read_text().split('patterns={',1)[0],str(source),'exec'))
def ids(records):return [int.from_bytes(x[7:11],'little') for x in records if x[0]==4]
def receive(c,packets):
 c.sent=list(packets)
 return [x for x in c.decoded() if x[0]!=0x1c]
def cache(c,house=1,slot=0):
 index=house*8+slot;n=c.r(0xac50fc+index*4)
 return {'slot':slot,'house':house,'count':n,'ids':[c.r(0xafa468+index*640+i*5) for i in range(n)]}
rows=[]
for protocol in (1,2):
 for intervening_foreign in (False,True):
  b=GenericProbe(protocol,'vanilla',group(1,[201,202]),local=1)
  foreign=GenericProbe(protocol,'generic_nocache',group(1,[101,102]),local=0)
  c=GenericProbe(protocol,'vanilla',[])
  b.execute();first_b_packets=list(b.sent)
  assert ids(b.local_records)==[201,202]
  assert ids(receive(c,first_b_packets))==[201,202]
  after_b=cache(c);assert after_b['ids']==[201,202]
  if intervening_foreign:
   foreign.execute();foreign_packets=list(foreign.sent)
   assert ids(foreign.local_records)==[101,102]
   assert ids(receive(c,foreign_packets))==[101,102]
   after_foreign=cache(c);assert after_foreign['ids']==[101,102]
  else:foreign_packets=[];after_foreign=None
  b.refill(group(1,[201,202]),1237);b.execute()
  local=ids(b.local_records);remote=ids(receive(c,b.sent))
  assert local==[201,202]
  assert remote==([101,102] if intervening_foreign else [201,202]),remote
  assert len(b.sent[0])<len(first_b_packets[0]),'expected real native dictionary reuse'
  rows.append({'protocol':protocol,'foreign_sender_intervenes':intervening_foreign,
   'real_house1_sender_first_packet_lengths':[len(x) for x in first_b_packets],
   'foreign_house1_sender_packet_lengths':[len(x) for x in foreign_packets],
   'real_house1_sender_cached_packet_lengths':[len(x) for x in b.sent],
   'receiver_house1_slot0_after_real_sender':after_b,'receiver_house1_slot0_after_foreign_sender':after_foreign,
   'real_sender_local_source_ids':local,'third_receiver_decoded_source_ids':remote,
   'source_id_mismatch':local!=remote,'status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'game_fixture_sha256':hashlib.sha256(raw).hexdigest(),
 'supported_source_exe_sha256':'7b8a068535d6af06845edf95ae829b113d00c02909330e16f197426cd7db94b6',
 'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
print('PASS',len(rows),'native cross-sender cache cases; third-receiver source substitution reproduced')
