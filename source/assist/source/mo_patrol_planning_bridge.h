#pragma once
#include "native_planning_input_bridge.h"
using MoPlanningUI=NativePlanningInput;
static MoPlanningUI moPlanningUI;
static void moPlanningReset(){moPlanningUI.reset();}
static bool moPlanningCapture(MSG*msg){return moPlanningUI.capture(msg);}
static bool moPlanningMessage(MSG*msg){return moPlanningUI.message(msg,moHolding||moStage);}
static bool moPlanningFinish(uint32_t house){return moPlanningUI.finish(house);}
