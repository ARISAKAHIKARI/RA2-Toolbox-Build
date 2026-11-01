#pragma once
namespace suite { namespace categories {
enum Category {Video,Control,Automatic,Rules,Experimental,Count};
inline const wchar_t*name(int category){constexpr const wchar_t*names[]={L"视频",L"控制",L"自动功能",L"游戏规则",L"实验性功能"};return category>=0&&category<Count?names[category]:L"设置";}
inline bool rule(int id){
 if(id==9250||id==9251||id==9252||id==807||id==865||id==9193||id==9194||id==9183)return true;
 if(id>=420&&id<=428)return true;
 switch(id){case 108:case 208:case 9199:case 476:case 808:case 809:case 814:case 815:case 896:case 860:case 722:case 9189:case 9190:case 9185:case 9186:case 9096:case 9097:case 9098:return true;default:return false;}
}
// Classify a complete label/input row, keeping related controls together.
// Historical page numbers describe control ownership, not navigation.
inline int row(int section,const int*ids,unsigned count){
 for(unsigned i=0;i<count;++i)if(rule(ids[i]))return Rules;
 for(unsigned i=0;i<count;++i)if(ids[i]==455||ids[i]==456||ids[i]==899)return Automatic;
 if(section==3)return Video;
 if(section==2||section==5)return Automatic;
 for(unsigned i=0;i<count;++i){int id=ids[i];if(id==9042||id==446||id==447||(id>=9000&&id<9015)||id==832||id==833)return Automatic;
  if(id==473||id==474||(id>=9170&&id<=9175)||id==788||id==9191||id==9192)return Control;}
 return section==4?Experimental:Control;
}
}}
