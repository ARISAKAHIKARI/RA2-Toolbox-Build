// Frozen reference: v1.0.51-pre.3 a2c4ed3, original edge converter.
static missilecompute::Edge legacyMissileEdgeFacts(uint32_t p,uint32_t q,uint32_t weapon,int close,int airborne){
 missilecompute::Edge e{};e.close=0;if(!weapon||!airborne)return e;
 auto info=rawWeaponInfo(weapon,true);int armor=get<int>(typeOf(q,kind(q))+0x9C);
 if(armor<0||armor>10||!(info.flags&planner::AirWeapon)||!(info.selectMask&(1u<<armor))||
    (info.flags&(planner::HealingWeapon|planner::MindControlWeapon|planner::TemporalWeapon|planner::ParasiteWeapon|planner::LocomotorWeapon)))return e;
 double damage=double(get<int>(weapon+0xA4))*info.effectiveness[armor];if(damage<=0)return e;
 if(sh->lowDamageFilter&&!(get<double>(get<uint32_t>(weapon+0xAC)+0xA0+armor*8)>0.02))return e;
 double dx=double(get<int>(q+0x9C))-get<int>(p+0x9C),dy=double(get<int>(q+0xA0))-get<int>(p+0xA0),dz=double(get<int>(q+0xA4))-get<int>(p+0xA4);
 double distance=std::sqrt(dx*dx+dy*dy+dz*dz);if(!std::isfinite(distance)||distance>1048576)return e;
 int eta=0;
 if(!close){
  int bonus=std::clamp(get<int>(typeOf(p,kind(p))+0x68C),-32768,32768);
  double range=std::max(0,info.range+bonus),gap=distance-range;
  // The selected raw/native range query controls the in-range estimate. This
  // gap only limits acquisition; the synced Attack mission owns actual fire.
  if(gap>missilecompute::AdvanceLeash||distance<info.minimum)return e;
  int at=missileRecord(get<uint32_t>(p+0x10));
  if(at>=0){const auto&r=missileSends[at];int frame=get<int>(0xA8ED84);
   if(r.releasedAt>=0&&frame-r.releasedAt<missilecompute::ReleaseGraceFrames)return e;
   if(kind(p)!=3&&r.advanceStart>=0){double ax=double(get<int>(p+0x9C))-r.anchorX,ay=double(get<int>(p+0xA0))-r.anchorY;
    double tx=double(get<int>(q+0x9C))-r.anchorX,ty=double(get<int>(q+0xA0))-r.anchorY;
    if(frame-r.advanceStart>=missilecompute::MaxAdvanceFrames||ax*ax+ay*ay>=double(missilecompute::AdvanceLeash)*missilecompute::AdvanceLeash||
       std::sqrt(tx*tx+ty*ty)>range+missilecompute::AdvanceLeash)return e;
   }
  }
  const MissileMotion* motion=nullptr;for(const auto&m:missileMotion)if(m.id==get<uint32_t>(q+0x10)&&m.pointer==q){motion=&m;break;}
  if(motion&&motion->known){double closing=distance>0?-(dx*motion->vx+dy*motion->vy+dz*motion->vz)/distance:0;
   if(sh->missileReserve&&(closing<=.5||gap/closing>missilecompute::LookAhead))return e;
   if(kind(p)!=3&&closing<-.5)return e;
   eta=closing>.5?std::min(missilecompute::LookAhead,std::max(1,int(std::ceil(std::max(0.0,gap)/closing)))):missilecompute::LookAhead;
  }else eta=missilecompute::LookAhead;
 }
 e.damage=int(std::min(damage+0.999999,1000000.0));e.distance=uint32_t(std::min(distance*distance,double(UINT32_MAX)));e.eta=eta;e.close=close;return e;
}
