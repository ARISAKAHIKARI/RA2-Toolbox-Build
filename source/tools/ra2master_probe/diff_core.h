#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace probe {
struct Snapshot {
    std::vector<uint8_t> bytes, valid, stable;
};
struct Run { size_t offset, length; };
inline bool available(const Snapshot& s, size_t i) {
    return i < s.bytes.size() && i < s.valid.size() && s.valid[i];
}
inline bool fixed(const Snapshot& s, size_t i) {
    return available(s,i) && i < s.stable.size() && s.stable[i];
}
inline bool changed(const Snapshot& a,const Snapshot& b,size_t i) {
    return available(a,i) && available(b,i) && a.bytes[i] != b.bytes[i];
}
inline bool reversible(const Snapshot& off,const Snapshot& on,const Snapshot& again,size_t i) {
    return fixed(off,i) && fixed(on,i) && fixed(again,i) &&
        off.bytes[i] != on.bytes[i] && off.bytes[i] == again.bytes[i];
}
template<class Predicate> std::vector<Run> runs(size_t start,size_t end,Predicate p) {
    std::vector<Run> out;
    for(size_t i=start;i<end;) {
        if(!p(i)){++i;continue;}
        const size_t begin=i++;
        while(i<end && p(i) && i-begin<64)++i;
        out.push_back({begin,i-begin});
    }
    return out;
}
inline std::string hex(const uint8_t* p,size_t n) {
    static const char h[]="0123456789ABCDEF";
    std::string s; s.reserve(n*2);
    for(size_t i=0;i<n;++i){s+=h[p[i]>>4];s+=h[p[i]&15];}
    return s;
}
}
