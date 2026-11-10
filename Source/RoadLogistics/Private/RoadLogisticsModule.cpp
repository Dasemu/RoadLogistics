#include "RoadLogisticsModule.h"
#include "RL_Log.h"

DEFINE_LOG_CATEGORY(LogRoadLogistics);

void FRoadLogisticsModule::StartupModule()
{
	UE_LOG(LogRoadLogistics, Log, TEXT("RoadLogistics module started"));
}

void FRoadLogisticsModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FRoadLogisticsModule, RoadLogistics)
