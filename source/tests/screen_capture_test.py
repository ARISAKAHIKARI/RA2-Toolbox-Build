"""Portable tests for the capture analyzer. Does NOT exercise Windows GDI."""
import importlib.util,struct,zlib
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'tools/screen_capture_test.py'
s=importlib.util.spec_from_file_location('capture',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
assert m.intersection((-1920,0,-100,1080),(-1500,-100,1920,900))==(-1500,0,-100,900)
assert m.intersection((0,0,10,10),(10,0,20,10)) is None
assert m.quantiles([])=={'samples':0}
assert m.quantiles(list(range(1,101)))=={'samples':100,'p50_ms':50,'p95_ms':95,'p99_ms':99,'max_ms':100}
data=m.png(2,2,bytes([0,0,255,255,0,255,0,255,255,0,0,255,255,255,255,255]));pos=8;compressed=b''
assert data[:8]==b'\x89PNG\r\n\x1a\n'
while pos<len(data):
 size=struct.unpack('>I',data[pos:pos+4])[0];kind=data[pos+4:pos+8];payload=data[pos+8:pos+8+size];crc=struct.unpack('>I',data[pos+8+size:pos+12+size])[0]
 assert zlib.crc32(kind+payload)&0xffffffff==crc
 if kind==b'IDAT':compressed+=payload
 pos+=size+12
assert zlib.decompress(compressed)==bytes([0,255,0,0,0,255,0,0,0,0,255,255,255,255])
for args in [(0,1,b''),(1,1,b''),(20000,20000,b'')]:
 try:m.png(*args);raise AssertionError('invalid input accepted')
 except ValueError:pass
print('PASS capture analyzer: negative desktop coordinates, clipping, empty samples, nearest-rank P50/P95/P99, lossless BGRA->PNG and dimensions')
for version in (41,):
 prefix=struct.pack('<19I',0x41533130,version,123,*([0]*16))
 assert m.assist_prefix(prefix,prefix,123)[1]==version
 assert m.assist_prefix(prefix,prefix,124) is None
 assert m.assist_prefix(prefix,prefix[:-1],123) is None
for unsupported in (33,34,35,36,37,38,39,40,42):
 future=struct.pack('<19I',0x41533130,unsupported,123,*([0]*16))
 assert m.assist_prefix(future,future,123) is None
assert m.assist_prefix(b'',b'',123) is None
print('PASS passive capture ACK prefix: current mapping protocol 41; PID, torn, short and unknown-version rejection')
