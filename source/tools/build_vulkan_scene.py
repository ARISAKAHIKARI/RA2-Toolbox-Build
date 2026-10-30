#!/usr/bin/env python3
"""Compile and validate viewport and native-page Vulkan scene variants."""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
source=Path(__file__).resolve().parents[1]/'external_reader/shaders/vulkan_scene.comp'
lines=['#pragma once','#include <cstdint>','namespace external { namespace vulkancompute {']
with tempfile.TemporaryDirectory(prefix='ra2-vulkan-scene-') as tmp:
 for name,define in [('SceneShader',[]),('Scene64Shader',['-DSCENE_FP64']),('ScenePagesShader',['-DSCENE_PAGES']),('ScenePagesBankedShader',['-DSCENE_PAGES','-DSCENE_BANKED'])]:
  binary=Path(tmp)/(name+'.spv');subprocess.run(['glslangValidator','-V','--target-env','vulkan1.0','-Os',*define,'-o',str(binary),str(source)],check=True);subprocess.run(['spirv-val','--target-env','vulkan1.0',str(binary)],check=True)
  data=binary.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data)
  lines.append('inline constexpr uint32_t '+name+'[]={')
  for i in range(0,len(words),8):lines.append(' '+','.join('0x%08xu'%w for w in words[i:i+8])+',')
  lines.append('};');print(json.dumps({'variant':name,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'spirv_sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)}))
lines+=['}}',''];a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text('\n'.join(lines))
