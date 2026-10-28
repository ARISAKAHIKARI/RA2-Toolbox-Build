#!/usr/bin/env python3
"""Execute checked-in xBRZ GLSL function text as a C++ semantic model.

This is NOT Vulkan, SPIR-V validation, or a GPU precision/performance claim.
Only declarations/qualifiers/vector syntax are adapted; the shader edge rules,
coefficients, integer blending and operation order are read from the real files.
The oracle remains the untouched upstream xBRZ 1.9 implementation.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SHADERS = ROOT / 'external_reader/shaders'


def adapt_common(text):
    text = re.sub(r'layout\([^\n]+\) readonly buffer NativeSource \{ uint pixels\[\]; \} nativeSource;', 'struct { std::vector<uint> pixels; } nativeSource;', text)
    text = re.sub(r'layout\([^\n]+\) buffer CornerFlags \{ uint value\[\]; \} cornerFlags;', 'struct { std::vector<uint> value; } cornerFlags;', text)
    text = re.sub(r'layout\([^\n]+\) writeonly buffer Reconstructed \{ uint pixels\[\]; \} reconstructed;', 'struct { std::vector<uint> pixels; } reconstructed;', text)
    text = text.replace('layout(push_constant) uniform XbrzParams {', 'struct XbrzParams {')
    return adapt(text)


def adapt(text):
    text = re.sub(r'^#(?:version|extension|include)[^\n]*\n', '', text, flags=re.M)
    text = re.sub(r'^layout\(local_size_[^\n]*\) in;\n', '', text, flags=re.M)
    text = re.sub(r'\bprecise\s+', '', text)
    text = re.sub(r'\binout\s+', '', text)
    text = re.sub(r'(?<=\d)LF\b', '', text)
    return text.replace('gl_GlobalInvocationID.xy', 'invocation')


PREFIX = r'''
#include "gpu_xbrz_reference.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace shader {
using uint=uint32_t;using std::min;using std::sqrt;
struct uvec2 {uint x,y;};
struct ivec2 {int x,y;explicit ivec2(int n):x(n),y(n){} ivec2(int a,int b):x(a),y(b){} explicit ivec2(uvec2 u):x(int(u.x)),y(int(u.y)){} };
inline ivec2 operator+(ivec2 a,ivec2 b){return ivec2(a.x+b.x,a.y+b.y);}
inline ivec2 operator-(ivec2 a,ivec2 b){return ivec2(a.x-b.x,a.y-b.y);}
uvec2 invocation;
'''
SUFFIX = r'''
}
int main(){
 using namespace external::gpuxbrztest;const auto all=cases();uint64_t pixels=0,dispatches=0;
 constexpr uint32_t canary=0xdeadbeefu;
 for(const auto&c:all)for(int factor:{2,3,4}){
  const auto expected=upstream(*c.source,c.width,c.height,factor);
  const size_t flagsCount=size_t(c.width+1)*(c.height+1);
  shader::nativeSource.pixels.assign(c.source->size()+19,canary);
  std::copy(c.source->begin(),c.source->end(),shader::nativeSource.pixels.begin()+7);
  shader::cornerFlags.value.assign(flagsCount+23,canary);
  shader::reconstructed.pixels.assign(expected.size()+29,canary);
  shader::p={7u,unsigned(c.width),unsigned(c.height),13u,unsigned(factor),11u};
  for(int y=0;y<((c.height+1+7)/8)*8;++y)for(int x=0;x<((c.width+1+7)/8)*8;++x){shader::invocation={unsigned(x),unsigned(y)};shader::preprocess();}
  for(int y=0;y<((c.height+7)/8)*8;++y)for(int x=0;x<((c.width+7)/8)*8;++x){shader::invocation={unsigned(x),unsigned(y)};shader::scale();}
  for(size_t i=0;i<expected.size();++i){const uint32_t actual=shader::reconstructed.pixels[i+13];
   if(actual!=expected[i]){std::fprintf(stderr,"FAIL %s %dx%d factor%d output(%zu,%zu) expected=%08x actual=%08x\n",c.label.c_str(),c.width,c.height,factor,i%size_t(c.width*factor),i/size_t(c.width*factor),expected[i],actual);return 1;}
   const unsigned alpha=actual>>24;assert((actual&255)<=alpha&&((actual>>8)&255)<=alpha&&((actual>>16)&255)<=alpha);
  }
  for(size_t i=0;i<11;++i)assert(shader::cornerFlags.value[i]==canary);
  for(size_t i=flagsCount+11;i<shader::cornerFlags.value.size();++i)assert(shader::cornerFlags.value[i]==canary);
  for(size_t i=0;i<13;++i)assert(shader::reconstructed.pixels[i]==canary);
  for(size_t i=expected.size()+13;i<shader::reconstructed.pixels.size();++i)assert(shader::reconstructed.pixels[i]==canary);
  pixels+=expected.size();dispatches+=2;
 }
 std::printf("PASS: checked-in GLSL C++ semantic model cases=%zu factors2,3,4 exact_pixels=%llu modeled_dispatches=%llu; offsets/canaries/8x8 overhang/premul; NOT GPU execution\n",all.size(),(unsigned long long)pixels,(unsigned long long)dispatches);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='g++')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    original = {name: (SHADERS / name).read_bytes() for name in ('vulkan_xbrz_common.glsl', 'vulkan_xbrz_preprocess.comp', 'vulkan_xbrz_scale.comp')}
    print(json.dumps({'model_shader_sha256': {name: hashlib.sha256(data).hexdigest() for name, data in original.items()}, 'gpu_execution': False}), flush=True)
    common = adapt_common(original['vulkan_xbrz_common.glsl'].decode())
    preprocess = adapt(original['vulkan_xbrz_preprocess.comp'].decode()).replace('void main(){', 'void preprocess(){')
    scale = adapt(original['vulkan_xbrz_scale.comp'].decode()).replace('void main(){', 'void scale(){')
    assert 'layout(' not in common + preprocess + scale
    with tempfile.TemporaryDirectory(prefix='ra2-gpu-xbrz-model-') as folder:
        source = Path(folder) / 'model.cpp'
        binary = Path(folder) / 'model'
        source.write_text(PREFIX + common + preprocess + scale + SUFFIX)
        flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitize else ['-O2']
        subprocess.run([args.compiler, '-std=c++17', '-ffp-contract=off', *flags, '-I' + str(ROOT / 'external_reader/source'), str(source), str(ROOT / 'third_party/xbrz/xbrz.cpp'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
