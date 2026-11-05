"""Rebuild the two viewport shaders; retain byte-identical native-page shaders.

The adaptive-bin parameter is viewport-only. Native sparse pages still use
their established 32px layout and shader binaries. No download is performed.
"""
from pathlib import Path
import argparse, hashlib, json, re, struct, subprocess, tempfile

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    root=Path(__file__).resolve().parents[1]
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--glslang',type=Path,required=True)
    p.add_argument('--output',type=Path,default=root/'external_reader/source/vulkan_scene_spirv.h')
    p.add_argument('--report',type=Path,required=True)
    a=p.parse_args();compiler=a.glslang.resolve();source=root/'external_reader/shaders/vulkan_scene.comp'
    header=root/'external_reader/source/vulkan_scene_spirv.h';text=header.read_text(encoding='utf-8-sig')
    version=subprocess.check_output([str(compiler),'--version'],text=True)
    if 'Glslang Version: 11:16.6.0' not in version:raise SystemExit('Expected pinned glslang 16.6.0')
    pages=text[text.index('inline constexpr uint32_t ScenePagesShader'):]
    output='#pragma once\n#include <cstdint>\nnamespace external { namespace vulkancompute {\n'
    reports={}
    a.report.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(dir=a.report.parent.resolve(),prefix='ra2-scene-shader-') as raw:
        directory=Path(raw).resolve();assert directory.is_relative_to(a.report.parent.resolve())
        for name,defines in [('SceneShader',[]),('Scene64Shader',['-DSCENE_FP64=1'])]:
            binary=directory/(name+'.spv')
            command=[str(compiler),'-V','--target-env','vulkan1.0',*defines,'-o',str(binary),str(source)]
            run=subprocess.run(command,capture_output=True,text=True)
            if run.returncode:raise SystemExit(run.stdout+run.stderr)
            data=binary.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data)
            output+='inline constexpr uint32_t '+name+'[]={\n'
            output+=''.join(' '+','.join(f'0x{x:08x}u' for x in words[n:n+8])+',\n' for n in range(0,len(words),8))+'};\n'
            reports[name]={'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'words':len(words)}
    output+=pages
    # The unrelated page variants are retained, not silently rebuilt with a
    # different compiler. Their macro branch was untouched by this change.
    assert output[output.index('inline constexpr uint32_t ScenePagesShader'):]==pages
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(output,encoding='utf-8',newline='\n')
    report={'schema':'ra2.scene-shader-build.v1','compiler_sha256':digest(compiler),'compiler_version':version,'source_sha256':digest(source),'output_sha256':digest(a.output),'native_page_suffix_sha256':hashlib.sha256(pages.encode()).hexdigest(),'shaders':reports}
    a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
