#include "RL_GameWorldModule.h"
#include "RL_TransportSubsystem.h"
#include "Debug/RL_DebugCommand.h"

URL_GameWorldModule::URL_GameWorldModule()
{
	bRootModule = true;
	ModSubsystems.Add(ARL_TransportSubsystem::StaticClass());
	mChatCommands.Add(ARL_DebugCommand::StaticClass());
}
