#pragma once
// Signature retries survive temporary loading/platform conditions. There is
// no remote FrameInfo/CRC prerequisite for installing the existing hooks.
static DWORD signatureRetryAt;
static void resetHookReadiness(){checked=good=false;signatureRetryAt=0;}
