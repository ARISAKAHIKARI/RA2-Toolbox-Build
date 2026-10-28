#!/usr/bin/env python3
"""Compile/validate the checked-in Vulkan 1.0 kernel and emit portable words."""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
source=Path(__file__).resolve().parents[1]/'external_reader/shaders/vulkan_bilinear.comp'
with tempfile.TemporaryDirectory(prefix='ra2-vulkan-shader-') as tmp:
 binary=Path(tmp)/'resolve.spv'
 subprocess.run(['glslangValidator','-V','--target-env','vulkan1.0','-Os','-o',str(binary),str(source)],check=True)
 subprocess.run(['spirv-val','--target-env','vulkan1.0',str(binary)],check=True)
 data=binary.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data)
 assert words[0]==0x07230203
 lines=['#pragma once','#include <cstdint>','// Generated from vulkan_bilinear.comp; regenerate with tools/build_vulkan_kernel.py.','namespace external { namespace vulkancompute {','inline constexpr uint32_t BilinearShader[]={']
 for i in range(0,len(words),8):lines.append(' '+','.join('0x%08xu'%w for w in words[i:i+8])+',')
 lines+=['};','}}','']
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text('\n'.join(lines))
 print(json.dumps({'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'spirv_sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'words':len(words)}))
