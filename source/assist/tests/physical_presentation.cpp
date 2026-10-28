#include "../../shared/source/physical_presentation.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace physicalpresentation;
int main(){
 Observation o;
 o.sourceW=2560;o.sourceH=1440;o.clientW=3840;o.clientH=2160;
 o.monitorW=o.currentW=o.registryW=3840;
 o.monitorH=o.currentH=o.registryH=2160;
 o.coversMonitor=true;
 auto r=classify(o);
 assert(r.status==Status::Ready&&r.permitted());
 assert(r.width==3840&&r.height==2160);
 assert(std::abs(r.scaleX-1.5)<1e-9&&std::abs(r.scaleY-1.5)<1e-9);
 // 4K hardware with only a 1440p client cannot truthfully call its
 // 2K-sized overlay a physical 4K image.
 o.clientW=2560;o.clientH=1440;o.coversMonitor=false;
 assert(classify(o).status==Status::WindowOnly);
 // Game is 4K already. Do not invent new pixels.
 o.clientW=3840;o.clientH=2160;o.coversMonitor=true;
 o.sourceW=3840;o.sourceH=2160;
 assert(classify(o).status==Status::NativeResolution);
 // Classic exclusive flip: Windows reports a current game-mode size that
 // differs from the configured desktop 4K size, while game covers it.
 o.sourceW=2560;o.sourceH=1440;
 o.clientW=o.currentW=o.monitorW=2560;
 o.clientH=o.currentH=o.monitorH=1440;
 assert(classify(o).status==Status::SuspiciousModeSwitch);
 assert(!classify(o).permitted());
 // Avoid false "exclusive confirmed" claims: configured-mode mismatch may
 // have unrelated explanations. Only classified as suspicious.
 o.registryW=2560;o.registryH=1440;
 assert(classify(o).status==Status::NativeResolution);
 // System-DPI virtualization vs physical device pixels: fail closed.
 o.monitorW=3840;o.monitorH=2160;
 assert(classify(o).status==Status::AmbiguousDpi);
 o.monitorW=2560;o.monitorH=1440;
 // Source video aspect differs from physical output; cannot safely position
 // world-space markers/cursor without a known letterbox transform.
 o.sourceW=1920;o.sourceH=1080;
 o.clientW=2560;o.clientH=1200;
 assert(classify(o).status==Status::WindowOnly);
 o.coversMonitor=true;o.monitorH=o.currentH=1200;
 assert(classify(o).status==Status::UnsupportedRatio);
 o.sourceW=0;
 assert(classify(o).status==Status::InvalidGeometry);
 o.sourceW=1920;o.sourceH=1080;
 o.clientW=8192;o.clientH=8192;o.monitorW=o.currentW=8192;
 o.monitorH=o.currentH=8192;
 assert(classify(o).status==Status::InvalidGeometry);
 o.captureFailed=true;
 assert(classify(o).status==Status::InvalidGeometry);
 std::puts("PASS: 4K physical 1.5x, window-only fallback, native resolution, suspected exclusive switch, DPI ambiguity, asymmetric viewport and bounds");
}
