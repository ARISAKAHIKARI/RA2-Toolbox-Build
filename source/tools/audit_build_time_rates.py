"""Verify the supported executable's multiplayer speed table, without a game process.
Requires pefile and unicorn. Private game bytes are never copied to the report.
"""
import argparse,hashlib,json,struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP

p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('--output',required=True);a=p.parse_args()
data=Path(a.game).read_bytes();sha=hashlib.sha256(data).hexdigest()
assert sha=='268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
pe=pefile.PE(data=data);base=pe.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
u.mem_map(0x20000000,0x10000)
cases=[]
for speed,rate in enumerate([60,45,30,20,15,12]):
 u.mem_write(0xA8B268,struct.pack('<i',speed));u.mem_write(0xA8B570,bytes(4))
 u.reg_write(UC_X86_REG_ESI,10);u.reg_write(UC_X86_REG_EDI,0)
 u.emu_start(0x79730D,0x7973B6,count=100)
 actual=struct.unpack('<I',u.mem_read(0xA8B570,4))[0];assert actual==rate,(speed,actual,rate)
 # Original UI conversion is internal GameSpeed = 6 - visible selector.
 u.reg_write(UC_X86_REG_EAX,6-speed);u.emu_start(0x4E1E14,0x4E1E1B,count=4)
 assert u.reg_read(UC_X86_REG_ECX)==speed
 cases.append(dict(visible_speed=6-speed,internal_speed=speed,multiplayer_cap=actual))
# The actual runtime negotiation reads the live option, not the setup copy.
runtime=[]
for speed,rate in enumerate([60,45,30,20,15,12]):
 u.mem_write(0xA8EB60,struct.pack('<i',speed));u.reg_write(UC_X86_REG_EBX,0);u.reg_write(UC_X86_REG_ESI,1000);u.reg_write(UC_X86_REG_ESP,0x20008000)
 u.emu_start(0x647C43,0x647C74,count=100)
 actual=struct.unpack('<I',u.mem_read(0x20008014,4))[0];assert actual==rate,(speed,actual,rate)
 runtime.append(dict(internal_speed=speed,cap=actual))
u.reg_write(UC_X86_REG_ESI,0x20000100);u.mem_write(0x20000107,struct.pack('<i',1));u.mem_write(0xA8B268,struct.pack('<i',4))
u.emu_start(0x4C794B,0x4C7954,count=10)
assert struct.unpack('<I',u.mem_read(0xA8EB60,4))[0]==1 and struct.unpack('<I',u.mem_read(0xA8B268,4))[0]==4
# Distinguish the selected duration from measured FPS and document clock granularity.
assert pe.get_data(0x55D79E-base,6)==bytes.fromhex('8b3560eba800')
assert pe.get_data(0x55D7BC-base,6)==bytes.fromhex('893550738800')
assert pe.get_data(0x6C8C46-base,4)==bytes.fromhex('c1e804c3')
assert pe.get_data(0x55D4FB-base,5)==bytes.fromhex('b83c000000')
report=dict(game_sha256=sha,kind='original-instruction controlled fixture and signature checks',cases=cases,runtime_caps=runtime,live_speed_event_preserves_setup_copy=True,
 addresses=dict(speed_selector_conversion='4E1E14',multiplayer_setup='79730D..7973B6',configured_rate='A8B570',selected_speed='A8EB60',singleplayer_duration_copy='55D79E..55D7BC',clock_quantization='6C8C46'),
 singleplayer_visible_speed_rates=[12,15,20,30,60,60],
 boundary='Single-player uses nominal 60-tick speed ceilings; its original timer quantizes wall time at 16 ms. Unlimited speed 6 intentionally follows speed 5. No actual wall-clock production, platform-modified timer, or multiplayer test.')
Path(a.output).parent.mkdir(parents=True,exist_ok=True);Path(a.output).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('PASS 6 setup + 6 running multiplayer cap branches, live speed event, original UI conversion and 4 timing signatures; no game process.')
