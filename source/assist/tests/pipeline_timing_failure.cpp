#ifndef PIPELINE_TIMING_HEADER
#define PIPELINE_TIMING_HEADER "../../shared/source/pipeline_timing.h"
#endif
#include PIPELINE_TIMING_HEADER
#include <atomic>
#include <cassert>
#include <exception>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>
using namespace std::chrono_literals;
static unsigned attempted=0,unknownCalls=0,unwindCalls=0;
static std::wstring recovered;
static void unavailable(const std::wstring&){++attempted;throw std::bad_alloc();}
static void throwing(const std::wstring&){++unwindCalls;throw std::runtime_error("logger");}
static void nonstandard(const std::wstring&){++unknownCalls;throw 17;}
static void collect(const std::wstring&s){recovered=s;}
int main(){
 std::set_terminate([]{std::_Exit(86);});
 pipelineperf::Window normal(L"normal",{L"draw"}),unwind(L"unwind",{L"draw"}),unknown(L"unknown",{L"draw"});
 std::this_thread::sleep_for(5100ms);
 {pipelineperf::Scope scope(normal,0,unavailable);}assert(attempted==1);
 bool original=false;try{pipelineperf::Scope scope(unwind,0,throwing);throw std::runtime_error("original-render-error");}catch(const std::runtime_error&e){original=std::string(e.what())=="original-render-error";}
 assert(original&&unwindCalls==1);
 {pipelineperf::Scope scope(unknown,0,nonstandard);scope.finish();scope.finish();}assert(unknownCalls==1);
 for(int i=0;i<10000;++i)normal.report(unavailable);assert(attempted==1);
 normal.add(0,7.0);std::this_thread::sleep_for(5100ms);normal.report(collect);
 assert(recovered.find(L"draw[n=2,")!=std::wstring::npos&&recovered.find(L"log_failures=1")!=std::wstring::npos);
 normal.report(collect);assert(attempted==1);
 std::cout<<"PASS production timing failure boundary: allocation/logger/nonstandard failures, original unwind preserved, idempotent finish, bounded retry and retained aggregates\n";
}
