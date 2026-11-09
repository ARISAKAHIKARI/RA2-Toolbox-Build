sampleExternal(now);uint32_t configured=flags();
 bool needsBridge=debugPageActive||runtimeFeatureDemand(configured,extra,techMode,buildTimeTooltipEnabled)||(shared&&shared->minerDefenseActive)||(externalData&&externalData->status==1)||commandQueue.size||hasSelection||externalSampler.flashCount;
 if(needsBridge&&!shared)ensureBridge();
