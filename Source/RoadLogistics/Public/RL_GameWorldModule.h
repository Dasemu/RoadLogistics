#pragma once

#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "RL_GameWorldModule.generated.h"

/**
 * Root game world module of RoadLogistics.
 * Native root modules are discovered by SML (Mods/SML/Source/SML/Private/ModLoading/PluginModuleLoader.cpp, FindRootModulesOfType),
 * so no Blueprint asset is required. Everything is registered from the constructor.
 */
UCLASS()
class ROADLOGISTICS_API URL_GameWorldModule : public UGameWorldModule
{
	GENERATED_BODY()
public:
	URL_GameWorldModule();
};
