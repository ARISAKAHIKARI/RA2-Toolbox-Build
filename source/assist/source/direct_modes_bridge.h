// All code writes and restoration remain owned by GameEntryManager.
static bool directGroupRestore(directpatch::Group group){
 bool ok=true;for(const auto&s:directpatch::Sites)if(s.group==group&&codeHookRegistry.active(s.address))
  if(!gameentry::patch(gameentry::Client::Assist,s.address,s.after,s.before,s.size))ok=false;
 return ok;
}
static bool directGroupEnable(directpatch::Group group){
 // Preflight the whole group before writing; an incompatible member rolls back
 // only our own installed sites. Foreign and stale bytes are never repaired.
 bool ok=true;
 for(const auto&s:directpatch::Sites)if(s.group==group){
  uint8_t actual[6]{};const bool active=codeHookRegistry.active(s.address);
  if(codeHookRegistry.conflict(s.address)||!patchcontext::compatible(s.address,[](uint32_t a,void*p,size_t n){return read(a,p,n);})||
     !read(s.address,actual,s.size)||(active?memcmp(actual,s.after,s.size):!hookregistry::admissibleOriginal(s.address,actual,s.size)))ok=false;
 }
 if(ok)for(const auto&s:directpatch::Sites)if(s.group==group&&!codeHookRegistry.active(s.address))
  if(!gameentry::patch(gameentry::Client::Assist,s.address,s.before,s.after,s.size)){ok=false;break;}
 if(!ok)directGroupRestore(group);
 return ok;
}
