#pragma once
#include "compute_tasks.h"
#include "missile_compute.h"
#include "unload_compute.h"
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace assist { namespace compute {
// Shared inputs keep their fixed ABI. Only the counted portions are observed
// by calculation and source revalidation. Counts are copied first and checked
// locally before they can control any length; the surrounding publication
// sequence still rejects a concurrent producer change.
template<class Input>struct InputCopy {
 static void copy(Input&to,const Input&from){
  static_assert(std::is_trivially_copyable<Input>::value,"Input snapshots must be values");
  std::memcpy(&to,&from,sizeof(Input));
 }
};
template<>struct InputCopy<unloadcompute::Input> {
 static void copy(unloadcompute::Input&to,const unloadcompute::Input&from){
  std::memcpy(&to,&from,offsetof(unloadcompute::Input,sources));
  if(!unloadcompute::valid(to)){std::memcpy(&to,&from,sizeof(to));return;}
  std::memcpy(to.sources,from.sources,size_t(to.count)*sizeof(to.sources[0]));
  std::memcpy(to.targets,from.targets,size_t(to.targetCount)*sizeof(to.targets[0]));
  for(int n=0;n<to.count;++n)std::memcpy(to.actions[n],from.actions[n],size_t(to.targetCount));
 }
};
template<>struct InputCopy<missilecompute::Input> {
 static void copy(missilecompute::Input&to,const missilecompute::Input&from){
  std::memcpy(&to,&from,offsetof(missilecompute::Input,sources));
  if(to.count<0||to.count>missilecompute::MaxSources||to.targetCount<0||to.targetCount>missilecompute::MaxTargets){std::memcpy(&to,&from,sizeof(to));return;}
  std::memcpy(to.sources,from.sources,size_t(to.count)*sizeof(to.sources[0]));
  std::memcpy(to.targets,from.targets,size_t(to.targetCount)*sizeof(to.targets[0]));
  for(int n=0;n<to.count;++n)std::memcpy(to.edges[n],from.edges[n],size_t(to.targetCount)*sizeof(to.edges[n][0]));
 }
};
template<>struct InputCopy<LoadInput> {
 static void copy(LoadInput&to,const LoadInput&from){
  std::memcpy(&to,&from,offsetof(LoadInput,soldiers));
  if(!valid(to)){std::memcpy(&to,&from,sizeof(to));return;}
  std::memcpy(to.soldiers,from.soldiers,size_t(to.ns)*sizeof(to.soldiers[0]));
  std::memcpy(to.vehicles,from.vehicles,size_t(to.nv)*sizeof(to.vehicles[0]));
  std::memcpy(to.tokens,from.tokens,size_t(to.ns)*sizeof(to.tokens[0]));
  std::memcpy(&to.composition,&from.composition,offsetof(loading::Composition,numbers));
  // assign() mutates vehicles and can append previously absent source types.
  // Its new type indices read these row tails, so preserve the entire row for
  // every active vehicle, including zeros beyond composition.count.
  for(int n=0;n<to.nv;++n)std::memcpy(to.composition.numbers[n],from.composition.numbers[n],sizeof(to.composition.numbers[n]));
  for(int n=0;n<to.ns;++n)std::memcpy(to.compatible[n],from.compatible[n],size_t(to.nv));
 }
};
template<>struct InputCopy<LoadObservation> {
 static void copy(LoadObservation&to,const LoadObservation&from){
  std::memcpy(&to,&from,offsetof(LoadObservation,soldiers));
  if(!valid(to)){std::memcpy(&to,&from,sizeof(to));return;}
  std::memcpy(to.soldiers,from.soldiers,size_t(to.ns)*sizeof(to.soldiers[0]));
  std::memcpy(to.vehicles,from.vehicles,size_t(to.nv)*sizeof(to.vehicles[0]));
  std::memcpy(to.tokens,from.tokens,size_t(to.ns)*sizeof(to.tokens[0]));
 }
};
template<>struct InputCopy<missilecompute::Observation> {
 static void copy(missilecompute::Observation&to,const missilecompute::Observation&from){
  std::memcpy(&to,&from,offsetof(missilecompute::Observation,sources));
  if(to.count<0||to.count>missilecompute::MaxSources||to.targetCount<0||to.targetCount>missilecompute::MaxTargets){std::memcpy(&to,&from,sizeof(to));return;}
  std::memcpy(to.sources,from.sources,size_t(to.count)*sizeof(to.sources[0]));
  std::memcpy(to.targets,from.targets,size_t(to.targetCount)*sizeof(to.targets[0]));
 }
};
template<class Input>inline void copyInput(Input&to,const Input&from){
 if(&to!=&from)InputCopy<Input>::copy(to,from);
}
}}
