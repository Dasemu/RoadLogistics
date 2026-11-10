#include "RL_VehicleLib.h"
#include "RL_Log.h"

#include "Buildables/FGBuildableDockingStation.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "FGInventoryComponent.h"
#include "FGRecipe.h"
#include "Resources/FGItemDescriptor.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "WheeledVehicles/FGVehiclePathSegment.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"

namespace RL_VehicleLibPrivate
{
	static const TCHAR* TruckClassPath = TEXT("/Game/FactoryGame/Buildable/Vehicle/Truck/BP_Truck.BP_Truck_C");
	static const TCHAR* TruckRecipePath = TEXT("/Game/FactoryGame/Recipes/Vehicle/Recipe_Truck.Recipe_Truck_C");
	static const TCHAR* CoalDescPath = TEXT("/Game/FactoryGame/Resource/RawResources/Coal/Desc_Coal.Desc_Coal_C");

	/** Distance along the leaving segment where vehicles are spawned, in cm */
	static constexpr float SpawnDistanceAlongSegment = 800.0f;
	/** Small vertical offset so the chassis does not spawn inside the ground */
	static constexpr float SpawnHeightOffset = 25.0f;
}

TSubclassOf<AFGWheeledVehicle> URL_VehicleLib::GetDefaultTruckClass()
{
	return TSoftClassPtr<AFGWheeledVehicle>(FSoftObjectPath(RL_VehicleLibPrivate::TruckClassPath)).LoadSynchronous();
}

TSubclassOf<UFGRecipe> URL_VehicleLib::GetDefaultTruckRecipe()
{
	return TSoftClassPtr<UFGRecipe>(FSoftObjectPath(RL_VehicleLibPrivate::TruckRecipePath)).LoadSynchronous();
}

TSubclassOf<UFGItemDescriptor> URL_VehicleLib::GetDebugFuelDescriptor()
{
	return TSoftClassPtr<UFGItemDescriptor>(FSoftObjectPath(RL_VehicleLibPrivate::CoalDescPath)).LoadSynchronous();
}

FGuid URL_VehicleLib::GetStationNodeGuid(const AFGDockingStationIdentifier* Station)
{
	return Station ? Station->GetPathNodeGUID() : FGuid();
}

bool URL_VehicleLib::FindRoadRoute(const UObject* WorldContext, const FGuid& FromNode, const FGuid& ToNode,
	TSubclassOf<AFGWheeledVehicle> VehicleClass, FRL_RoadRoute& OutRoute)
{
	OutRoute = FRL_RoadRoute();

	AFGVehicleSubsystem* VehicleSubsystem = AFGVehicleSubsystem::Get(WorldContext);
	if (!VehicleSubsystem || !FromNode.IsValid() || !ToNode.IsValid() || !VehicleClass)
	{
		return false;
	}

	UFGVehiclePathNetwork* Network = VehicleSubsystem->FindNetworkByPathNodeGuid(FromNode);
	if (!Network || Network != VehicleSubsystem->FindNetworkByPathNodeGuid(ToNode))
	{
		return false;
	}

	const UFGVehiclePathPreset* Preset = AFGWheeledVehicle::GetPathPathPresetForVehicleType(VehicleClass);
	if (!Network->FindVehiclePath(FromNode, ToNode, Preset, OutRoute.Nodes) || OutRoute.Nodes.Num() == 0)
	{
		OutRoute.Nodes.Reset();
		return false;
	}

	for (int32 i = 1; i < OutRoute.Nodes.Num(); ++i)
	{
		const int32 A = Network->FindPathNodeIndexByGuid(OutRoute.Nodes[i - 1]);
		const int32 B = Network->FindPathNodeIndexByGuid(OutRoute.Nodes[i]);
		const int32 SegmentIndex = Network->FindPathIndexBetweenPathNodes(A, B);
		if (SegmentIndex == INDEX_NONE)
		{
			UE_LOG(LogRoadLogistics, Warning, TEXT("FindRoadRoute: no segment between consecutive route nodes %d -> %d"), A, B);
			continue;
		}
		const FVehiclePathNetworkSegmentData Segment = Network->GetPathSegmentAtIndex(SegmentIndex);
		OutRoute.DistanceCm += Segment.SegmentLength;
		OutRoute.EnergyMJ += Network->GetVehicleFuelConsumptionForSegment(Segment, VehicleClass);
	}
	return true;
}

AFGWheeledVehicle* URL_VehicleLib::SpawnVehicleAtStation(AFGBuildableDockingStation* Station, TSubclassOf<AFGWheeledVehicle> VehicleClass,
	TSubclassOf<UFGItemDescriptor> FuelClass, int32 FuelAmount, FString& OutError)
{
	using namespace RL_VehicleLibPrivate;

	if (!Station || !Station->HasAuthority())
	{
		OutError = TEXT("Invalid station or not running on the server");
		return nullptr;
	}
	if (!VehicleClass)
	{
		OutError = TEXT("Vehicle class could not be loaded");
		return nullptr;
	}

	AFGVehiclePathNode* DockingNode = Station->GetDockingPathNode();
	if (!DockingNode)
	{
		OutError = TEXT("Station has no docking path node");
		return nullptr;
	}

	const TArray<AFGVehiclePathSegment*>& Leaving = DockingNode->GetLeavingConnections();
	AFGVehiclePathSegment* Segment = Leaving.Num() > 0 ? Leaving[0] : nullptr;
	USplineComponent* Spline = Segment ? Segment->GetSplineComponent() : nullptr;
	if (!Spline)
	{
		OutError = TEXT("No vehicle path leaves the station docking node");
		return nullptr;
	}

	const float Distance = FMath::Min(SpawnDistanceAlongSegment, Spline->GetSplineLength() * 0.5f);
	const FVector Location = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) + FVector(0.0f, 0.0f, SpawnHeightOffset);
	const FRotator Rotation = Spline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	const FTransform SpawnTransform(Rotation, Location);

	UWorld* World = Station->GetWorld();
	AFGWheeledVehicle* Vehicle = World->SpawnActorDeferred<AFGWheeledVehicle>(VehicleClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Vehicle)
	{
		OutError = TEXT("SpawnActorDeferred failed");
		return nullptr;
	}

	if (TSubclassOf<UFGRecipe> Recipe = GetDefaultTruckRecipe())
	{
		Vehicle->SetBuiltWithRecipe(Recipe);
	}
	Vehicle->FinishSpawning(SpawnTransform);

	if (FuelClass && FuelAmount > 0)
	{
		if (UFGInventoryComponent* FuelInventory = Vehicle->GetFuelInventory())
		{
			const int32 Added = FuelInventory->AddStack(FInventoryStack(FuelAmount, FuelClass), true);
			UE_LOG(LogRoadLogistics, Log, TEXT("Spawn: added %d/%d fuel items"), Added, FuelAmount);
		}
	}

	if (!Vehicle->GetCurrentVehiclePathSegment())
	{
		Vehicle->UpdateCurrentVehiclePathSegmentFromVehicleLocation();
	}

	UE_LOG(LogRoadLogistics, Log, TEXT("Spawned %s at %s on segment %s (expected %s)"),
		*GetNameSafe(Vehicle), *Location.ToString(), *GetNameSafe(Vehicle->GetCurrentVehiclePathSegment()), *GetNameSafe(Segment));
	return Vehicle;
}

void URL_VehicleLib::DespawnVehicle(AFGWheeledVehicle* Vehicle)
{
	if (!Vehicle || !Vehicle->HasAuthority())
	{
		return;
	}

	if (AFGBuildableDockingStation* Station = Cast<AFGBuildableDockingStation>(Vehicle->GetCurrentlyDockingStationActor()))
	{
		if (Station->GetDockedActor() == Vehicle)
		{
			Station->ForceUndockActor();
		}
	}
	if (AFGWheeledVehicleIdentifier* Identifier = Vehicle->GetVehicleIdentifier())
	{
		Identifier->SetAutopilotEnabled(false);
	}
	Vehicle->KickAllPlayers();

	UE_LOG(LogRoadLogistics, Log, TEXT("Despawning %s"), *GetNameSafe(Vehicle));
	Vehicle->Destroy();
}

bool URL_VehicleLib::SetRouteAndEnableAutopilot(AFGWheeledVehicle* Vehicle, const TArray<FGuid>& Route, FString& OutError)
{
	if (!Vehicle || !Vehicle->HasAuthority())
	{
		OutError = TEXT("Invalid vehicle or not running on the server");
		return false;
	}
	AFGWheeledVehicleIdentifier* Identifier = Vehicle->GetVehicleIdentifier();
	if (!Identifier)
	{
		OutError = TEXT("Vehicle has no identifier (not a 1.2 wheeled vehicle?)");
		return false;
	}

	Identifier->SetVehicleRoute(Route);
	if (!Identifier->CanEnableAutopilot())
	{
		// Not fatal: we still try, and the error status tells us why it refuses
		OutError = TEXT("CanEnableAutopilot() returned false; enabling anyway to read the error status");
	}
	Identifier->SetAutopilotEnabled(true);
	return true;
}

FString URL_VehicleLib::DescribeVehicle(const AFGWheeledVehicle* Vehicle)
{
	if (!Vehicle)
	{
		return TEXT("<null>");
	}
	const AFGWheeledVehicleIdentifier* Id = Vehicle->GetVehicleIdentifier();
	const FString Name = Id ? Id->GetVehicleName().ToString() : FString();
	const FString Status = Id ? StaticEnum<EVehicleAutopilotErrorStatus>()->GetNameStringByValue(static_cast<int64>(Id->GetAutopilotErrorStatus())) : TEXT("?");
	const UFGInventoryComponent* Fuel = Vehicle->GetFuelInventory();
	int32 FuelItems = 0;
	if (Fuel)
	{
		TArray<FInventoryStack> Stacks;
		Fuel->GetInventoryStacks(Stacks);
		for (const FInventoryStack& Stack : Stacks)
		{
			FuelItems += Stack.NumItems;
		}
	}
	return FString::Printf(TEXT("'%s' [%s] autopilot=%s status=%s docking=%s wp=%d/%d fuel=%d onPath=%s"),
		*Name, *GetNameSafe(Vehicle->GetClass()),
		(Id && Id->IsAutopilotEnabled()) ? TEXT("on") : TEXT("off"),
		*Status,
		(Id && Id->IsCurrentlyDocking()) ? TEXT("yes") : TEXT("no"),
		Id ? Id->GetCurrentTargetWaypointIndex() : -1,
		Id ? Id->GetVehicleRoute().Num() : 0,
		FuelItems,
		Vehicle->GetCurrentVehiclePathSegment() ? TEXT("yes") : TEXT("no"));
}

FString URL_VehicleLib::DescribeStation(const AFGDockingStationIdentifier* Station)
{
	if (!Station)
	{
		return TEXT("<null>");
	}
	const AFGBuildableDockingStation* Building = Station->GetStation();
	const FString Where = Building ? Building->GetActorLocation().ToCompactString() : FString(TEXT("?"));
	return FString::Printf(TEXT("'%s' at %s"), *Station->GetStationName().ToString(), *Where);
}
