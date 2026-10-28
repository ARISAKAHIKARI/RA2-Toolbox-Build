#!/usr/bin/env python3
"""Compile/validate the three Vulkan 1.0 voxel raster passes using official tools."""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
source=Path(__file__).resolve().parents[1]/'external_reader/shaders/gpu_voxel_raster.comp'
lines=['#pragma once','#include <cstdint>','// Generated from gpu_voxel_raster.comp; use tools/build_gpu_voxel_kernels.py.','namespace external { namespace gpuvoxel {']
reports=[]
with tempfile.TemporaryDirectory(prefix='ra2-voxel-shader-') as tmp:
 for i,name in enumerate(['DepthShader','WinnerShader','ResolveShader']):
  binary=Path(tmp)/(name+'.spv')
  subprocess.run(['glslangValidator','-V','--target-env','vulkan1.0','-Os','-DVOXEL_PASS='+str(i),'-o',str(binary),str(source)],check=True)
  subprocess.run(['spirv-val','--target-env','vulkan1.0',str(binary)],check=True)
  data=binary.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data);assert words[0]==0x07230203
  lines.append('inline constexpr uint32_t '+name+'[]={')
  for n in range(0,len(words),8):lines.append(' '+','.join('0x%08xu'%w for w in words[n:n+8])+',')
  lines.append('};')
  reports.append({'name':name,'spirv_sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'words':len(words)})
lines+=['}}',''];a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text('\n'.join(lines))
print(json.dumps({'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'passes':reports}))
