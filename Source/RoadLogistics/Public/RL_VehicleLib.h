#pragma once

#include "CoreMinimal.h"
#include "RL_VehicleLib.generated.h"

class AFGWheeledVehicle;
class AFGBuildableDockingStation;
class AFGDockingStationIdentifier;
class UFGItemDescriptor;
class UFGRecipe;

/** Result of a road route query between two path nodes */
USTRUCT(BlueprintType)
struct ROADLOGISTICS_API FRL_RoadRoute
{
	GENERATED_BODY()

	/** Path node GUIDs from start to end (inclusive) */
	UPROPERTY(BlueprintReadOnly, Category = "RoadLogistics")
	TArray<FGuid> Nodes;

	/** Total road length, in cm */
	UPROPERTY(BlueprintReadOnly, Category = "RoadLogistics")
	float DistanceCm = 0.0f;

	/** Energy the given vehicle type needs to drive the route, in MJ (MW*s), as computed by the vanilla path network */
	UPROPERTY(BlueprintReadOnly, Category = "RoadLogistics")
	float EnergyMJ = 0.0f;
};

/**
 * Thin, stateless helpers around the vanilla 1.2 vehicle path API.
 * All references to vanilla APIs are documented in docs/RESEARCH.md.
 */
UCLASS()
class ROADLOGISTICS_API URL_VehicleLib : public UObject
{
	GENERATED_BODY()
public:
	/** Vanilla truck class: /Game/FactoryGame/Buildable/Vehicle/Truck/BP_Truck */
	static TSubclassOf<AFGWheeledVehicle> GetDefaultTruckClass();
	/** Vanilla truck recipe (so dismantling a spawned truck refunds correctly) */
	static TSubclassOf<UFGRecipe> GetDefaultTruckRecipe();
	/** Coal, used as debug fuel */
	static TSubclassOf<UFGItemDescriptor> GetDebugFuelDescriptor();

	/** Path node GUID that a vehicle must use as a waypoint to dock at the station */
	static FGuid GetStationNodeGuid(const AFGDockingStationIdentifier* Station);

	/** Finds the road route between two path nodes for the given vehicle class. Returns false if unreachable / not on the same network */
	static bool FindRoadRoute(const UObject* WorldContext, const FGuid& FromNode, const FGuid& ToNode,
		TSubclassOf<AFGWheeledVehicle> VehicleClass, FRL_RoadRoute& OutRoute);

	/**
	 * Spawns a vehicle on the first path segment leaving the station docking node, so the autopilot considers it "on path".
	 * Server only. Returns nullptr on failure and fills OutError.
	 */
	static AFGWheeledVehicle* SpawnVehicleAtStation(AFGBuildableDockingStation* Station, TSubclassOf<AFGWheeledVehicle> VehicleClass,
		TSubclassOf<UFGItemDescriptor> FuelClass, int32 FuelAmount, FString& OutError);

	/** Cleanly removes a vehicle: undock, autopilot off, kick players, destroy. Server only. */
	static void DespawnVehicle(AFGWheeledVehicle* Vehicle);

	/** Sets the autopilot route (list of path node GUIDs) and enables the autopilot. Server only. */
	static bool SetRouteAndEnableAutopilot(AFGWheeledVehicle* Vehicle, const TArray<FGuid>& Route, FString& OutError);

	/** Human readable one-line state of a vehicle, for debug */
	static FString DescribeVehicle(const AFGWheeledVehicle* Vehicle);
	/** Human readable name of a station, for debug */
	static FString DescribeStation(const AFGDockingStationIdentifier* Station);
};
