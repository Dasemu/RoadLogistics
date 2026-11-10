#pragma once

#include "CoreMinimal.h"
#include "FGSaveInterface.h"
#include "Subsystem/ModSubsystem.h"
#include "RL_TransportSubsystem.generated.h"

class AFGWheeledVehicle;

/** Last observed state of a watched vehicle (debug, phase 1) */
struct FRL_WatchedVehicleState
{
	FString LastDescription;
	TWeakObjectPtr<AActor> LastDockingStation;
};

/**
 * Server-side brain of RoadLogistics.
 * Phase 1: only a debug watcher that reports autopilot / docking state changes of selected vehicles.
 * Later phases add the fleet, orders and the dispatcher here (see docs/DESIGN.md).
 *
 * Note: named ARL_ (not URL_) because AModSubsystem is an AActor and UHT enforces the 'A' prefix.
 */
UCLASS()
class ROADLOGISTICS_API ARL_TransportSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()
public:
	ARL_TransportSubsystem();

	/** Returns the subsystem for the given world (nullptr on clients while ReplicationPolicy is SpawnOnServer) */
	static ARL_TransportSubsystem* Get(const UObject* WorldContext);

	// Begin AActor
	virtual void Tick(float DeltaSeconds) override;
	// End AActor

	// Begin IFGSaveInterface
	virtual bool ShouldSave_Implementation() const override { return true; }
	// End IFGSaveInterface

	/** Debug: report state changes of this vehicle to chat and log */
	void WatchVehicle(AFGWheeledVehicle* Vehicle);
	void UnwatchVehicle(AFGWheeledVehicle* Vehicle);

	/** Broadcasts a system chat message to all players (server only) */
	void BroadcastDebugMessage(const FString& Message) const;

private:
	TMap<TWeakObjectPtr<AFGWheeledVehicle>, FRL_WatchedVehicleState> WatchedVehicles;
};
