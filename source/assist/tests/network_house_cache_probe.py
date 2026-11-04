"""Offline generic House batching plus native MegaMission cache-miss experiment.

Runs native sender/packer/decoder with modeled interception only. No real
transport, event effects or House scheduling. AI Houses may be skipped by the
unmodified scheduler; serialization equality is not live execution evidence.
"""
from pathlib import Path
SCOPE=__doc__
support=Path(__file__).with_name('network_house_batching_probe.py')
exec(compile(support.read_text().split('mixed=[',1)[0],str(support),'exec'))

class GenericProbe(Probe):
 def __init__(self,*args,**kwargs):
  super().__init__(*args,**kwargs)
  # A clean synthetic compression state avoids borrowing another captured
  # process's dictionaries. Subsequent sends/decodes keep their real updates.
  for at,size in ((0xac50fc,64*4),(0xafa468,64*640),(0xafa338,8*4),
                  (0xafa42c,8*4),(0xac5230,8*640)):
   self.u.mem_write(at,bytes(size))
  self.misses=0;self.u.hook_add(UC_HOOK_CODE,self.hook,begin=0x64b4e8,end=0x64b4e8)
 def effective(self,slot):return bytes(self.u.mem_read(0xa802d4+slot*111+2,1))[0]
 def hook(self,u,at,size,opaque):
  if self.variant.startswith('generic'):
   if at in HOUSE_WRITES:u.reg_write(UC_X86_REG_EIP,at+7);return
   if at==0x64b4e8:
    if self.variant=='generic_nocache' and (u.reg_read(UC_X86_REG_EDX)&255)>1:
     self.misses+=1;u.reg_write(UC_X86_REG_EIP,0x64b551)
    return
   variant=self.variant
   try:self.variant='candidate';super().hook(u,at,size,opaque)
   finally:self.variant=variant
  else:super().hook(u,at,size,opaque)
 def refill(self,events,frame,mega=(2,0,0,0,0,0,0)):
  self.events=events;self.head=0;self.frame=frame;self.sent=[];self.stopped=False
  self.w(0xa8ed84,frame);self.w(0xa802c8,len(events));self.w(0xa802cc,0);self.w(0xa802d0,len(events))
  self.w(0x8b41f8,0);self.w(0x8b41fc,0);self.w(0x8b4200,0)
  for index,(kind,house,ident) in enumerate(events):
   dest=0xa802d4+index*111;self.u.mem_write(dest,bytes(111))
   self.w(0xa83a54+index*4,432100+index)
   if kind==0x16:self.run(0x4c65e0,dest,(house,kind,ident,0x34))
   elif kind==4:self.run(0x4c6860,dest,(house,ident,0x34,*mega))
   else:self.run(0x4c66c0,dest,(house,kind))

def group(house,ids):return [(4,house,ident) for ident in ids]
def canonical(b):
 # 4C6860 intentionally skips Event+0D. Native packer/decoder use that byte
 # for their dictionary slot; gameplay target starts at +0E.
 out=bytearray(b)
 if out[0]==4:out[13]=0
 return bytes(out)
def compare(m,capacity=4096):
 m.execute(capacity);local=m.local_records
 assert all(m.r(0xa70204+i*4)==(432100+i if m.protocol else 5678) for i in range(len(local)))
 frame=((m.frame+m.delay+2)//3)*3 if m.protocol==2 else m.frame+m.delay
 assert all(len(p)<=capacity and int.from_bytes(p[3:7],'little')==frame and p[7:14]==bytes.fromhex('78563412010005') for p in m.sent)
 decoded=m.decoded();remote=[b for b in decoded if b[0]!=0x1c]
 differences=[]
 for i,(l,r) in enumerate(zip(local,remote)):
  if canonical(l)!=canonical(r):differences.append({'index':i,'local':brief(l),'remote':brief(r),'offsets':[j for j in range(111) if l[j]!=r[j]]})
 if len(local)!=len(remote):differences.append({'local_count':len(local),'remote_count':len(remote)})
 return differences,local,remote

patterns={
 'same_list_0_1_0':group(0,[101,102])+group(1,[101,102])+group(0,[101,102]),
 'different_groups':group(0,[201,202,203])+group(1,[301,302])+group(0,[201,202,203])+group(1,[301,302]),
 'sell_destruct_between':group(0,[401,402])+[(0x16,1,501)]+group(1,[401,402])+[(8,0,0)]+group(0,[401,402]),
}
for protocol in (1,2):
 for name,events in patterns.items():
  def missing_cache(protocol=protocol,name=name,events=events):
   m=GenericProbe(protocol,'generic_cache',events)
   try:differences,local,remote=compare(m)
   except AssertionError as error:
    assert str(error)=='instruction budget exhausted',error
    return {'expected_divergence_without_cache_fix':True,'decoder_instruction_budget_exhausted':True,
            'stopped_at':hex(m.u.reg_read(UC_X86_REG_EIP)),'packet_lengths':[len(p) for p in m.sent]}
   assert bool(differences)==(name!='different_groups'),differences
   return {'expected_divergence_without_cache_fix':name!='different_groups','differences':differences,'packet_lengths':[len(p) for p in m.sent]}
  check(f'negative_protocol_{protocol}_{name}',missing_cache)
  def fixed(protocol=protocol,name=name,events=events):
   m=GenericProbe(protocol,'generic_nocache',events);differences,local,remote=compare(m)
   assert not differences,differences
   assert m.misses>0
   return {'all_event_bytes_equal_except_native_cache_slot_0d':True,'modeled_query_misses':m.misses,'packet_lengths':[len(p) for p in m.sent],
           'raw_cache_slot_pairs':[[l[13],r[13]] for l,r in zip(local,remote) if l[0]==4],
           'packet_houses':[p[2] for p in m.sent],'records':[brief(b) for b in remote]}
  check(f'fixed_protocol_{protocol}_{name}',fixed)
 for name in ('repeated_20_frames','on_off_on'):
  def sustained(protocol=protocol,name=name):
   events=patterns['same_list_0_1_0'];m=GenericProbe(protocol,'generic_nocache',events);traces=[]
   for turn in range(20 if name=='repeated_20_frames' else 6):
    # OFF returns the entire sender to native rewriting/cache behavior. No
    # cache reset is performed, so re-enabling must tolerate retained state.
    m.variant='vanilla' if name=='on_off_on' and turn in (1,2,4) else 'generic_nocache'
    m.refill(events,1234+turn*3);differences,local,remote=compare(m)
    assert not differences,(turn,m.variant,differences)
    traces.append({'turn':turn,'variant':m.variant,'packets':len(m.sent),'local_count':len(local),
                   'remote_count':len(remote),'houses':[p[2] for p in m.sent]})
   return {'all_event_bytes_equal_except_native_cache_slot_0d':True,'rounds':traces,'modeled_query_misses':m.misses}
  check(f'sustained_protocol_{protocol}_{name}',sustained)

 def original_metadata(protocol=protocol):
  events=group(0,[101,102])+[(0x16,0,501)]+group(0,[201,202])+[(8,0,0)]+group(0,[101,102])
  m=GenericProbe(protocol,'vanilla',events);differences,local,remote=compare(m)
  assert not differences
  raw=[{'index':i,'offsets':[j for j in range(111) if l[j]!=r[j]],'local':l[13],'remote':r[13]} for i,(l,r) in enumerate(zip(local,remote)) if l!=r]
  assert raw and all(x['offsets']==[13] for x in raw),raw
  return {'original_sender_also_has_cache_slot_metadata_difference':raw}
 check(f'original_cache_metadata_protocol_{protocol}',original_metadata)

 def toggle_negative(protocol=protocol):
  m=GenericProbe(protocol,'generic_nocache',group(1,[901,902]));differences,_,_=compare(m);assert not differences
  m.variant='vanilla';m.refill(group(0,[901,902]),1237)
  try:differences,_,_=compare(m)
  except AssertionError as error:
   assert str(error)=='instruction budget exhausted'
   return {'expected_stale_foreign_dictionary_after_disable':True,'decoder_instruction_budget_exhausted':True}
  assert differences,'expected stale foreign dictionary reuse'
  return {'expected_stale_foreign_dictionary_after_disable':True,'differences':differences}
 check(f'toggle_without_invalidation_protocol_{protocol}',toggle_negative)

 def toggle_fixed(protocol=protocol):
  m=GenericProbe(protocol,'generic_nocache',group(1,[901,902]));turns=[]
  for turn in range(10):
   enabled=turn%2==0
   if not enabled:m.u.mem_write(0xafa338,bytes(8*4)) # native sender dictionary counts only
   m.variant='generic_nocache' if enabled else 'vanilla'
   m.refill(group(1 if enabled else 0,[901,902]),1234+turn*3)
   differences,local,remote=compare(m);assert not differences,(turn,differences)
   turns.append({'turn':turn,'enabled':enabled,'house':remote[0][2],'event_count':len(remote)})
  return {'sender_counts_invalidated_before_disable':True,'all_event_bytes_equal_except_native_cache_slot_0d':True,'rounds':turns}
 check(f'toggle_with_sender_invalidation_protocol_{protocol}',toggle_fixed)

 for capacity in (39,43,44,49):
  def bounded(protocol=protocol,capacity=capacity):
   m=GenericProbe(protocol,'generic_nocache',patterns['same_list_0_1_0'],head=126)
   differences,local,remote=compare(m,capacity);assert not differences,differences
   assert len(local)==len(remote)==6 and m.r(0xa802c8)==0
   return {'capacity':capacity,'packet_lengths':[len(p) for p in m.sent],
           'event_order_and_all_fields_except_cache_slot_equal':True,'ring_head_after':m.r(0xa802cc)}
  check(f'mega_capacity_{capacity}_protocol_{protocol}',bounded)
 def first_too_large(protocol=protocol):
  m=GenericProbe(protocol,'generic_nocache',group(1,[701,702]),stop_send=True).execute(38)
  assert m.stopped and m.r(0xa802c8)==2 and m.r(0xa802cc)==0 and m.r(0x8b41f8)==0
  assert [m.out(i) for i in range(2)]==m.before
  return {'capacity':38,'no_outlist_or_local_dolist_consumption':True,'stopped_at_first_send':True}
 check(f'mega_insufficient_capacity_protocol_{protocol}',first_too_large)
 def refs(protocol=protocol):
  events=patterns['same_list_0_1_0'];m=GenericProbe(protocol,'generic_nocache',events)
  m.refill(events,1234,(1,12345,0x34,30020,11,12346,0x34))
  differences,local,remote=compare(m);assert not differences,differences
  assert all(p[12]==1 and p[14:29]==struct.pack('<IBIBIB',12345,0x34,30020,11,12346,0x34) for p in remote)
  return {'attack_and_nonzero_target_destination_nearest_references_equal':True}
 check(f'mega_reference_fields_protocol_{protocol}',refs)

Path(a.output).write_text(json.dumps({'scope':SCOPE,'game_fixture_sha256':hashlib.sha256(raw).hexdigest(),'results':rows},indent=2)+'\n')
raise SystemExit(any(row['status']!='PASS' for row in rows))
