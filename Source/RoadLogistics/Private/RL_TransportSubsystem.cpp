#include "RL_TransportSubsystem.h"
#include "RL_Log.h"
#include "RL_VehicleLib.h"

#include "Engine/World.h"
#include "FGChatManager.h"
#include "Subsystem/SubsystemActorManager.h"
#include "WheeledVehicles/FGWheeledVehicle.h"

ARL_TransportSubsystem::ARL_TransportSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval = 1.0f;
}

ARL_TransportSubsystem* ARL_TransportSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	USubsystemActorManager* Manager = World ? World->GetSubsystem<USubsystemActorManager>() : nullptr;
	return Manager ? Manager->GetSubsystemActor<ARL_TransportSubsystem>() : nullptr;
}

void ARL_TransportSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}

	for (auto It = WatchedVehicles.CreateIterator(); It; ++It)
	{
		AFGWheeledVehicle* Vehicle = It->Key.Get();
		if (!Vehicle)
		{
			BroadcastDebugMessage(TEXT("[RL] watched vehicle no longer exists"));
			It.RemoveCurrent();
			continue;
		}

		FRL_WatchedVehicleState& State = It->Value;
		AActor* DockingStation = Vehicle->GetCurrentlyDockingStationActor();
		if (DockingStation != State.LastDockingStation.Get())
		{
			BroadcastDebugMessage(FString::Printf(TEXT("[RL] %s %s %s"), *GetNameSafe(Vehicle),
				DockingStation ? TEXT("DOCKED at") : TEXT("UNDOCKED from"),
				*GetNameSafe(DockingStation ? DockingStation : State.LastDockingStation.Get())));
			State.LastDockingStation = DockingStation;
		}

		const FString Description = URL_VehicleLib::DescribeVehicle(Vehicle);
		if (Description != State.LastDescription)
		{
			UE_LOG(LogRoadLogistics, Log, TEXT("[watch] %s: %s"), *GetNameSafe(Vehicle), *Description);
			BroadcastDebugMessage(FString::Printf(TEXT("[RL] %s"), *Description));
			State.LastDescription = Description;
		}
	}
}

void ARL_TransportSubsystem::WatchVehicle(AFGWheeledVehicle* Vehicle)
{
	if (Vehicle)
	{
		WatchedVehicles.FindOrAdd(Vehicle);
	}
}

void ARL_TransportSubsystem::UnwatchVehicle(AFGWheeledVehicle* Vehicle)
{
	WatchedVehicles.Remove(Vehicle);
}

void ARL_TransportSubsystem::BroadcastDebugMessage(const FString& Message) const
{
	UE_LOG(LogRoadLogistics, Log, TEXT("%s"), *Message);
	if (AFGChatManager* ChatManager = AFGChatManager::Get(GetWorld()))
	{
		FChatMessageStruct ChatMessage;
		ChatMessage.MessageText = FText::FromString(Message);
		ChatMessage.MessageType = EFGChatMessageType::CMT_SystemMessage;
		ChatManager->BroadcastChatMessage(ChatMessage);
	}
}
