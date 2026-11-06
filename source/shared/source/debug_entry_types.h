#pragma once
#include <stdint.h>
#include <stddef.h>
// Stable identities distinguish a callable API from a patch/DR at the same VA.
namespace debugentries {
constexpr uint32_t Capacity=256;
enum class Kind:uint32_t {Call=1,Patch=2,Breakpoint=3,System=4};
constexpr uint32_t id(Kind kind,uint32_t address){return (uint32_t(kind)<<24)|(address&0x00ffffffu);}
enum EntryFlags:uint32_t {Checkable=1,Disabled=2};
struct Entry {uint32_t id,address;Kind kind;const char*name;const char*group;const char*purpose;const char*features;uint32_t flags;};
struct Config {uint32_t generation=1,count=0,skip[Capacity]{};};
enum class Signature:uint32_t {Unknown,Match,Platform,Mismatch,Skipped,Unreadable};
enum class Hook:uint32_t {NA,Inactive,Pending,Installed,Conflict,Unavailable};
enum class Reason:uint32_t {None,Signature,Unreadable,Foreign,NoRequest,OwnerPending,Unsupported,Disabled,PathUnreachable,Structural,AwaitRuntime};
struct Observation {
 uint32_t id=0;Signature signature=Signature::Unknown;Hook hook=Hook::Inactive;
 Reason reason=Reason::AwaitRuntime;int32_t error=0;uint32_t tick=0,policyGeneration=0;
};
struct Snapshot {uint32_t policyGeneration=0,tick=0,count=0;Observation entries[Capacity]{};};
static_assert(sizeof(Config)==1032,"debug signature configuration ABI");
static_assert(sizeof(Observation)==28,"debug entry observation ABI");
static_assert(sizeof(Snapshot)==7180,"debug entry snapshot ABI");
}
