#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "RL_DebugCommand.generated.h"

class AFGWheeledVehicle;
class AFGDockingStationIdentifier;

/**
 * Debug-only chat command: /rl <subcommand> ...
 * Used in phase 1 to validate that we can drive vanilla trucks over the 1.2 vehicle path network from C++.
 * Chat commands execute on the server (SML AChatCommandSubsystem).
 */
UCLASS()
class ROADLOGISTICS_API ARL_DebugCommand : public AChatCommandInstance
{
	GENERATED_BODY()
public:
	ARL_DebugCommand();

	virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;

private:
	EExecutionStatus CmdStations(UCommandSender* Sender);
	EExecutionStatus CmdTrucks(UCommandSender* Sender);
	EExecutionStatus CmdStatus(UCommandSender* Sender, const TArray<FString>& Args);
	EExecutionStatus CmdGoto(UCommandSender* Sender, const TArray<FString>& Args);
	EExecutionStatus CmdStop(UCommandSender* Sender, const TArray<FString>& Args);
	EExecutionStatus CmdPath(UCommandSender* Sender, const TArray<FString>& Args);
	EExecutionStatus CmdSpawn(UCommandSender* Sender, const TArray<FString>& Args);
	EExecutionStatus CmdDespawn(UCommandSender* Sender, const TArray<FString>& Args);

	AFGWheeledVehicle* ResolveTruck(UCommandSender* Sender, const FString& Arg) const;
	AFGDockingStationIdentifier* ResolveStation(UCommandSender* Sender, const FString& Arg) const;
	void PrintHelp(UCommandSender* Sender) const;
};
