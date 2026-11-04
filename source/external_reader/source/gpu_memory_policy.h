#pragma once
namespace external { namespace gpumemory {
// Uploads are CPU writes; readbacks are CPU reads. Coherency alone does not
// make mapped reads fast. Cached noncoherent memory is valid after invalidate.
inline int score(bool host,bool readback,bool visible,bool coherent,bool cached,bool local){
 if(!host)return local?4:0;
 if(!visible)return -1;
 return readback?(cached?8:0)+(coherent?4:0)+(!local?1:0):
                 (coherent?8:0)+(cached?2:0);
}
}}
