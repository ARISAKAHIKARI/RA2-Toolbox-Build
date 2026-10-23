"""Execute original YR event constructors and the compiled bridge in x86 emulation.
Requires: pip install pefile unicorn
Usage: python entry_runtime_regression.py path/to/gamemd-spawn.exe path/to/GameRuntime.dll
No game executable is distributed with this project. Not a multiplayer test.
"""
import math
import ctypes as C
import struct
import sys
from pathlib import Path
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_READ
from unicorn.x86_const import *

class Label(C.Structure):
    _fields_=[('id',C.c_uint32)]+[(x,C.c_int32) for x in 'x y used capacity ironFrames shield advanced'.split()]+[('ownerColor',C.c_uint32),('frozenFrames',C.c_int32)]
class Super(C.Structure):
    _fields_=[('id',C.c_char*24),('name',C.c_uint16*40)]+[(x,C.c_int32) for x in 'index ready frames'.split()]
class Type(C.Structure):
    _fields_=[('id',C.c_char*24),('name',C.c_uint16*40)]+[(x,C.c_int32) for x in 'kind index naval cost'.split()]
class Threat(C.Structure):
    _fields_=[(x,C.c_int32) for x in 'id target x y tx ty'.split()]
class Crate(C.Structure):
    _fields_=[(x,C.c_int32) for x in 'cellX cellY x y preview'.split()]
class CrateResult(C.Structure):
    _fields_=[(x,C.c_uint32) for x in 'serial tick'.split()]+[(x,C.c_int32) for x in 'cellX cellY reward amount affected factor detail'.split()]+[('unitID',C.c_char*24),('unitName',C.c_uint16*40)]
class Weapon(C.Structure):
    _fields_=[('range',C.c_int32),('minimum',C.c_int32),('flags',C.c_uint32),('armorMask',C.c_uint32),('selectMask',C.c_uint32),('slot',C.c_int32),('effectiveness',C.c_float*11)]
class Actor(C.Structure):
    _fields_=[(x,C.c_uint32) for x in 'id type flags manualVersion target destination destinationCell'.split()]+[(x,C.c_int32) for x in 'kind x y z mission queued cost storageLimit range'.split()]+[('veterancy',C.c_float),('storage',C.c_float*4),('dockTypes',C.c_uint32*8),('radioIDs',C.c_uint32*4)]+[(x,C.c_int32) for x in 'armor rearmStart rearmDuration airRangeBonus'.split()]+[('liftTarget',C.c_uint32),('weapons',Weapon*2),('combatFirst',C.c_int32),('combatCount',C.c_int32)]
class Intent(C.Structure):
    _fields_=[(x,C.c_uint32) for x in 'id cell token mode global'.split()]
class WaitingCell(C.Structure):
    _fields_=[('cell',C.c_uint32),('occupation',C.c_uint32),('land',C.c_int32)]
class Refinery(C.Structure):
    _fields_=[('id',C.c_uint32)]+[(x,C.c_int32) for x in 'cellX cellY dockStyleA dockStyleB count'.split()]+[('waiting',WaitingCell*40)]
class CombatProbe(C.Structure):
    _fields_=[('target',C.c_uint32),('flags',C.c_uint32),('slot',C.c_int32),('effectiveness',C.c_float)]
class World(C.Structure):
    _fields_=[('match',C.c_uint32),('tick',C.c_uint32)]+[(x,C.c_int32) for x in 'frame count intentCount refineryCount'.split()]+[('inputSerial',C.c_uint32)]+[('actors',Actor*8192),('intents',Intent*2048),('refineries',Refinery*128),('combatCount',C.c_int32),('combat',CombatProbe*8192)]
class Order(C.Structure):
    _fields_=[(x,C.c_uint32) for x in 'unit token target destination'.split()]+[(x,C.c_int32) for x in 'mission destinationTag purpose'.split()]+[('refinery',C.c_uint32),('pursuit',C.c_uint32)]

class TaskReceiptSummary(C.Structure):
    _fields_=[(x,C.c_uint32) for x in 'serial match unit target queuedAt observedAt'.split()]+[(x,C.c_uint8) for x in 'phase failure owner flags'.split()]
class Snapshot(C.Structure):
    _fields_=[(x,C.c_int32) for x in 'status frame session width height labels swCount typeCount selected transports used capacity factories busyFactories pausedFactories completedFactories credits techOpen match noticeSerial notice scanMicros'.split()]+[('label',Label*2048),('supers',Super*32),('types',Type*512),('threatCount',C.c_int32),('crateCount',C.c_int32),('threats',Threat*128),('crates',Crate*256)]+[(x,C.c_int32) for x in 'techVisible techUnavailable techError autoRepair repairTotal loadUnits loadVehicles loadIssued loadWaiting loadUnassigned sampledTick cameraX cameraY crateHookStatus crateResultCount crateResultSerial'.split()]+[('crateResults',CrateResult*16)]+[(x,C.c_int32) for x in 'chat csHookStatus csConverted nativeHookStatus advancedUnits unloadPending unloadIssued ejectWatched inputPatchMask oreQueued oreDocking oreIssued placementStatus nativeAirStatus controlMicros worldMicros displayMicros oreManual oreYielded'.split()]+[(x,C.c_uint32) for x in 'oreDelivered oreUnverified magnetShots magnetUnverified hookInstalled hookConflicts queueSent queueFull queueTimingFailure planDeferred'.split()]+[('megaQueueStatus',C.c_int32),('megaQueueAccepted',C.c_uint32),('megaQueueRejected',C.c_uint32),('objectQueueStatus',C.c_int32),('objectQueueAccepted',C.c_uint32),('objectQueueRejected',C.c_uint32)]+[(x,C.c_int32) for x in 'readStatus bridgeStatus commandStatus renderStatus'.split()]+[(x,C.c_uint32) for x in 'queuedEvents observedEvents commandRejected receiptCancelled receiptExpired receiptDropped receiptWrite receiptCount'.split()]+[('recentReceipts',TaskReceiptSummary*32),('entryThread',C.c_uint32),('entryCalls',C.c_uint32),('entryReentrant',C.c_uint32),('entryContended',C.c_uint32),('entryWrongThread',C.c_uint32),('c4TestStatus',C.c_int32),('renderRevealStatus',C.c_int32),('lastSubmitError',C.c_int32)]
class CooperativeSource(C.Structure):
    _fields_=[('id',C.c_uint32),('token',C.c_uint32)]
class CooperativeRequest(C.Structure):
    _fields_=[(x,C.c_uint32) for x in 'serial match tick target type'.split()]+[(x,C.c_int32) for x in 'frame mode count cameraX cameraY viewWidth viewHeight'.split()]+[('sources',CooperativeSource*512)]
class CooperativeAssignment(C.Structure):
    _fields_=[(x,C.c_uint32) for x in 'id token target type'.split()]
class PointerSource(C.Structure):
 _fields_=[('id',C.c_uint32),('pointer',C.c_uint32)]
class PointerIntent(C.Structure):
 _fields_=[(x,C.c_uint32) for x in 'tick match house bindings inputSerial'.split()]+[(x,C.c_int32) for x in 'frame operation x y width height cameraX cameraY count'.split()]+[('sources',PointerSource*512)]
class PointerMailbox(C.Structure):
 _fields_=[(x,C.c_uint32) for x in 'request ack cancelSerial'.split()]+[('intent',PointerIntent)]+[(x,C.c_int32) for x in 'result accepted rejected'.split()]+[('resolvedCell',C.c_uint32),('resolvedTarget',C.c_uint32),('resolvedZ',C.c_int32),('sourceTokens',C.c_uint32*512),('sourceAccepted',C.c_uint32*16)]
class NativePolicy(C.Structure):
 _fields_=[('values',C.c_uint32*22)] # 3 + Bindings(9) + MouseCount(10)
class SuppressedClick(C.Structure):
 _fields_=[('values',C.c_uint32*5)]
class OreConfig(C.Structure):
 _fields_=[(x,C.c_uint32) for x in ("customMiners","customRefineries")]+[(x,C.c_int32) for x in ("minerCount","refineryCount")]+[(x,(C.c_char*24)*512) for x in ("miners","refineries")]
class FeatureFilter(C.Structure):
 _fields_=[("custom",C.c_uint32),("count",C.c_int32),("ids",(C.c_char*24)*512)]
class FeatureConfig(C.Structure):
 _fields_=[("filters",FeatureFilter*20)]
class ReservationPolicy(C.Structure):
 _fields_=[(x,C.c_uint32) for x in 'match house parent parentID tick tab slots count'.split()]+[('queued',C.c_int32*5)]
class ReservationClick(C.Structure):
 _fields_=[(x,C.c_uint32) for x in 'tick match house parent parentID tab index operation all'.split()]
class LoadSoldier(C.Structure):
 _fields_=[(x,C.c_uint32) for x in 'object id'.split()]+[(x,C.c_int32) for x in 'x y slots'.split()]+[('type',C.c_uint32),('kind',C.c_int32),('ticket',C.c_uint32),('queuedAt',C.c_uint32)]
class LoadVehicle(C.Structure):
 _fields_=[('object',C.c_uint32),('id',C.c_uint32)]+[(x,C.c_int32) for x in 'x y capacity used reserved'.split()]+[('sizeLimit',C.c_double),('passengers',C.c_int32)]
class LoadComposition(C.Structure):
 _fields_=[('types',C.c_uint32*512),('count',C.c_int32),('numbers',(C.c_int16*512)*128)]
class LoadInput(C.Structure):
 _fields_=[('match',C.c_uint32),('tick',C.c_uint32)]+[(x,C.c_int32) for x in 'frame ns nv mode radius'.split()]+[('soldiers',LoadSoldier*512),('vehicles',LoadVehicle*128),('tokens',C.c_uint32*512),('composition',LoadComposition),('compatible',(C.c_uint8*128)*512)]
class LoadResult(C.Structure):
 _fields_=[('match',C.c_uint32),('tick',C.c_uint32),('frame',C.c_int32),('count',C.c_int32),('assignment',C.c_int32*512)]
class RepairView(C.Structure):
 _fields_=[('hp',C.c_int32),('maxHp',C.c_int32)]+[(x,C.c_bool) for x in 'active alreadyRepairing issuedRecently'.split()]
class RepairCandidate(C.Structure):
 _fields_=[('id',C.c_uint32),('view',RepairView)]
class RepairInput(C.Structure):
 _fields_=[('match',C.c_uint32),('tick',C.c_uint32),('frame',C.c_int32),('count',C.c_int32),('items',RepairCandidate*2048)]
class RepairResult(C.Structure):
 _fields_=[('match',C.c_uint32),('tick',C.c_uint32),('frame',C.c_int32),('count',C.c_int32),('ids',C.c_uint32*2048)]
class Quota(C.Structure):
 _fields_=[(x,C.c_int32) for x in 'first second maximum'.split()]
class QuotaEntry(C.Structure):
 _fields_=[('id',C.c_char*24),('quota',Quota)]
class QuotaConfig(C.Structure):
 _fields_=[('defaults',Quota),('count',C.c_int32),('entries',QuotaEntry*512)]
class UnloadState(C.Structure):
 _fields_=[(x,C.c_int32) for x in 'stage sentFrame seenUnload retries x y stableSince deployedSince'.split()]+[('sleepObserved',C.c_uint32)]
class UnloadView(C.Structure):
 _fields_=[(x,C.c_int32) for x in 'frame mode landedFrame mission queued sequence x y'.split()]+[(x,C.c_uint32) for x in 'deployer ready engineer spy c4 queryFailed'.split()]
class UnloadSource(C.Structure):
 _fields_=[(x,C.c_uint32) for x in 'job id pointer type token'.split()]+[('state',UnloadState),('view',UnloadView)]
class UnloadTarget(C.Structure):
 _fields_=[('id',C.c_uint32),('pointer',C.c_uint32),('x',C.c_int32),('y',C.c_int32)]
class UnloadDecision(C.Structure):
 _fields_=[('observed',UnloadState),('accepted',UnloadState),('mission',C.c_int32)]+[(x,C.c_uint32) for x in 'target done doneAfter follow'.split()]
class UnloadInput(C.Structure):
 _fields_=[('match',C.c_uint32),('tick',C.c_uint32)]+[(x,C.c_int32) for x in 'frame count targetCount'.split()]+[('sources',UnloadSource*64),('targets',UnloadTarget*8192),('actions',(C.c_uint8*8192)*64)]
class UnloadResult(C.Structure):
 _fields_=[('match',C.c_uint32),('tick',C.c_uint32),('frame',C.c_int32),('count',C.c_int32),('decisions',UnloadDecision*64)]
class BurstSource(C.Structure):
 _fields_=[(x,C.c_uint32) for x in 'job id pointer token target targetPointer weapon'.split()]+[(x,C.c_int32) for x in 'phase frame sentFrame mission queued burst index fireStamp previousFire'.split()]+[(x,C.c_uint32) for x in 'currentTarget eligible inRange moving'.split()]
class BurstDecision(C.Structure):
 _fields_=[('mission',C.c_int32),('done',C.c_uint32)]
class BurstInput(C.Structure):
 _fields_=[('match',C.c_uint32),('tick',C.c_uint32),('frame',C.c_int32),('count',C.c_int32),('sources',BurstSource*64)]
class BurstResult(C.Structure):
 _fields_=[('match',C.c_uint32),('tick',C.c_uint32),('frame',C.c_int32),('count',C.c_int32),('decisions',BurstDecision*64)]
class DodgeSource(C.Structure):
 _fields_=[(x,C.c_uint32) for x in 'id pointer token job active busy observed freshThreat'.split()]+[(x,C.c_int32) for x in 'frame sentFrame quietFrame count'.split()]+[('shots',C.c_uint32*32)]
class DodgeDecision(C.Structure):
 _fields_=[('scatter',C.c_uint32),('release',C.c_uint32)]
class DodgeInput(C.Structure):
 _fields_=[('match',C.c_uint32),('tick',C.c_uint32),('frame',C.c_int32),('count',C.c_int32),('sources',DodgeSource*32)]
class DodgeResult(C.Structure):
 _fields_=[('match',C.c_uint32),('tick',C.c_uint32),('frame',C.c_int32),('count',C.c_int32),('decisions',DodgeDecision*32)]
class PlayerTestInfo(C.Structure):
 _fields_=[(x,C.c_uint32) for x in ("pointer","id")]+[(x,C.c_int32) for x in ("index","human")]+[("name",C.c_uint16*21)]
class MissileSource(C.Structure):
 _fields_=[(x,C.c_uint32) for x in 'id pointer token current pending'.split()]+[(x,C.c_int32) for x in 'cooldown held pendingMission holdFrames releaseFrames'.split()]
class MissileTarget(C.Structure):
 _fields_=[('id',C.c_uint32),('pointer',C.c_uint32),('hp',C.c_int32),('threatening',C.c_uint32)]
class MissileEdge(C.Structure):
 _fields_=[('damage',C.c_int32),('distance',C.c_uint32),('eta',C.c_int32),('close',C.c_int32)]
class MissileInput(C.Structure):
 _fields_=[('match',C.c_uint32),('tick',C.c_uint32)]+[(x,C.c_int32) for x in 'frame count targetCount mode'.split()]+[('sources',MissileSource*64),('targets',MissileTarget*64),('edges',(MissileEdge*64)*64)]
class MissileAssignment(C.Structure):
 _fields_=[('source',C.c_int32),('target',C.c_int32),('action',C.c_int32)]
class MissileResult(C.Structure):
 _fields_=[('match',C.c_uint32),('tick',C.c_uint32),('frame',C.c_int32),('count',C.c_int32),('orders',MissileAssignment*64)]
class FormationPolicy(C.Structure):
    _fields_=[('enabled',C.c_uint32),('hold',C.c_uint32),('keys',C.c_uint32*4),('mouse',C.c_uint32)]
class FormationUnit(C.Structure):
    _fields_=[(x,C.c_uint32) for x in 'id pointer type token color'.split()]+[(x,C.c_int32) for x in 'x y z kind'.split()]
class FormationSlot(C.Structure):
    _fields_=[(x,C.c_int32) for x in 'x y z'.split()]+[('valid',C.c_uint32)]
class FormationPreview(C.Structure):
    _fields_=[(x,C.c_uint32) for x in 'match tick active dragging revision'.split()]+[(x,C.c_int32) for x in 'shape count ax ay'.split()]+[('facing',C.c_double),('units',FormationUnit*256),('slots',FormationSlot*256)]
class Shared(C.Structure):
    _fields_=[(x,C.c_uint32) for x in 'magic version pid owner heartbeat stop foreground flags scanMs request ack command arg arg2 requestToken requestTick requestMatch result count seq'.split()]+[('data',Snapshot),('techAllowed',C.c_uint8*512),('repairMs',C.c_uint32),('clockStart',C.c_uint64),('clockFrequency',C.c_uint64),('clockTick',C.c_uint32),('csSeq',C.c_int32),('csMatch',C.c_uint32),('csAllowed',C.c_uint8*512)]+[(x,C.c_uint32) for x in 'advancedKey pathTimeout veteranPercent elitePercent selectRequest selectAck selectIndex selectGlobal selectObjectID selectTick selectMatch selectResult selectCount csHoldKey'.split()]+[('worldSeq',C.c_int32),('world',World)]+[(x,C.c_uint32) for x in 'planRequest planAck planTick planMatch planCount planAccepted planResult'.split()]+[('plan',Order*24),('plannerTracking',C.c_uint32),('hostWindow',C.c_uint32),('hostNotifyPending',C.c_int32),('gameWakePending',C.c_int32),('magnetronKey',C.c_uint32),('planAcceptedMask',C.c_uint32),('planRejectedMask',C.c_uint32)]+[(x,C.c_uint32) for x in 'externalDisplay loadMode loadRadius enterKey airKey1 airKey2'.split()]+[('cooperativeSeq',C.c_int32),('cooperativeRequest',CooperativeRequest)]+[(x,C.c_uint32) for x in 'cooperativePlan cooperativeAck cooperativeSerial cooperativeTick'.split()]+[(x,C.c_int32) for x in 'cooperativeCount cooperativeAccepted cooperativeRejected cooperativeMode'.split()]+[('cooperativeOrders',CooperativeAssignment*512),('clickHookStatus',C.c_int32)]+[(x,C.c_uint32) for x in 'driverMode driverTarget megaQueueMode objectQueueMode'.split()]+[('driverStatus',C.c_int32),('driverTicks',C.c_uint32),('driverLastTick',C.c_uint32)]+[(x,C.c_uint32) for x in 'productionRequest productionAck productionMatch productionTick'.split()]+[('productionCount',C.c_int32),('productionTypes',C.c_int32*4),('planFrame',C.c_int32),('cooperativeFrame',C.c_int32),('cooperativeMatch',C.c_uint32),('cooperativeProcessed',C.c_int32),('cooperativeResult',C.c_int32),('cooperativeAcceptedBits',C.c_uint32*16),('pointer',PointerMailbox),('pointerPolicySeq',C.c_int32),('pointerPolicy',NativePolicy),('suppressedClick',SuppressedClick),('selectTypes',C.c_uint8*512),('oreConfigSeq',C.c_int32),('oreConfig',OreConfig),('featureConfigSeq',C.c_int32),('featureConfig',FeatureConfig),('c4Test',C.c_uint32),('advancedRangeCells',C.c_uint32),('renderReveal',C.c_uint32)]+[(x,C.c_uint32) for x in 'uiSoundRequest uiSoundAck uiSoundTick uiSoundMatch'.split()]+[('reservePolicySeq',C.c_int32),('reservePolicy',ReservationPolicy),('reserveInput',C.c_uint32),('reserveConsumed',C.c_uint32),('reserveClick',ReservationClick)]+[(x,C.c_uint32) for x in 'computeRequested computeApplied advancedScanHz advancedPriority runtimeTracking runtimePlannerMicros'.split()]+[('loadTask',C.c_uint32),('loadComputed',C.c_uint32),('loadInput',LoadInput),('loadResult',LoadResult),('repairTask',C.c_uint32),('repairComputed',C.c_uint32),('repairInput',RepairInput),('repairResult',RepairResult),('quotaConfigSeq',C.c_int32),('quotaConfig',QuotaConfig),('runtimeCoopDone',C.c_uint32),('runtimeCoopAccepted',C.c_int32),('runtimeCoopRejected',C.c_int32)]+[('unloadInputSeq',C.c_int32),('unloadResultSeq',C.c_int32),('unloadTask',C.c_uint32),('unloadComputed',C.c_uint32),('unloadInput',UnloadInput),('unloadResult',UnloadResult)]+[(x,C.c_uint32) for x in ('controlAll','switchPlayerEnabled')]+[(x,C.c_int32) for x in ('playerCount','playerCurrent','playerOriginal','playerControlStatus')]+[(x,C.c_uint32) for x in 'missileEnabled missileScanHz missileInputSeq missileResultSeq missileTask missileComputed'.split()]+[('missileInput',MissileInput),('missileResult',MissileResult)]+[(x,C.c_uint32) for x in 'missileSources missileTargets missileIssued'.split()]+[(x,C.c_uint32) for x in 'burstEnabled burstHoldEnabled burstHeld burstInputSeq burstResultSeq burstTask burstComputed'.split()]+[('burstInput',BurstInput),('burstResult',BurstResult)]+[(x,C.c_uint32) for x in 'burstActive burstIssued dodgeEnabled dodgeInputSeq dodgeResultSeq dodgeTask dodgeComputed'.split()]+[('dodgeInput',DodgeInput),('dodgeResult',DodgeResult)]+[(x,C.c_uint32) for x in 'dodgeActive dodgeIssued'.split()]+[('players',PlayerTestInfo*32),('scrollRefreshMode',C.c_uint32),('scrollRefreshHz',C.c_uint32),('scrollRefreshStatus',C.c_int32),('scrollRefreshDraws',C.c_uint32),('scrollRefreshRateMilli',C.c_uint32),('scrollRefreshMaxGapUs',C.c_uint32),('viewZoomPercent',C.c_uint32),('viewZoomBusy',C.c_int32),('viewZoomStatus',C.c_int32),('viewZoomApplied',C.c_int32),('viewZoomWidth',C.c_int32),('viewZoomHeight',C.c_int32),('magnetronDrop',C.c_uint32)]+[(x,C.c_uint32) for x in 'missileScope missileAllocation magnetronGlobal missileHeld'.split()]+[('formationPolicySeq',C.c_int32),('formationSeq',C.c_int32),('formationPolicy',FormationPolicy),('formationPreview',FormationPreview)]+[(x,C.c_uint32) for x in 'formationKey formationKeyAck formationKeyCode formationConsumedUntil formationAccepted formationRejected formationJobs'.split()]+[('cameraSeq',C.c_int32)]+[(x,C.c_uint32) for x in 'cameraAck cameraTick cameraMatch'.split()]+[('cameraTargetX',C.c_int32),('cameraTargetY',C.c_int32),('dodgeMode',C.c_uint32)]
    def __init__(self,*args,**kw):
        super().__init__(*args,**kw);self.computeRequested=1984;self.computeApplied=1984;self.missileScanHz=10;self.advancedScanHz=10

u=Uc(UC_ARCH_X86,UC_MODE_32)
fixture_paths=globals().get("fixture_paths",sys.argv[1:3])
game=pefile.PE(fixture_paths[0]); dll=pefile.PE(fixture_paths[1])
# Stop stale test layouts before emulating any Runtime function.
protocol_export=next(e for e in dll.DIRECTORY_ENTRY_EXPORT.symbols if e.name==b'AssistProtocolInfo')
assert C.sizeof(Shared)==struct.unpack('<II',dll.get_data(protocol_export.address,8))[1],('fixture ABI',C.sizeof(Shared))
broker=dll
from types import SimpleNamespace
cursor_dll=SimpleNamespace(OPTIONAL_HEADER=SimpleNamespace(ImageBase=0x19000000))
monitor_dll=SimpleNamespace(OPTIONAL_HEADER=SimpleNamespace(ImageBase=0x1b000000))
images=[game,dll]
for pe in images:
    u.mem_map(pe.OPTIONAL_HEADER.ImageBase,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
    u.mem_write(pe.OPTIONAL_HEADER.ImageBase,pe.get_memory_mapped_image())
SH=0x20000000;HEAP=0x21000000;STACK=0x30000000;STOP=0x31000000;STUB=0x32000000
for a,n in [(0,0x10000),(SH,0x600000),(HEAP,0x400000),(STACK-0x100000,0x120000),(STOP,0x10000),(STUB,0x10000)]:u.mem_map(a,n)
def w(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
def r(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def b(a,v):u.mem_write(a,bytes([int(v)]))
w(0,0xffffffff);u.reg_write(UC_X86_REG_CR4,0x600)
cursor_mapping_enabled=False;watchdog_callback=0;watchdog_threads=[];legacy_loaded=False;process_dead=False
fail_owner_open=False;fail_watchdog_pin=False;fail_watchdog_timer=False;mapping_names=[];
unique_process_handles=False;opened_process_handles=[];dead_process_handles=set()
native_query_exception=None
win32_last_error=0;clobber_win32_error=False;exception_observer=0;observer_registration_fails=False
canbuild_limit_result=1
canbuild_full_result=0
execute_native_find_factory=False
clock=1000;counter=1;va_count=0;held_keys=set();action_overrides={};calls=[];imports={};factory_available=True;canbuild_calls=[];used_sizes={};blocked_pairs=set();enter_queries=[];projection_calls=0;size_calls=0;projection_visible_bounds=None;posted_messages=[];projection_receipts=[];sidebar_add_calls=[];watch_reads=set();watched_read_count=0
for pe in (dll,):
 for entry in pe.DIRECTORY_ENTRY_IMPORT:
    for imp in entry.imports:
        a=STUB+len(imports)*16;imports[a]=imp.name.decode();w(imp.address,a)
def string_at(at,wide=False):
 out=[]
 while True:
  c=int.from_bytes(u.mem_read(at,2 if wide else 1),'little')
  if not c:return ''.join(out)
  out.append(chr(c));at+=2 if wide else 1

def module_at(address):
 return next((p for p in images if p.OPTIONAL_HEADER.ImageBase<=address<p.OPTIONAL_HEADER.ImageBase+p.OPTIONAL_HEADER.SizeOfImage),None)
current_thread=777
dead_threads=set()
thread_open_counts={}
def ret(value,argc):
    sp=u.reg_read(UC_X86_REG_ESP);ip=r(sp)
    u.reg_write(UC_X86_REG_EAX,value&0xffffffff);u.reg_write(UC_X86_REG_ESP,sp+4+argc*4);u.reg_write(UC_X86_REG_EIP,ip)
def on_code(uc,a,size,_):
    global counter,projection_calls,size_calls,va_count,watched_read_count,watchdog_callback,win32_last_error,exception_observer
    sp=u.reg_read(UC_X86_REG_ESP)
    arg=lambda i:r(sp+4+i*4)
    if a in imports:
        n=imports[a]
        if clobber_win32_error and n not in ('GetLastError','SetLastError','CallNextHookEx'):win32_last_error=0xdead
        if n=='GetCurrentProcessId':ret(123,0)
        elif n=='GetCurrentThreadId':ret(current_thread,0)
        elif n=='VirtualQuery':
            image=module_at(arg(0));allocation=image.OPTIONAL_HEADER.ImageBase if image else arg(0)&~4095
            u.mem_write(arg(1),struct.pack('<7I',arg(0)&~4095,allocation,0x20,4096,0x1000,0x20,0x1000000));ret(28,3)
        elif n=='OpenProcess':
            h=0x60000000+len(opened_process_handles)+1 if unique_process_handles else 0x1234
            opened_process_handles.append(h);ret(0 if fail_owner_open else h,3)
        elif n=='OpenThread':
            tid=arg(2);thread_open_counts[tid]=thread_open_counts.get(tid,0)+1;ret(0x50000000+tid,3)
        elif n=='WaitForSingleObject':ret(0 if arg(0)-0x50000000 in dead_threads or arg(0) in dead_process_handles or (arg(0)==0x1234 and process_dead) else 258,2)
        elif n=='GetLastError':ret(5 if fail_owner_open else 8 if fail_watchdog_timer or fail_watchdog_pin else win32_last_error,0)
        elif n=='SetLastError':win32_last_error=arg(0);ret(0,1)
        elif n=='AddVectoredExceptionHandler':
            assert arg(0)==0;exception_observer=arg(1);ret(0 if observer_registration_fails else 0x12345678,2)
        elif n=='GetCurrentProcess':ret(0xffffffff,0)
        elif n=='GetProcessHeap':ret(1,0)
        elif n=='HeapAlloc':ret(alloc(arg(2)),3)
        elif n=='HeapFree':ret(1,3)
        elif n=='RaiseException':raise AssertionError('Runtime allocation fault '+hex(arg(0)))
        elif n in ('GetTickCount','timeGetTime'):ret(clock,0)
        elif n=='OpenFileMappingW':
            name=string_at(arg(2),True);mapping_names.append(name)
            ret(2 if name in ('Local\\RA2Runtime_v3_123_456_','Local\\RA2Runtime_v4_123_456_') else 1 if name==f'Local\\RA2Assist_v{struct.unpack("<I",dll.get_data(protocol_export.address,4))[0]:03}_123' else 3 if cursor_mapping_enabled and name=='Local\\RA2CursorBridge_v154_123' else 0,3)
        elif n=='MapViewOfFile':ret(SH+0x500000 if arg(0)==2 else 0x22000000 if arg(0)==3 else SH,5)
        elif n=='SetTimer':watchdog_callback=arg(3);watchdog_threads.append(current_thread);ret(0 if fail_watchdog_timer else 1,4)
        elif n=='wsprintfW':
            value='Local\\RA2CursorBridge_v154_123';u.mem_write(arg(0),(value+'\0').encode('utf-16le'));ret(len(value),0)
        elif n=='ReadProcessMemory':
            u.mem_write(arg(2),bytes(u.mem_read(arg(1),arg(3))));w(arg(4),arg(3));ret(1,5)
        elif n in ('QueryPerformanceCounter','QueryPerformanceFrequency'):
            u.mem_write(arg(0),struct.pack('<Q',globals().get('precise_us',clock*1000) if n.endswith('Counter') else 1000000));counter+=10;ret(1,1)
        elif n=='VirtualProtect':w(arg(3),0x20);ret(1,4)
        elif n=='FlushInstructionCache':ret(1,3)
        elif n=='GetModuleHandleExW':
            pe=module_at(arg(1));ok=pe is not None and not (fail_watchdog_pin and arg(0)&1);w(arg(2),pe.OPTIONAL_HEADER.ImageBase if ok else 0);ret(int(ok),3)
        elif n=='GetModuleHandleW' and not arg(0):ret(game.OPTIONAL_HEADER.ImageBase,1)
        elif n=='GetModuleHandleW':ret(dll.OPTIONAL_HEADER.ImageBase if string_at(arg(0),True)=='GameRuntime.dll' else 1 if legacy_loaded else 0,1)
        elif n=='GetModuleFileNameW':
            value='C:\\Toolbox\\GameRuntime.dll';data=(value+'\0').encode('utf-16le');u.mem_write(arg(1),data);ret(len(value),3)
        elif n=='LoadLibraryW':
            raise AssertionError('Game runtime must not load another DLL')
        elif n=='GetProcAddress':
            pe=module_at(arg(0));name=string_at(arg(1)).encode();exp=next((x for x in pe.DIRECTORY_ENTRY_EXPORT.symbols if x.name==name),None);ret(pe.OPTIONAL_HEADER.ImageBase+exp.address if exp else 0,2)
        elif n=='FreeLibrary':ret(1,1)
        elif n=='abs':ret(abs(C.c_int32(arg(0)).value),0)
        elif n=='lround':
            value=struct.unpack('<d',u.mem_read(sp+4,8))[0];ret(math.floor(value+.5) if value>=0 else math.ceil(value-.5),0)
        elif n in ('hypot','atan2','sin','cos','sqrt','ceil','floor'):
            values=struct.unpack('<2d' if n in ('hypot','atan2') else '<d',u.mem_read(sp+4,16 if n in ('hypot','atan2') else 8));value=getattr(math,n)(*values)
            u.mem_write(STUB+0xfe80,struct.pack('<d',value));u.mem_write(STUB+0xfe00,b'\xdd\x05'+struct.pack('<I',STUB+0xfe80)+b'\xc3');u.reg_write(UC_X86_REG_EIP,STUB+0xfe00)
        elif n=='memchr':
            start,value,count=arg(0),arg(1)&255,arg(2);index=bytes(u.mem_read(start,count)).find(bytes([value]));ret(start+index if index>=0 else 0,0)
        elif n=='memcmp':
            a1,a2,count=arg(0),arg(1),arg(2);v1,v2=bytes(u.mem_read(a1,count)),bytes(u.mem_read(a2,count));ret((v1>v2)-(v1<v2),0)
        elif n in ('memcpy','memmove'):u.mem_write(arg(0),bytes(u.mem_read(arg(1),arg(2))));ret(arg(0),0)
        elif n=='memset':u.mem_write(arg(0),bytes([arg(1)&255])*arg(2));ret(arg(0),0)
        elif n=='VirtualAlloc':
            ret(HEAP+0x3e0000+va_count*4096,4);va_count+=1
        elif n=='GetForegroundWindow':ret(1,0)
        elif n=='GetWindowThreadProcessId':w(arg(1),123);ret(current_thread,2)
        elif n=='GetCursorPos':u.mem_write(arg(0),struct.pack('<ii',*globals().get('mousePoint',(100,100))));ret(1,1)
        elif n=='ScreenToClient':ret(1,2)
        elif n=='GetClientRect':u.mem_write(arg(1),struct.pack('<4i',0,0,globals().get('client_width',800),globals().get('client_height',600)));ret(1,2)
        elif n in ('GetAsyncKeyState','GetKeyState'):ret(0x8000 if arg(0) in held_keys else 0,1)
        elif n=='CallNextHookEx':ret(0,4)
        elif n=='PostMessageW':posted_messages.append(tuple(arg(i) for i in range(4)));ret(1,4)
        elif n in ('UnmapViewOfFile','CloseHandle','DisableThreadLibraryCalls'):ret(1,1)
        else:raise AssertionError('unexpected import '+n)
    elif a==0x6d2140:
        projection_calls+=1;projection_receipts.append((r(SH+Shared.ack.offset),r(SH+Shared.request.offset)));p=arg(0);x,y,z=struct.unpack('<iii',u.mem_read(p,12));tactical=u.reg_read(UC_X86_REG_ECX);w(arg(1),x//256-r(tactical+0xb0));w(arg(1)+4,y//256-r(tactical+0xb4));ret(int(projection_visible_bounds is None or (0<=x//256-r(tactical+0xb0)<projection_visible_bounds[0] and 0<=y//256-r(tactical+0xb4)<projection_visible_bounds[1])),2)
    elif a==0x4f7870:
        canbuild_calls.append((u.reg_read(UC_X86_REG_ECX),arg(0),arg(1),arg(2)));ret(canbuild_limit_result if arg(1) else canbuild_full_result,3)
    elif a==0x5f7900:
        if execute_native_find_factory:return
        ret(building if factory_available else 0,4)
    elif a==0x6abc60:
        k=u.reg_read(UC_X86_REG_ECX);ret(0 if k==7 else 2 if k==16 else 3,1)
    elif a==0x6a6300:
        sidebar_add_calls.append((arg(0),arg(1)))
        k,index=arg(0),arg(1);tab=0 if k==7 else 2 if k==16 else 3
        strip=0x87f7e8+0x1544+tab*3988;count=r(strip+84)
        if not any(r(strip+88+j*52)==index and r(strip+92+j*52)==k for j in range(count)):
            w(strip+88+count*52,index);w(strip+92+count*52,k);w(strip+84,count+1)
        ret(1,2)
    elif a==0x578080:ret(globals().get('floor_height',0),1)
    elif a==0x473460:size_calls+=1;ret(used_sizes.get(u.reg_read(UC_X86_REG_ECX)-0x114,3),0)
    elif a in (0x51e3b0,0x73fd50):
        if native_query_exception and native_query_exception(a):return
        source=u.reg_read(UC_X86_REG_ECX);target=arg(0);assert arg(1)==1
        enter_queries.append((source,target));ret(action_overrides.get((source,target),31 if (source,target) in blocked_pairs or a==0x73fd50 else 3),2)
    elif a==0x6cc360:ret(u.mem_read(u.reg_read(UC_X86_REG_ECX)+0x6f,1)[0],0)
    elif a in (0x48dc90,0x6fbfa0,0x6d2420,0x431410,0x577d90,0x4a8bf0,0x4ac8c0,0x4ac660):
        if globals().get("execute_native_selection",False) and a in (0x48dc90,0x6fbfa0):return
        calls.append((a,u.reg_read(UC_X86_REG_ECX),[arg(i) for i in range(2)]))
        ret(1,{0x48dc90:0,0x6fbfa0:0,0x6d2420:2,0x431410:1,0x577d90:1,0x4a8bf0:1,0x4ac8c0:1,0x4ac660:1}[a])
for target in list(imports)+[0x4f7870,0x5f7900,0x6abc60,0x6a6300,0x6d2140,0x578080,0x473460,0x51e3b0,0x73fd50,0x6cc360,0x48dc90,0x6fbfa0,0x6d2420,0x431410,0x577d90,0x4a8bf0,0x4ac8c0,0x4ac660]:
    u.hook_add(UC_HOOK_CODE,on_code,begin=target,end=target)
def run(address,args=(),self=0):
    # Existing command regressions now enter through the production runtime.
    # A host wake sets the ready bit; an absent request is separately tested.
    if address==globals().get('entry') and args==(0,1,0):
        w(SH+0x500000+20,r(SH+0x500000+20)|1)
        args=(0,1,SH+0x500100)

    sp=STACK+0x1e000;u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),STOP,*args));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,self)
    try:u.emu_start(address,STOP,count=2000000)
    except Exception:
        print('Emulation fetch/state:',hex(u.reg_read(UC_X86_REG_EIP)),hex(r(u.reg_read(UC_X86_REG_ESP))))
        raise
    assert u.reg_read(UC_X86_REG_EIP)==STOP, 'instruction budget exceeded'
    assert u.reg_read(UC_X86_REG_ESP)==sp+4+4*len(args),'calling convention / stack imbalance'


u.hook_add(UC_HOOK_CODE,lambda uc,a,size,_:ret(0,0),begin=0x4068e0,end=0x4068e0)
cursor=HEAP
def alloc(n=4096):
    global cursor
    p=cursor;cursor+=(n+15)&~15;return p
def vector(at,values):
    p=alloc(max(16,4*len(values)))
    for i,x in enumerate(values):w(p+i*4,x)
    u.mem_write(at,struct.pack('<6I',0,p,len(values),0x101,len(values),10))
house=alloc(0x20000);enemy=alloc(0x20000);t=alloc(0x2000);engT=alloc(0x2000);buildT=alloc(0x2000)
w(house+0x30,0);w(enemy+0x30,1);w(house+0x5788,1);w(house+0x30c,10000)
for typ,id in [(t,b'HTNK'),(engT,b'ENGINEER'),(buildT,b'NAPOWR')]:
    u.mem_write(typ+0x24,id+b'\0');w(typ+0xa0,100);w(typ+0x634,5);w(typ+0x6cc,1);w(typ+0x610,500)
for typ,vt in [(t,0x7F5FA0),(engT,0x7EB218),(buildT,0x7E3EBC)]:
    table=alloc(0x200);w(typ,table);w(table+0x94,0x5f7900)
w(t+0x5e0,6);b(t+0xdbc,1);b(engT+0xec3,1)
# Synthetic objects need the secondary COM identity vtable used by the real
# TargetClass packing constructor. Stub only Fetch_ID, not the command path.
identityTable=alloc(32);identityFetch=STUB+0xef00;w(identityTable+16,identityFetch)
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,_:ret(r(r(u.reg_read(UC_X86_REG_ESP)+4)+12),1),begin=identityFetch,end=identityFetch)
def obj(vt,typ,owner,id,hp=100,selected=False):
    b(typ+0x231,1);b(typ+0xd3a,1);b(typ+0xd38,1);w(typ+0x684,0xffffffff)
    p=alloc(0x2000);w(p,vt);w(p+4,identityTable);w(p+0x10,id);w(p+0x6c,hp);b(p+0x74,1);b(p+0x90,1);b(p+0x83,selected);w(p+0x21c,owner);w(p+(0x520 if vt==0x7e3ebc else 0x6c0 if vt==0x7eb058 else 0x6c4),typ);w(p+0xac,5);w(p+0xb4,0xffffffff);u.mem_write(p+0x9c,struct.pack('<iii',25600+id*256,25600,0));return p
b(buildT+0x157a,1) # YRpp ClickRepairable; damaged fixture is an ordinary repairable building.
unit=obj(0x7f5c70,t,house,10,60,True);eng=obj(0x7eb058,engT,house,11);building=obj(0x7e3ebc,buildT,house,12,50);enemyUnit=obj(0x7f5c70,t,enemy,13)
w(enemyUnit+0x2b4,unit);w(unit+0x18c,90);w(unit+0x194,60)
vector(0xa80228,[house,enemy]);vector(0xa8ec78,[unit,eng,building,enemyUnit]);vector(0xa8ecb8,[unit,eng]);vector(0xa8eb40,[building])
vector(0xa83ce0,[t]);vector(0xa8e348,[engT]);vector(0xa83c68,[buildT]);vector(0xa8b218,[]);vector(0xa83e30,[]);vector(house+0x254,[])
b(0xa8e378,1);w(0xa8eda0,0);w(0xa8ed8c,0);w(0xb0ce30,632);w(0xb0ce34,600);
w(0xa83d4c,house);b(0xa8e9a0,1);w(0xac1198,0);w(0xa8ed84,100);w(0xa8b238,5);w(0xa8eb84,800);w(0xa8eb88,600);w(0x8a00a4,800);w(0x8a00a8,600);w(0x887324,alloc());w(0xa802c8,0);w(0xa802d0,0)
# One live crate and a stale expired entry, no whole-map scan.
cells=alloc(512*64*4);w(0x87f7e8+316,cells);w(0x87f7e8+320,512*64)
crateCell=alloc();w(crateCell+68,5);w(cells+(30*512+20)*4,crateCell)
overlayType=alloc();b(overlayType+0x2aa,1);vector(0xa83d80,[0,0,0,0,0,overlayType]);b(crateCell+0x11e,255)
u.mem_write(0x87f7e8+344,struct.pack('<iii hh',90,0,900,20,30))
u.mem_write(0x87f7e8+360,struct.pack('<iii hh',0,0,1,20,30))
class EntryRequest(C.Structure):
 _fields_=[(n,C.c_uint32) for n in 'size version operation client module token bindThread address length'.split()]+[('before',C.c_uint8*16),('after',C.c_uint8*16)]+[(n,C.c_uint32) for n in 'gameThread activeModule installed conflicts active conflict entries reentrant contended wrongThread'.split()]
assert C.sizeof(EntryRequest)==108
control=SH+0x500000
manager_export=broker.OPTIONAL_HEADER.ImageBase+next(x.address for x in broker.DIRECTORY_ENTRY_EXPORT.symbols if x.name==b'ManageGameEntry')
def manager_call(req):
 req.size=108;req.version=int.from_bytes(broker.get_data(next(x.address for x in broker.DIRECTORY_ENTRY_EXPORT.symbols if x.name==b'GameEntryManagerInfo'),4),'little');u.mem_write(control,bytes(req));run(manager_export,(control,));result=C.c_int32(u.reg_read(UC_X86_REG_EAX)).value
 return result,EntryRequest.from_buffer_copy(u.mem_read(control,108))
def request(operation,client=1,module=None):
 q=EntryRequest();q.operation=operation;q.client=client;q.module=dll.OPTIONAL_HEADER.ImageBase if module is None else module;return q

if __name__ == '__main__' and '--clock-baseline' not in sys.argv:
 current_thread=776
 provisional=request(0,2,cursor_dll.OPTIONAL_HEADER.ImageBase);provisional.bindThread=1
 result,provisional=manager_call(provisional);assert result==1
 provisional.operation=1;assert manager_call(provisional)[0]==1
 current_thread=777
 attempt=request(0);attempt.bindThread=1
 assert manager_call(attempt)[0]==-2
 dead_threads.add(776)
 print('PASS initial Cursor binding cannot be stolen while alive; next Assist bind requires thread termination')

# Initialize the actual compiled Planner object as the DLL CRT does. Locate
# its constructor call through the unique final global-initializer tail; this
# does not mock the Planner implementation or claim a Windows loader test.
ctor_calls=[]
for section in dll.sections:
 if not section.Characteristics&0x20000000:continue
 data=section.get_data()
 for off in range(len(data)-24):
  code=data[off:off+24]
  if code[0]==0xb9 and code[5]==0xe8 and code[10]==0x68 and code[15]==0xe8 and code[20:]==bytes.fromhex('83c404c3'):
   objaddr=struct.unpack_from('<I',code,1)[0]
   if dll.OPTIONAL_HEADER.ImageBase+0x70000<=objaddr<dll.OPTIONAL_HEADER.ImageBase+dll.OPTIONAL_HEADER.SizeOfImage:
    ctor_calls.append((dll.OPTIONAL_HEADER.ImageBase+section.VirtualAddress+off+10+struct.unpack_from('<i',code,6)[0],objaddr))
assert len(ctor_calls)==1,('Planner initializer tail',ctor_calls)
run(ctor_calls[0][0],(),ctor_calls[0][1])
s=Shared();s.viewZoomPercent=100;s.magic=0x41533130;s.advancedRangeCells=2;s.version=int.from_bytes(dll.get_data(next(e.address for e in dll.DIRECTORY_ENTRY_EXPORT.symbols if e.name==b'AssistProtocolInfo'),4),'little');s.pid=123;s.heartbeat=clock;s.foreground=1;s.flags=1|2|4|16|32;s.scanMs=100;s.repairMs=2000
u.mem_write(SH,bytes(s));entry=dll.OPTIONAL_HEADER.ImageBase+next(e.address for e in dll.DIRECTORY_ENTRY_EXPORT.symbols if e.name==b'GameRuntimeHook')
u.mem_write(SH+0x500000,struct.pack('<10I',0x52543236,struct.unpack('<I',dll.get_data(next(e.address for e in dll.DIRECTORY_ENTRY_EXPORT.symbols if e.name==b'GameRuntimeInfo'),4))[0],123,456,1,1,1,clock,0,0))
u.mem_write(SH+0x500100,struct.pack('<7I',0,0,0x52545736,456,0,0,0))
def hook():
    global clock
    clock+=110;w(SH+Shared.heartbeat.offset,clock);run(entry,(0,1,0));return Shared.from_buffer_copy(u.mem_read(SH,C.sizeof(Shared)))

timer=STUB+0xf000
w(0x7e1530,timer)
timer_clobber={UC_X86_REG_ECX:0x13579bdf,UC_X86_REG_EDX:0}
timer_side_effect=None
def timing_callback(uc,a,size,_):
    if timer_side_effect:timer_side_effect()
    for reg,value in timer_clobber.items():uc.reg_write(reg,value)
    ret(clock,0)
u.hook_add(UC_HOOK_CODE,timing_callback,begin=timer,end=timer)

if __name__ == '__main__':
 for mode,backend in ((5,0),(5,1),(0,0),(0,1),(3,0),(4,0),(3,1),(4,1)):
  clock+=5100
  w(0xa8b238,mode);w(0xa8ed84,r(0xa8ed84)+10)
  w(0x8b41f8,0);w(0x8b41fc,0);w(0x8b4200,0);w(0xa802c8,0);w(0xa802d0,0)
  s=Shared.from_buffer_copy(u.mem_read(SH,C.sizeof(Shared)));s.megaQueueMode=backend;s.objectQueueMode=backend;u.mem_write(SH,bytes(s))
  s=hook();assert s.data.status==1
  s.command=1;s.request+=1;s.requestTick=clock;s.requestMatch=s.data.match
  u.mem_write(SH,bytes(s));s=hook()
  assert s.ack==s.request and s.result==1 and s.count==1,(mode,backend,s.result,s.count,s.data.lastSubmitError)
  at=0x8b4204 if mode in (0,5) else 0xa802d4
  assert r(0x8b41f8)==(1 if mode in (0,5) else 0)
  assert r(0xa802c8)==(0 if mode in (0,5) else 1)
  packet=bytes(u.mem_read(at,111));assert packet[0]==0x15 and packet[7:12]==struct.pack('<IB',12,0x34)
  assert struct.unpack_from('<I',packet,3)[0]==r(0xa8ed84)+(1 if mode in (0,5) else 0)
  assert s.data.lastSubmitError==0
  if mode in (0,5):assert s.data.objectQueueStatus==2
  print('PASS compiled repair constructor + queue: session',mode,'configured backend',backend)
  # Model observed execution before asking the next backend to repair again.
  b(building+0x6e8,1);w(0xa8ed84,r(0xa8ed84)+1);hook();b(building+0x6e8,0)
 # Legacy nonzero clock calibration must never affect the command age domain.
 s=Shared.from_buffer_copy(u.mem_read(SH,C.sizeof(Shared)))
 s.clockTick=clock-7;s.clockStart=counter;s.clockFrequency=1000000
 s.command=1;s.request+=1;s.requestTick=clock;s.requestMatch=s.data.match
 w(0xa8ed84,r(0xa8ed84)+1);u.mem_write(SH,bytes(s));run(entry,(0,1,0))
 s=Shared.from_buffer_copy(u.mem_read(SH,C.sizeof(Shared)))
 assert s.ack==s.request and s.result==1,('mixed-clock expiry',C.c_int32(s.result).value)
 print('PASS fresh request with QPC calibration 7ms behind: no unsigned expiry or heartbeat underflow')
 # Exercise production through the same real constructor/entry/queue, then
 # prove queue-full is distinct from timer failure and thread/field rejection.
 s=Shared.from_buffer_copy(u.mem_read(SH,C.sizeof(Shared)));s.flags=4096;s.techAllowed[1]=1;u.mem_write(SH,bytes(s))
 def issue(c,arg=0):
  s=Shared.from_buffer_copy(u.mem_read(SH,C.sizeof(Shared)));s.command=c;s.arg=arg;s.request+=1;s.requestTick=clock;s.requestMatch=s.data.match;u.mem_write(SH,bytes(s));s=hook();assert s.ack==s.request;return s
 s=issue(10,1);assert s.result==1
 w(0xa802c8,0);w(0xa802d0,0)
 s=issue(12,1);assert s.result==1 and r(0xa802c8)==1
 assert bytes(u.mem_read(0xa802d4,19))[7:19]==struct.pack('<iii',40,0,0)
 w(0xa8ed84,r(0xa8ed84)+1);w(0xa802c8,112);s=issue(12,1);assert C.c_int32(s.result).value==-6 and s.data.lastSubmitError==6
 w(0xa8ed84,r(0xa8ed84)+1);w(0xa802c8,0);w(0x7e1530,0);s=issue(12,1);assert C.c_int32(s.result).value==-6 and s.data.lastSubmitError==9
 print('PASS compiled production payload, queue-full reason and unavailable-timer reason; no gameplay/netplay certification')

 # SP high-level MegaMission selection must construct Enter into DoList.
 w(0x7e1530,timer);w(0xa8b238,5);w(0xa8ed84,r(0xa8ed84)+10)
 u.mem_write(t+0x388,struct.pack('<d',4.0));u.mem_write(engT+0x380,struct.pack('<d',1.0))
 w(0x8b41f8,0);w(0x8b41fc,0);w(0x8b4200,0)
 s=Shared.from_buffer_copy(u.mem_read(SH,C.sizeof(Shared)));s.megaQueueMode=1;u.mem_write(SH,bytes(s));s=hook()
 s=issue(15);assert s.data.loadIssued==1,(s.result,s.data.loadUnits,s.data.loadVehicles,s.data.lastSubmitError)
 packet=bytes(u.mem_read(0x8b4204,111))
 assert packet[0]==4 and packet[12]==7 and packet[7:12]==struct.pack('<IB',11,0x34)
 assert packet[19:24]==struct.pack('<IB',10,0x34) and r(0x8b41f8)==1
 assert s.data.megaQueueStatus==2
 print('PASS compiled SP AutoBoard with configured high-level MegaMission backend -> original Enter constructor -> DoList')

 # Reproduce the user's passenger-only selection, and a compatible Ares/MO
 # detour at an implementation that the virtual-call ABI need not leave pristine.
 w(0xa8ed84,20);w(0x8b41f8,0);w(0x8b4200,0);s=hook()
 vector(0xa8ecb8,[eng]);u.mem_write(0x51e3b0,bytes.fromhex('e9 00 00 00 00'))
 s=issue(15);assert s.data.loadVehicles>=1 and s.data.loadIssued==1,(s.result,s.data.loadVehicles,s.data.loadIssued)
 assert r(0x8b41f8)==1
 print('PASS passenger-only selection finds nearby transport; compatible MouseOverObject detour no longer disables AutoBoard')
 # An engine suspension must preserve epoch, pending serial and job ledger.
 round_id=s.data.match;old_request=s.request
 b(0xa8e9a0,0)
 s=Shared.from_buffer_copy(u.mem_read(SH,C.sizeof(Shared)));s.command=15;s.request+=1;s.requestTick=clock;s.requestMatch=round_id;u.mem_write(SH,bytes(s))
 for _ in range(20):s=hook();assert s.data.match==round_id and s.ack==old_request and s.data.commandStatus==0
 b(0xa8e9a0,1);s=hook();assert s.data.match==round_id and s.ack==s.request and s.data.commandStatus==1
 print('PASS 2.2-second engine suspension preserves the same match, pending request and loading jobs; resumes without a new round')
 # A slow UI heartbeat with a live owner is not a reason to disconnect/reset.
 s=Shared.from_buffer_copy(u.mem_read(SH,C.sizeof(Shared)));s.owner=456;s.heartbeat=clock-4001;u.mem_write(SH,bytes(s))
 run(entry,(0,1,0));s=Shared.from_buffer_copy(u.mem_read(SH,C.sizeof(Shared)))
 assert s.data.match==round_id and s.data.status==1 and s.data.commandStatus==1
 print('PASS live controller survives a >4-second heartbeat delay without resetting the command bridge')

 q=request(0);q.bindThread=1;result,lease=manager_call(q);assert result==1
 # A nested wake must retain pending work while the execution lease is busy.
 for _ in range(2):run(entry,(0,1,0))
 result,stats=manager_call(request(3));assert result==1 and stats.reentrant>=2
 stale=EntryRequest.from_buffer_copy(bytes(lease));stale.operation=1;stale.token+=1
 assert manager_call(stale)[0]==-3
 current_thread=778
 foreign=request(0,2,cursor_dll.OPTIONAL_HEADER.ImageBase);foreign.bindThread=1
 assert manager_call(foreign)[0]==-1
 stale=EntryRequest.from_buffer_copy(bytes(lease));stale.operation=1;assert manager_call(stale)[0]==-3
 current_thread=777
 lease.operation=1;assert manager_call(lease)[0]==1
 foreign.bindThread=1;current_thread=778;assert manager_call(foreign)[0]==-2;current_thread=777
 foreign.bindThread=0;result,second=manager_call(foreign);assert result==1
 second.operation=1;assert manager_call(second)[0]==1
 print('PASS single runtime rejects nested scheduling, foreign owners, wrong-thread and stale-token release')
 # Real code-page writes through the manager, with per-module patch ownership.
 # Idle or forged-token callers must never patch, even on the bound thread.
 idle=request(2);idle.address=0x646e90;idle.length=5
 assert manager_call(idle)[0]==-3
 q=request(0);result,patch_lease=manager_call(q);assert result==1
 # Explicit capture is the only nested path, and cannot release its parent.
 nested=request(5);nested.token=patch_lease.token
 result,child=manager_call(nested);assert result==1 and child.token!=patch_lease.token
 assert manager_call(q)[0]==-1
 parent_leave=EntryRequest.from_buffer_copy(bytes(patch_lease));parent_leave.operation=1
 assert manager_call(parent_leave)[0]==-3
 child.operation=1;child_copy=EntryRequest.from_buffer_copy(bytes(child));assert manager_call(child)[0]==1
 assert manager_call(child_copy)[0]==-3
 print('PASS actual manager rejects scheduler recursion, out-of-order and duplicate nested lease release')
 at=0x4a8eb0;original=bytes(u.mem_read(at,5));patch=b'\xe9'+struct.pack('<I',(entry-at-5)&0xffffffff)
 def patch_request(before,after,module=None,client=1):
  q=request(2,client,module);q.token=patch_lease.token;q.address=at;q.length=5
  for i in range(5):q.before[i]=before[i];q.after[i]=after[i]
  return q
 assert manager_call(patch_request(original,patch))[0]==1
 assert bytes(u.mem_read(at,5))==patch
 assert manager_call(patch_request(original,patch))[0]==-5
 assert manager_call(patch_request(patch,original,cursor_dll.OPTIONAL_HEADER.ImageBase))[0]==-1
 assert manager_call(patch_request(patch,original,client=2))[0]==-1
 assert bytes(u.mem_read(at,5))==patch
 assert manager_call(patch_request(patch,original))[0]==1
 assert bytes(u.mem_read(at,5))==original
 assert manager_call(patch_request(original,patch))[0]==1
 foreign_bytes=bytes.fromhex('e9 09 08 07 06');u.mem_write(at,foreign_bytes)
 assert manager_call(patch_request(patch,original))[0]==-5
 assert bytes(u.mem_read(at,5))==foreign_bytes
 q=request(3);q.address=at;result,stats=manager_call(q);assert result==1 and stats.conflict==1 and stats.contended>=1 and stats.wrongThread>=1
 print('PASS one patch writer; duplicate install and cross-module restore denied; foreign bytes preserved; conflict remains sticky')

 for frame_at in (0x55d360,0x48ce8a,0x646e90):
  saved_at=at;at=frame_at
  frame_original=bytes(u.mem_read(at,5));frame_patch=b'\xe9'+struct.pack('<I',(entry-at-5)&0xffffffff)
  result,_=manager_call(patch_request(frame_original,frame_patch))
  assert result==-5,(hex(frame_at),result)
  at=saved_at
 print('PASS central manager denies every retired frame/input hook candidate')
 patch_lease.operation=1;assert manager_call(patch_lease)[0]==1
 assert manager_call(idle)[0]==-3
 print('PASS patch writes require a current manager lease; idle, stale and foreign owners cannot write')

 # After a verified Assist bind, an idle manager still rejects another live
 # Assist thread. Only a signalled old thread handle permits migration.
 q=request(0);q.bindThread=1;current_thread=779
 assert manager_call(q)[0]==-2
 current_thread=777;dead_threads.add(777);current_thread=779
 result,migrated=manager_call(q);assert result==1
 migrated.operation=1;assert manager_call(migrated)[0]==1
 current_thread=777;assert manager_call(q)[0]==-2
 print('PASS actual manager refuses live-thread stealing and migrates only after retained old-thread handle is signalled')
 # A recycled numeric thread ID must replace the retained handle to the old
 # thread object; otherwise its signalled state would authorize later stealing.
 current_thread=779;dead_threads.add(779);opened=thread_open_counts[779]
 result,reused=manager_call(q);assert result==1 and thread_open_counts[779]==opened+1
 dead_threads.remove(779);reused.operation=1;assert manager_call(reused)[0]==1
 current_thread=780;assert manager_call(q)[0]==-2
 print('PASS recycled thread ID refreshes retained handle and cannot authorize a later live-thread takeover')

 import hashlib, json
 print("VERIFIED_INPUT_SHA256 " + json.dumps({str(p.name):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(sys.argv[1]),*[Path(fixture_paths[1]).with_name(n) for n in ("GameRuntime.dll",)]]},sort_keys=True))

