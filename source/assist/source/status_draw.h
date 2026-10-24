// Small vector badges in game pixels, independent of DPI/text zoom.
#include "status_badges.h"
static void drawUnitStatus(Graphics&g,const external::status::Marker&m,const visual::LabelView&v,bool pulse){
 auto saved=g.Save();g.SetSmoothingMode(SmoothingModeAntiAlias);
 if(m.parasite){
  auto spiderState=g.Save();g.TranslateTransform(v.x,v.y);g.ScaleTransform(statusbadges::SpiderScale,statusbadges::SpiderScale);
  Color color(m.parasiteColor);Pen shadow(Color(245,5,10,17),3.1f),leg(Color(pulse?255:155,color.GetR(),color.GetG(),color.GetB()),1.5f);
  // Four distinct pairs of bent legs and a separate head/abdomen.
  for(int side:{-1,1})for(int i=0;i<4;++i){float y=-4.f+i*2.5f;PointF p[]={{side*1.5f,y},{side*4.f,y+(i<2?-2.f:1.f)},{side*6.5f,y+(i<2?-3.f:2.f)}};
   g.DrawLines(&shadow,p,3);g.DrawLines(&leg,p,3);
  }
  SolidBrush body(Color(pulse?255:180,color.GetR(),color.GetG(),color.GetB())),edge(Color(245,5,10,17)),eye(Color(255,245,249,255));
  g.FillEllipse(&edge,-3.8f,-3.5f,7.6f,8.f);g.FillEllipse(&body,-2.7f,-2.5f,5.4f,6.f);
  g.FillEllipse(&edge,-2.8f,-6.f,5.6f,4.5f);g.FillEllipse(&body,-1.8f,-5.f,3.6f,3.f);g.FillEllipse(&eye,-.8f,-4.5f,1.4f,1.2f);
  g.Restore(spiderState);
 }
 auto layout=statusbadges::layout(v.x,v.y,m.buffs,m.parasite!=0);
 for(int i=0;i<layout.count;++i){unsigned k=layout.kinds[i];auto p=layout.icons[i];Color color=k==0?Color(255,255,182,69):k==1?Color(255,104,201,255):Color(255,117,235,156);
  SolidBrush bg(Color(230,9,17,26)),ink(color);Pen line(color,1.25f);line.SetLineJoin(LineJoinRound);
  g.FillEllipse(&bg,p.x-4.5f,p.y-4.5f,9.f,9.f);
  if(k==0){PointF arrow[]={{p.x-2.6f,p.y+2.3f},{p.x+.1f,p.y-1.f},{p.x-1.4f,p.y-1.5f},{p.x+3.f,p.y-3.f},{p.x+1.7f,p.y+1.3f},{p.x+1.1f,p.y-.1f},{p.x-1.2f,p.y+3.f}};g.FillPolygon(&ink,arrow,7);}
  else if(k==1){PointF shield[]={{p.x-3,p.y-2.5f},{p.x,p.y-3.4f},{p.x+3,p.y-2.5f},{p.x+2.3f,p.y+1.1f},{p.x,p.y+3.4f},{p.x-2.3f,p.y+1.1f}};g.DrawPolygon(&line,shield,6);g.DrawLine(&line,p.x,p.y-2.2f,p.x,p.y+2.2f);}
  else{for(float dx:{-2.f,1.2f}){PointF chevron[]={{p.x+dx-1,p.y-2.5f},{p.x+dx+1.4f,p.y},{p.x+dx-1,p.y+2.5f}};g.DrawLines(&line,chevron,3);}}
 }
 g.Restore(saved);
}
