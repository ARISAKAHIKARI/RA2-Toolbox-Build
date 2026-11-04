#pragma once
// One Runtime-owned CCToolTip slot serves both cursor presentation and the
// optional build-time suffix. Call setters under the Assist execution lease.
extern "C" bool NativeTooltipBuildTime(bool wanted);
extern "C" void NativeTooltipMaintain(bool allowInstall);
extern "C" bool NativeTooltipDetached();
