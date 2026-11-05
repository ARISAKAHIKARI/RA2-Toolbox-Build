bool tile(uint32_t type,unsigned sub,const std::array<uint32_t,256>&pal,external::voxel::Image&out){
 const uint32_t resource=word(type+0xA4);const SharedTileKey key{process_,match_,type,resource,sub,files.revision(),pal};
 if(process_){std::shared_ptr<const external::voxel::Image> cached;
  {std::lock_guard<std::mutex>lock(sharedTileMutex_);auto found=sharedTiles_.find(key);if(found!=sharedTiles_.end())cached=found->second;}
  if(cached){out=*cached;return true;}
 }
 auto retain=[&]{if(process_){
  auto cost=[](auto&&self,const external::voxel::Image&i)->size_t{size_t n=4*(i.pixels.size()+i.depth.size())+i.indices.size();for(const auto&a:i.attachments)n+=self(self,a);return n;};
  const size_t bytes=cost(cost,out);if(bytes<=rendermemory::SharedTiles){auto image=std::make_shared<const external::voxel::Image>(out);std::lock_guard<std::mutex>lock(sharedTileMutex_);
   if(!sharedTiles_.count(key)){if(sharedTileBytes_+bytes>rendermemory::SharedTiles){sharedTiles_.clear();sharedTileBytes_=0;}sharedTiles_[key]=std::move(image);sharedTileBytes_+=bytes;}}
 }return true;};
 std::vector<uint8_t> bytes;auto filename=text(type+0x2F5,14);std::string reason;
 if(!filename.empty()&&files.load(filename,bytes)&&external::assets::tmp(std::move(bytes),sub,pal,out,&reason))return retain();
 if(external::terrain::tile(read,resource,sub,pal,out,&reason)){failure.clear();return retain();}
 failure="tile-not-found:"+filename+" memory="+reason;return false;
}
