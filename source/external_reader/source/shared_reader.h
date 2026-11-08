#pragma once
#include "process_reader.h"
namespace external {
// Ordinary resource sessions retain task caches; WorldFrameScope pins one
// actual Runtime-produced immutable read set shared across every session.
using ReadSession=ProcessReader;
using SharedProcessReader=ReadSession; // compatibility for external consumers
struct SharedReadView {ReadSession*reader;bool operator()(uint32_t p,void*out,size_t n)const{return reader->read(p,out,n);}bool fresh(uint32_t p,void*out,size_t n)const{return reader->readFresh(p,out,n);}bool live(uint32_t p,void*out,size_t n)const{return reader->readLive(p,out,n);}bool optional(uint32_t p,void*out,size_t n)const{return reader->readOptional(p,out,n);}void prefetch(const std::vector<uint32_t>&addresses,size_t span)const{reader->prefetch(addresses,span);}};
}
