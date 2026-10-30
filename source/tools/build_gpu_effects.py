#!/usr/bin/env python3
"""Compile/validate GPU effect commands; does not load a graphics driver."""
from pathlib import Path
import argparse, hashlib, json, struct, subprocess, tempfile
p=argparse.ArgumentParser()
p.add_argument('--glslang', default='glslangValidator')
p.add_argument('--validator', default='spirv-val')
p.add_argument('--output', type=Path, required=True)
a=p.parse_args()
source=Path(__file__).resolve().parents[1]/'external_reader/shaders/gpu_effects.comp'
with tempfile.TemporaryDirectory(prefix='ra2-effects-') as tmp:
    binary=Path(tmp)/'effects.spv'
    subprocess.run([a.glslang,'-V','--target-env','vulkan1.0','-Os','-o',str(binary),str(source)],check=True)
    subprocess.run([a.validator,'--target-env','vulkan1.0',str(binary)],check=True)
    data=binary.read_bytes()
words=struct.unpack('<'+'I'*(len(data)//4),data)
lines=['#pragma once','#include <cstdint>','namespace external { namespace vulkancompute {','inline constexpr uint32_t EffectsShader[]={']
for i in range(0,len(words),8):lines.append(' '+','.join('0x%08xu'%w for w in words[i:i+8])+',')
lines+=['};','}}','']
a.output.write_text('\n'.join(lines))
print(json.dumps({'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'spirv_sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'graphics_driver_loaded':False}))
