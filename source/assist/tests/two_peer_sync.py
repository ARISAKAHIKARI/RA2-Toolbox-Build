#!/usr/bin/env python3
"""Two isolated real x86 senders plus an explicitly simplified lockstep model.

Real: released DLLs, unified manager, original YR constructors/OutList append.
Modeled: transport barrier/retry and receiver command state (NOT YR simulation).
Never equate a matching model hash with passing actual multiplayer gameplay.
"""
import argparse
import copy
import ctypes as C
import hashlib
import importlib.util
import json
from pathlib import Path
import random
import struct
import traceback
from unicorn.x86_const import UC_X86_REG_EDX, UC_X86_REG_EAX

HERE=Path(__file__).resolve().parent
def digest(value):
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()

class Peer:
    def __init__(self,game,dll,player,mode=3,backend=0):
        spec=importlib.util.spec_from_file_location('fixture_'+str(player),HERE/'entry_runtime_regression.py')
        self.m=importlib.util.module_from_spec(spec)
        self.m.fixture_paths=[str(game),str(dll)]
        spec.loader.exec_module(self.m)
        m=self.m;self.player=player;self.seq=0;self.engine_stubs=True
        self.houses=[m.house,m.enemy]
        m.w(m.enemy+0x5788,2);m.w(m.enemy+0x30c,10000)
        self.objects=[m.unit,m.eng,m.building,m.enemyUnit,
                      m.obj(0x7eb058,m.engT,m.enemy,14),m.obj(0x7e3ebc,m.buildT,m.enemy,15,50)]
        self.byid={m.r(p+0x10):p for p in self.objects}
        self.owned=self.objects[:3] if player==0 else self.objects[3:]
        for h in self.houses:
            country=m.alloc();m.w(h+0x34,country)
        m.vector(0xa8ec78,self.objects);m.vector(0xa8eb40,[self.objects[2],self.objects[5]])
        m.vector(0xa83de8,[self.objects[1],self.objects[4]])
        m.vector(0xa83d10,[self.objects[0],self.objects[3]])
        m.vector(0xa8ecb8,[self.owned[0],self.owned[1]])
        for p in self.objects:m.b(p+0x83,p in self.owned[:2])
        m.w(0xa83d4c,self.houses[player]);m.w(0xa8b238,mode)
        m.u.mem_write(m.t+0x388,struct.pack('<d',4.0))
        m.u.mem_write(m.engT+0x380,struct.pack('<d',1.0))
        m.used_sizes={self.objects[0]:0,self.objects[3]:0}
        # Both sides have an ordinary weapon. Its queries remain read-only.
        weapon=m.alloc();projectile=m.alloc();warhead=m.alloc()
        m.w(weapon+0xb4,16384);m.w(weapon+0xa0,projectile);m.w(weapon+0xac,warhead)
        m.b(projectile+0x2a5,1);m.b(projectile+0x2a4,1)
        for i in range(11):m.u.mem_write(warhead+0xa0+i*8,struct.pack('<d',1.0))
        m.w(m.t+0x898,weapon);m.w(m.engT+0x898,weapon)
        self.weapon=weapon;self.projectile=projectile;self.warhead=warhead
        self.set(flags=0,megaQueueMode=backend,objectQueueMode=backend)
        m.hook()
        assert self.state().data.status==1
        self.clear_queues()
        self.model={'objects':{str(m.r(p+0x10)):{'owner':m.r(m.r(p+0x21c)+0x30),
                     'hp':m.r(p+0x6c),'mission':m.r(p+0xac),'repairing':0,'target':0,'destination':0}
                     for p in self.objects},'production':[],'supers':[]}
    def state(self):
        m=self.m;return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
    def set(self,**values):
        m=self.m;s=self.state()
        for key,value in values.items():setattr(s,key,value)
        m.u.mem_write(m.SH,bytes(s));return s
    def save(self,s):self.m.u.mem_write(self.m.SH,bytes(s))
    def clear_queues(self):
        for addr in [0xa802c8,0xa802cc,0xa802d0,0x8b41f8,0x8b41fc,0x8b4200]:self.m.w(addr,0)
    def advance(self,frames=1):
        self.m.w(0xa8ed84,self.m.r(0xa8ed84)+frames)
    def tick(self):return self.m.hook()
    def issue(self,command,arg=0,arg2=0):
        m=self.m;s=self.state();s.command=command;s.arg=arg;s.arg2=arg2;s.request+=1
        s.requestTick=m.clock;s.requestMatch=s.data.match;self.save(s)
        s=self.tick();return C.c_int32(s.result).value,s
    def protected(self):
        # Entire synthetic objects/types/houses/weapon data are watched, except
        # object selected byte, which is UI-local. Excludes pointers/queues/UI.
        result={}
        for p in self.objects:
            data=bytearray(self.m.u.mem_read(p,0x2000));data[0x83]=0;result[hex(p)]=bytes(data).hex()
        for p,n in [(h,0x20000) for h in self.houses]+[(p,0x2000) for p in (self.m.t,self.m.engT,self.m.buildT)]+[(p,4096) for p in (self.weapon,self.projectile,self.warhead)]:
            result[hex(p)]=bytes(self.m.u.mem_read(p,n)).hex()
        return digest(result)
    def drain(self):
        m=self.m;count=m.r(0xa802c8);head=m.r(0xa802cc)
        assert count<=112 and head<128,(count,head)
        assert m.r(0x8b41f8)==0,'MP sender bypassed transport via DoList'
        result=[]
        for i in range(count):
            packet=bytes(m.u.mem_read(0xa802d4+((head+i)&127)*111,111))
            assert packet[2]==self.player,('foreign sender house',packet[2],self.player)
            self.seq+=1;result.append({'source':self.player,'seq':self.seq,'packet':packet.hex()})
        m.w(0xa802cc,(head+count)&127);m.w(0xa802c8,0)
        return result
    def native_move(self,mission=2):
        m=self.m;target=m.alloc(16);destination=m.alloc(16)
        m.w(destination,110110);m.b(destination+4,11)
        m.u.reg_write(UC_X86_REG_EDX,target)
        m.run(0x646e90,(m.r(self.owned[0]+0x10),0x34,destination),self=mission)
    def close(self):
        # Explicit release keeps repeated PE fixtures bounded in long suites.
        self.m.u=None

def decode(packet):
    raw=bytes.fromhex(packet);kind,executed,house=raw[:3]
    out={'kind':kind,'house':house,'frame':struct.unpack_from('<I',raw,3)[0]}
    if kind in (0x15,0x16):out.update(actor=struct.unpack_from('<I',raw,7)[0],tag=raw[11])
    elif kind==4:
        out.update(actor=struct.unpack_from('<I',raw,7)[0],tag=raw[11],mission=raw[12],
                   target=struct.unpack_from('<I',raw,14)[0],target_tag=raw[18],
                   destination=struct.unpack_from('<I',raw,19)[0],destination_tag=raw[23])
    elif kind==0x0e:out['type']=list(struct.unpack_from('<iii',raw,7))
    elif kind==0x12:out['payload']=raw[7:15].hex()
    else:raise AssertionError('unmodeled event '+hex(kind))
    return out

def apply_model(model,envelope):
    # Command-state oracle only: does not model movement, weapons, damage,
    # pathfinding, economy, desync CRC, or real networking.
    e=decode(envelope['packet']);assert e['house']==envelope['source']
    if 'actor' in e:
        actor=model['objects'][str(e['actor'])];assert actor['owner']==e['house']
        assert e['tag']==0x34
        if e['kind']==0x15:actor['repairing']^=1
        elif e['kind']==0x16:actor['sold']=True
        elif e['kind']==4:
            actor['mission']=e['mission'];actor['target']=e['target'];actor['destination']=e['destination']
    elif e['kind']==0x0e:model['production'].append([e['house'],e['type']])
    elif e['kind']==0x12:model['supers'].append([e['house'],e['payload']])

def deliver(peers,events,seed=7):
    # Explicit complete-frame barrier; simulated jitter/reorder/retransmission.
    # These are harness assumptions, not an implementation of CnCNet/LBW.
    expected={(e['source'],e['seq']) for e in events};traces=[]
    for i,p in enumerate(peers):
        incoming=events+events[::2];random.Random(seed+i).shuffle(incoming)
        unique={}
        for envelope in incoming:
            key=(envelope['source'],envelope['seq'])
            if key in unique:assert unique[key]['packet']==envelope['packet']
            unique[key]=envelope
        assert set(unique)==expected
        ordered=[unique[key] for key in sorted(unique)]
        for e in ordered:apply_model(p.model,e)
        traces.append([decode(e['packet']) for e in ordered])
    assert traces[0]==traces[1]
    assert digest(peers[0].model)==digest(peers[1].model),'modeled command state diverged'
    return {'events':traces[0],'model_hash':digest(peers[0].model),'transport':'modeled barrier, shuffle and duplicate suppression'}

def run_case(game,dll,name,action,mode=3,backend=0,expected=None):
    peers=[Peer(game,dll,i,mode,backend) for i in range(2)]
    try:
        if hasattr(action,'prepare'):action.prepare(peers)
        before=[p.protected() for p in peers]
        details=action(peers) or {}
        for i,p in enumerate(peers):assert p.protected()==before[i],f'local simulation-data write on peer {i}'
        events=[e for p in peers for e in p.drain()]
        if expected is not None:assert len(events)==expected,(name,len(events),expected,details)
        sync=deliver(peers,events)
        return {'name':name,'status':'PASS','mode':mode,'backend':backend,
                'protected_memory_unchanged':True,**details,**sync}
    finally:
        for p in peers:p.close()

def main():
    parser=argparse.ArgumentParser();parser.add_argument('game',type=Path);parser.add_argument('dll',type=Path)
    parser.add_argument('--output',type=Path,default=Path('two-peer-results.json'))
    parser.add_argument('--filter',default='');args=parser.parse_args()
    cases=[]
    def add(name,action,expected=0,mode=3,backend=0):cases.append((name,action,mode,backend,expected))
    def commands(command,arg=0,both=True):
        def action(ps):
            results=[]
            for p in ps if both else ps[:1]:
                r,s=p.issue(command,arg);assert r==1,(command,r);assert s.ack==s.request
                results.append({'player':p.player,'result':r,'count':s.count})
            return {'commands':results}
        return action
    for mode in (3,4):
        for backend in (0,1):
            for command,name in [(1,'repair'),(15,'auto_board')]:
                add(f'{name}_both_mode{mode}_backend{backend}',commands(command),2,mode,backend)
    add('repair_only_one_enabled',commands(1,both=False),1)
    def automatic(ps):
        ps[0].set(flags=64,repairMs=500);ps[0].tick()
        for i in range(12):
            for p in ps:p.advance();p.tick()
    add('auto_repair_asymmetric_repeated_ticks',automatic,1)
    for command,name in [(2,'combat_selection'),(3,'empty_transports'),(4,'damaged_selection'),
                         (5,'idle_harvester'),(6,'idle_engineer'),(7,'idle_transport'),
                         (8,'clear_beacons'),(9,'reveal_map'),
                         (16,'type_selection'),(18,'cancel_automatic'),(19,'weapon_variant_selection')]:
        add(name,commands(command))
    def tech_guard(ps):
        for p in ps:
            r,s=p.issue(10,1);assert r==-13 and not s.data.techOpen
        return {'restriction':'Native technology sidebar shares the existing experimental multiplayer guard'}
    add('technology_native_sidebar_default_guard',tech_guard)
    def production(ps):
        out=[]
        for p in ps:
            s=p.state();s.techAllowed[1]=1;p.save(s);assert p.issue(10,1)[0]==-13
            r,s=p.issue(12,1);assert r==-13
            out.append(r)
        return {'restriction':'experimental production remains blocked by default','results':out}
    add('production_default_guard',production)
    def experimental_production(ps):
        for p in ps:
            p.set(flags=4096);s=p.state();s.techAllowed[1]=1;p.save(s)
            assert p.issue(10,1)[0]==1;assert p.issue(12,1)[0]==1
        return {'limitation':'Only packet path; production authorization/economy not simulated'}
    add('production_explicit_experimental',experimental_production,2)
    for bit,name in [(1,'capacity'),(2,'iron'),(4,'production_notice'),(16,'psychic'),(32,'crates'),
                     (524288,'frozen'),(512,'auto_cs_install'),(131072,'custom_cs_install'),
                     (8192|1024,'advanced_install'),(262144|1024,'magnetron_install'),
                     (32768,'ore_reader'),(2048,'ejection_watch_empty'),(1048576,'enter_install'),
                     (2097152,'air_attack_install'),(4194304,'disk_install'),
                     (8388608,'drained_sell_install'),(16777216,'vehicle_sell_install'),
                     (33554432,'enemy_sell_guard'),(65536,'experimental_placement_opt_in')]:
        def flags(ps,bit=bit):
            ps[0].set(flags=bit)
            for i in range(3):
                for p in ps:p.advance();p.tick()
            s=ps[0].state()
            if bit==65536:assert s.data.placementStatus==1 # code-only opt-in, not a multiplayer gameplay safety test
            return {'flags':bit,'csHookStatus':s.data.csHookStatus,'nativeHookStatus':s.data.nativeHookStatus,
                    'clickHookStatus':s.clickHookStatus,'placementStatus':s.data.placementStatus,
                    'scope':'enable/tick only; not complete gameplay effect'}
        add(name,flags)
    def chat(ps):
        for p in ps:
            p.m.b(0xa8bc60+25,1);assert p.issue(1)[0]==-8
    add('chat_rejects_commands',chat)
    def stale(ps):
        for p in ps:
            s=p.state();s.command=1;s.request+=1;s.requestMatch=s.data.match+1;s.requestTick=p.m.clock;p.save(s)
            s=p.tick();assert C.c_int32(s.result).value==-7
    add('old_match_request_rejected',stale)
    def expired(ps):
        for p in ps:
            s=p.state();s.command=1;s.request+=1;s.requestMatch=s.data.match;s.requestTick=p.m.clock-4000;p.save(s)
            s=p.tick();assert C.c_int32(s.result).value==-7
    add('expired_request_rejected',expired)
    def repeat(ps):
        for p in ps:
            assert p.issue(1)[0]==1
            for i in range(5):p.tick()
    add('mailbox_same_serial_not_replayed',repeat,2)
    def cs(ps):
        for p in ps:
            s=p.set(flags=512);s.csSeq=2;s.csMatch=s.data.match
            for i in range(512):s.csAllowed[i]=1
            p.save(s);p.tick();assert p.state().data.csHookStatus==1
            p.native_move()
        return {'scope':'real patched native local input function and original OutList append'}
    add('auto_cs_native_input_both',cs,2)
    def asymmetric_cs(ps):
        # A local input feature must not need the remote peer to install its hook.
        p=ps[0];s=p.set(flags=512);s.csSeq=2;s.csMatch=s.data.match
        for i in range(512):s.csAllowed[i]=1
        p.save(s);p.tick();assert p.state().data.csHookStatus==1
        assert ps[1].state().data.csHookStatus==0
        for i,p in enumerate(ps):
            p.native_move()
            raw=bytes(p.m.u.mem_read(0xa802d4,111));event=decode(raw.hex())
            assert event['mission']==(29 if i==0 else 2),event
            assert event['target']==0 and event['destination']==110110 and event['destination_tag']==11,event
            assert p.m.r(0xa802c8)==1, 'input duplicated or dropped'
        return {'scope':'only peer 0 installs CS; native packets are AttackMove and Move respectively; both receive the same merged event stream',
                'original_receiver_executed':False,'full_game_sync_tested':False}
    for mode in (3,4):
        add('auto_cs_only_one_peer_mode'+str(mode),asymmetric_cs,2,mode)

    def custom_cs(ps):
        for p in ps:
            p.set(flags=131072,csHoldKey=67);p.m.held_keys.add(67);p.tick();p.native_move()
        return {'scope':'real configured-key CS interception'}
    add('custom_cs_native_input_both',custom_cs,2)
    def advanced(ps):
        for p in ps:
            p.set(flags=8192|1024,advancedKey=71);p.m.held_keys.add(71);p.tick();p.native_move()
            p.advance();p.tick();assert p.state().data.advancedUnits==1
        return {'scope':'input capture + initial mission; subsequent targeting separately tested'}
    add('advanced_move_native_input_both',advanced,2)
    def combined(ps):
        flags=(1|2|4|16|32|64|512|1024|2048|8192|16384|32768|131072|262144|524288|1048576|2097152|4194304|8388608|16777216)
        p=ps[0];p.set(flags=flags);p.tick()
        assert p.issue(15)[0] in (1,2)
        s=p.state();assert s.data.csHookStatus==1 and s.data.nativeHookStatus==1 and s.clickHookStatus==1
        for i in range(5):p.advance();p.tick()
        return {'scope':'combined automation + all local input hooks; other peer disabled'}
    add('combined_features_asymmetric',combined,2)
    def stalled_repair(ps):
        ps[0].set(flags=64,repairMs=500)
        for i in range(65):
            for p in ps:p.advance();p.tick()
        return {'scope':'7.15 seconds of unchanged repair state; no receiver observation yet'}
    add('repair_unobserved_delivery_not_toggled_again',stalled_repair,1)
    def full_queue(ps):
        for p in ps:
            p.m.w(0xa802c8,112);p.issue(1)
            for i in range(30):p.advance();p.tick()
            s=p.state();assert s.ack==s.request and C.c_int32(s.result).value==-6 and s.count==0
            assert p.m.r(0xa802c8)==112
            p.clear_queues();p.advance();p.tick()
        return {'scope':'backpressure timeout drops unsent repairs; no delayed surprise submission'}
    add('queue_full_timeout_no_late_repair',full_queue)
    def ring(ps):
        for p in ps:
            p.m.w(0xa802cc,127);p.m.w(0xa802d0,127);assert p.issue(1)[0]==1
            assert p.m.r(0xa802d0)==0
    add('outlist_wrap_at_127',ring,2)
    def timer_failure(ps):
        for p in ps:
            p.m.w(0x7e1530,0);p.issue(1)
            for i in range(30):p.advance();p.tick()
            assert p.state().data.lastSubmitError==9
    add('missing_game_timer_no_packet',timer_failure)
    def unloaded_setup(ps):
        for p in ps:
            carrier,passenger=p.owned[:2];p.m.w(carrier+0x114,1);p.m.w(carrier+0x118,passenger)
            p.m.w(passenger+0x11c,carrier);p.m.used_sizes[carrier]=1
    def unload(ps):
        for p in ps:
            p.set(flags=16384|1024);r,s=p.issue(17);assert r==1 and s.count==1
        return {'scope':'Unload events; disembark motion and AI remain engine-dependent'}
    unload.prepare=unloaded_setup
    add('smart_unload_both',unload,2)
    def coop(ps):
        for p in ps:
            p.set(flags=1048576);p.tick();s=p.state()
            s.cooperativePlan=1;s.cooperativeSerial=1;s.cooperativeTick=p.m.clock;s.cooperativeFrame=p.m.r(0xa8ed84);s.cooperativeMatch=s.data.match;s.cooperativeMode=1;s.cooperativeCount=1
            s.cooperativeRequest.serial=1;s.cooperativeRequest.match=s.data.match;s.cooperativeRequest.mode=1;s.cooperativeRequest.tick=p.m.clock
            s.cooperativeOrders[0].id=p.m.r(p.owned[1]+0x10);s.cooperativeOrders[0].target=p.m.r(p.owned[0]+0x10)
            s.cooperativeOrders[0].type=p.m.t;p.save(s);p.tick()
            assert p.state().cooperativeAck==1 and p.state().cooperativeAccepted==1
        return {'scope':'external cooperative mailbox -> live validation -> Enter event'}
    add('cooperative_enter_both',coop,2)
    def foreign_coop(ps):
        for p in ps:
            p.set(flags=1048576);p.tick();s=p.state()
            s.cooperativePlan=1;s.cooperativeSerial=1;s.cooperativeTick=p.m.clock;s.cooperativeFrame=p.m.r(0xa8ed84);s.cooperativeMatch=s.data.match;s.cooperativeMode=1;s.cooperativeCount=1
            s.cooperativeRequest.serial=1;s.cooperativeRequest.match=s.data.match;s.cooperativeRequest.mode=1;s.cooperativeRequest.tick=p.m.clock
            s.cooperativeOrders[0].id=p.m.r(p.objects[4 if p.player==0 else 1]+0x10)
            s.cooperativeOrders[0].target=p.m.r(p.owned[0]+0x10);s.cooperativeOrders[0].type=p.m.t
            p.save(s);p.tick();assert p.state().cooperativeRejected==1
    add('cooperative_foreign_actor_rejected',foreign_coop)
    def supers_setup(ps):
        for p in ps:
            m=p.m;st=m.alloc();sw=m.alloc();m.w(sw+0x28,st);m.w(sw+0x2c,p.houses[p.player]);m.b(sw+0x6d,1);m.b(sw+0x6f,1)
            m.w(st+0x98,0);m.w(st+0xbc,0);m.vector(p.houses[p.player]+0x254,[sw])
    super_command=commands(11);super_command.prepare=supers_setup
    add('non_positional_super_event',super_command,2)
    def mixed_backends(ps):
        ps[0].set(megaQueueMode=1,objectQueueMode=1)
        for p in ps:assert p.issue(15)[0]==1
    add('different_backends_same_mission_semantics',mixed_backends,2)
    def collision_setup(ps):
        for p in ps:
            m=p.m
            for owner,ident in [(m.house,4108),(m.enemy,4111)]:
                obj=m.obj(0x7e3ebc,m.buildT,owner,ident,50);p.objects.append(obj);p.byid[ident]=obj
                p.model['objects'][str(ident)]={'owner':m.r(owner+0x30),'hp':50,'mission':5,'repairing':0,'target':0,'destination':0}
            m.vector(0xa8ec78,p.objects)
            m.vector(0xa8eb40,[x for x in p.objects if m.r(x)==0x7e3ebc])
    def collision(ps):
        ps[0].set(flags=64,repairMs=500)
        for i in range(12):
            for p in ps:p.advance();p.tick()
    collision.prepare=collision_setup
    add('repair_ids_same_low12_bits_do_not_requeue',collision,2)
    def negative_controls(ps):
        p=ps[0];before=p.protected();hp=p.m.r(p.owned[0]+0x6c)
        p.m.w(p.owned[0]+0x6c,hp-1);assert p.protected()!=before
        p.m.w(p.owned[0]+0x6c,hp)
        p.issue(1);events=p.drain();assert len(events)==1
        a=copy.deepcopy(p.model);b=copy.deepcopy(p.model)
        apply_model(a,events[0]);assert digest(a)!=digest(b) # dropped event
        apply_model(b,events[0]);apply_model(b,events[0]);assert digest(a)!=digest(b) # toggle duplicate
        bad=copy.deepcopy(events[0]);raw=bytearray.fromhex(bad['packet']);raw[2]=1;bad['packet']=raw.hex()
        try:apply_model(copy.deepcopy(p.model),bad)
        except AssertionError:pass
        else:raise AssertionError('forged sender was not caught')
        # Different final missions make incorrect replay ordering visible.
        def mission(n):
            raw=bytearray(111);raw[0]=4;raw[2]=0;struct.pack_into('<I',raw,7,10);raw[11]=0x34;raw[12]=n
            return {'source':0,'seq':n,'packet':raw.hex()}
        a=copy.deepcopy(p.model);b=copy.deepcopy(p.model)
        for e in (mission(2),mission(6)):apply_model(a,e)
        for e in (mission(6),mission(2)):apply_model(b,e)
        assert digest(a)!=digest(b)
        return {'negative_controls_detected':['local HP mutation','dropped event','duplicate repair toggle','wrong sender','different replay order']}
    add('negative_controls_verify_oracles',negative_controls)
    rows=[]
    for name,action,mode,backend,expected in cases:
        if args.filter and not any(part in name for part in args.filter.split(',')):continue
        try:row=run_case(args.game,args.dll,name,action,mode,backend,expected)
        except Exception as e:row={'name':name,'status':'FAIL','error':str(e),'traceback':traceback.format_exc()}
        rows.append(row);print(row['status'],name,row.get('error',''),flush=True)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps({'schema':1,'scope':'real senders + modeled receiver/transport; no live netplay',
          'inputs':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [args.game,*[args.dll.with_name(n) for n in ('GameRuntime.dll',)]]},
          'results':rows},indent=2,ensure_ascii=False))
    return int(any(r['status']=='FAIL' for r in rows))

if __name__=='__main__':raise SystemExit(main())
