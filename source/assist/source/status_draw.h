// Host GDI+ renderer; code-native pixel icons, cached by parasite owner color.
#include "status_badges.h"
struct StatusRaster {statusbadges::Sprite sprite;std::unique_ptr<Bitmap> image;};
static std::unordered_map<uint32_t,StatusRaster> spiderRasters;
static StatusRaster buffRasters[3];
static Bitmap* statusBitmap(StatusRaster&r,statusbadges::Sprite sprite){
 if(!r.image){r.sprite=sprite;r.image=std::make_unique<Bitmap>(sprite.width,sprite.height,sprite.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(r.sprite.pixels.data()));}
 return r.image.get();
}
static void drawUnitStatus(Graphics&g,const external::status::Marker&m,const visual::LabelView&v){
 auto saved=g.Save();g.SetInterpolationMode(InterpolationModeNearestNeighbor);g.SetPixelOffsetMode(PixelOffsetModeHalf);
 if(m.parasite){
  if(spiderRasters.size()>64)spiderRasters.clear();auto&r=spiderRasters[m.parasiteColor];
  if(!r.image)statusBitmap(r,statusbadges::spider(m.parasiteColor));
  g.DrawImage(r.image.get(),v.x-7.f,v.y-7.f,14.f,14.f);
 }
 auto layout=statusbadges::layout(v.x,v.y,float(m.x+m.cellOffsetX),float(m.y+m.cellOffsetY),m.buffs,m.parasite!=0);
 // The grid itself must not interpolate with the unit: its center comes from
 // the current camera/sample, while the unit follows the common Motion clock.
 PointF cell[]={{layout.center.x,layout.center.y-15},{layout.center.x+30,layout.center.y},{layout.center.x,layout.center.y+15},{layout.center.x-30,layout.center.y}};
 GraphicsPath clip;clip.AddPolygon(cell,4);g.SetClip(&clip,CombineModeIntersect);
 for(int i=0;i<layout.count;++i){unsigned k=layout.kinds[i];auto p=layout.icons[i];
  if(!buffRasters[k].image)statusBitmap(buffRasters[k],statusbadges::buff(k));
  g.DrawImage(buffRasters[k].image.get(),p.x-3.5f,p.y-3.5f,7.f,7.f);
 }
 g.Restore(saved);
}
