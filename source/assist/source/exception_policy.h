#pragma once

// This policy is shared by the real x86 SEH filter and portable verification.
// A dispatcher fault must remain visible to the caller/OS. Only local feature
// boundaries with a defined access-failure return may consume AV/in-page faults.
namespace assistguard {
enum class Disposition {Search,ReportSearch,ReportHandle};
inline constexpr bool globalBoundary(unsigned boundary){return boundary==1||boundary==10||boundary==11;}
inline constexpr bool localBoundary(unsigned boundary){
 return boundary==9||boundary==12||boundary==13||boundary==14||boundary==17||
        boundary==18||boundary==19||boundary==21||boundary==22;
}
// Only NONCONTINUABLE is a fault attribute. Unwind, invalid-stack, nested
// dispatch and unknown control flags must never select local recovery.
inline constexpr bool debugOrControl(unsigned code,unsigned flags){
 return (flags&~1u)!=0||code==0x80000003u||code==0x80000004u||code==0x80000007u||
        (code&0xffff0000u)==0x40010000u||code==0x80010001u||code==0x406d1388u||
        code==0x80000026u||code==0x80000029u||code==0xc0000027u;
}
inline constexpr bool accessFault(unsigned code){return code==0xc0000005u||code==0xc0000006u;}
inline constexpr bool localAccessFailure(unsigned code,unsigned flags){return !debugOrControl(code,flags)&&accessFault(code);}
inline constexpr bool runtimeFault(unsigned code){
 // Explicit native faults only. Language exceptions and unknown software
 // exceptions retain their own exception and unwind handlers.
 return accessFault(code)||code==0xc000001du||code==0xc0000025u||code==0xc0000028u||
        (code>=0xc000008cu&&code<=0xc0000096u)||code==0xc00000fdu||
        code==0xc0000374u||code==0xc0000409u;
}
inline constexpr Disposition disposition(unsigned boundary,unsigned code,unsigned flags){
 if(debugOrControl(code,flags)||!runtimeFault(code))return Disposition::Search;
 if(globalBoundary(boundary))return Disposition::ReportSearch;
 return localBoundary(boundary)&&localAccessFailure(code,flags)?Disposition::ReportHandle:Disposition::Search;
}
// Native x86 EXCEPTION_RECORD/EXCEPTION_POINTERS prefix. No Windows headers:
// these guards are compiled with the MSVC SEH ABI and no standard includes.
struct Record {unsigned code,flags;void* nested;void* address;unsigned count,info[15];};
struct Pointers {Record* record;void* context;};
inline int noReportAccess(void* raw){
 const auto* pointers=(const Pointers*)raw;const auto* record=pointers?pointers->record:nullptr;
 return record&&localAccessFailure(record->code,record->flags)?1:0;
}

}
