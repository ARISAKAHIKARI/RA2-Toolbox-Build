#pragma once
#include <initializer_list>
namespace suite { namespace categories {
enum Category {Video=0,Keyboard=1,Tactical=2,Entry=3,Automatic=4,Rules=5,Experimental=6,Debug=7,Count=8,Control=Tactical};
inline const wchar_t*name(int category){constexpr const wchar_t*names[]={L"视频",L"键鼠与选兵",L"战术操作",L"进驻与装卸",L"自动功能",L"游戏规则",L"实验功能",L"调试"};return category>=0&&category<Count?names[category]:L"设置";}
inline bool rule(int id){
 if(id==9250||id==9251||id==9252||id==807||id==865||id==9193||id==9194)return true;
 if(id>=420&&id<=428)return true;
 switch(id){case 108:case 208:case 9199:case 476:case 808:case 809:case 814:case 815:case 896:case 860:case 722:case 9189:case 9190:case 9096:case 9097:case 9098:case 9470:case 9471:case 9473:case 9472:case 9753:case 9607:case 9612:return true;default:return false;}
}
inline bool any(const int*ids,unsigned count,std::initializer_list<int> values){for(unsigned i=0;i<count;++i)for(int value:values)if(ids[i]==value)return true;return false;}
// Classify a complete native row. Feature cards can refine this ownership in
// the Web UI, while legacy/non-card controls still land on the right page.
inline int row(int section,const int*ids,unsigned count){
 if(any(ids,count,{111,211,459,451,453,702,703,704,732,700,701}))return Entry;
 if(any(ids,count,{724,795,440,441,443,444,445,454,853,9604,9608}))return Keyboard;
 if(any(ids,count,{458,450,729,787,818,802,705,706,707,708,709,710,711,712,713,714,715,716,717,718,733,734,751,9195,9794,9795,477,478,730,719,725,726,735,720,727,728,737,721,789,738,9400,9401,9402,9440,9441,9442,9443,9444,9445,9446,9601,9602,9606,9610,9611,9615,803,894,866,781,782,783,784,9170,9171,9172,9173,9174,9175,9176,9177,788,473,474,9460,9461}))return Tactical;
 for(unsigned i=0;i<count;++i)if(rule(ids[i]))return Rules;
 for(unsigned i=0;i<count;++i){int id=ids[i];if(id==9750||id==9751||id==9185||id==9186||id==9253||id==9254||(id>=9090&&id<=9093)||id==9403||id==9404||id==9406||id==9407||id==9613||id==9614||(id>=9462&&id<=9464)||(id>=9450&&id<=9455))return Experimental;
  if(id==9405)return Video;}
 for(unsigned i=0;i<count;++i)if(ids[i]==100||ids[i]==200||ids[i]==312||ids[i]==455||ids[i]==456||ids[i]==899||ids[i]==780||ids[i]==9183||ids[i]==9603||ids[i]==9605||(ids[i]>=9502&&ids[i]<=9511))return Automatic;
 if(section==3)return Video;
 if(section==2||section==5)return Automatic;
 for(unsigned i=0;i<count;++i){int id=ids[i];if(id==9042||id==446||id==447||(id>=9000&&id<9015)||id==832||id==833)return Automatic;}
 return section==4?Experimental:Keyboard;
}
}}
