#pragma once
#include "voxel_posture.h"
namespace external {namespace voxel {
// Immutable per-section HVA poses relative to the already captured frame zero.
// A face retains its section owner; shared model copies never reread the game.
struct Motion {std::vector<voxelposture::Matrix> changes;};
inline bool inverseMotion(const voxelposture::Matrix&a,voxelposture::Matrix&b){
 double d=double(a[0])*(double(a[5])*a[10]-double(a[6])*a[9])-double(a[1])*(double(a[4])*a[10]-double(a[6])*a[8])+double(a[2])*(double(a[4])*a[9]-double(a[5])*a[8]);if(!std::isfinite(d)||std::abs(d)<1e-9)return false;
 b={{float((double(a[5])*a[10]-double(a[6])*a[9])/d),float((double(a[2])*a[9]-double(a[1])*a[10])/d),float((double(a[1])*a[6]-double(a[2])*a[5])/d),0,float((double(a[6])*a[8]-double(a[4])*a[10])/d),float((double(a[0])*a[10]-double(a[2])*a[8])/d),float((double(a[2])*a[4]-double(a[0])*a[6])/d),0,float((double(a[4])*a[9]-double(a[5])*a[8])/d),float((double(a[1])*a[8]-double(a[0])*a[9])/d),float((double(a[0])*a[5]-double(a[1])*a[4])/d),0}};
 for(int row=0;row<3;++row)b[row*4+3]=-(b[row*4]*a[3]+b[row*4+1]*a[7]+b[row*4+2]*a[11]);return true;
}
inline voxelposture::Matrix motionBasis(voxelposture::Matrix m,float offset,float sx,float sy){for(int col=0;col<4;++col){m[col]*=sx;m[4+col]*=-sy;}m[3]+=offset*sx;return m;}
}}
