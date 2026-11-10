#include "Debug/RL_DebugCommand.h"
#include "RL_Log.h"
#include "RL_TransportSubsystem.h"
#include "RL_VehicleLib.h"

#include "Buildables/FGBuildableDockingStation.h"
#include "Command/CommandSender.h"
#include "Resources/FGItemDescriptor.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"

ARL_DebugCommand::ARL_DebugCommand()
{
	CommandName = TEXT("rl");
	MinNumberOfArguments = 0;
	bOnlyUsableByPlayer = false;
	Usage = NSLOCTEXT("RoadLogistics", "DebugCommand.Usage",
		"/rl stations | trucks | status <truck> | goto <truck> <station> [station...] | stop <truck> | path <stationA> <stationB> | spawn <station> [fuel] | despawn <truck>");
}

EExecutionStatus ARL_DebugCommand::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
	if (!HasAuthority())
	{
		Sender->SendChatMessage(TEXT("RoadLogistics debug commands only run on the server"), FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}
	if (Arguments.Num() == 0)
	{
		PrintHelp(Sender);
		return EExecutionStatus::COMPLETED;
	}

	const FString Sub = Arguments[0].ToLower();
	TArray<FString> Rest(Arguments);
	Rest.RemoveAt(0);

	if (Sub == TEXT("stations")) return CmdStations(Sender);
	if (Sub == TEXT("trucks"))   return CmdTrucks(Sender);
	if (Sub == TEXT("status"))   return CmdStatus(Sender, Rest);
	if (Sub == TEXT("goto"))     return CmdGoto(Sender, Rest);
	if (Sub == TEXT("stop"))     return CmdStop(Sender, Rest);
	if (Sub == TEXT("path"))     return CmdPath(Sender, Rest);
	if (Sub == TEXT("spawn"))    return CmdSpawn(Sender, Rest);
	if (Sub == TEXT("despawn"))  return CmdDespawn(Sender, Rest);

	PrintHelp(Sender);
	return EExecutionStatus::BAD_ARGUMENTS;
}

void ARL_DebugCommand::PrintHelp(UCommandSender* Sender) const
{
	Sender->SendChatMessage(Usage.ToString());
}

AFGWheeledVehicle* ARL_DebugCommand::ResolveTruck(UCommandSender* Sender, const FString& Arg) const
{
	const AFGVehicleSubsystem* VehicleSubsystem = AFGVehicleSubsystem::Get(this);
	if (!VehicleSubsystem || !Arg.IsNumeric())
	{
		Sender->SendChatMessage(TEXT("Expected a truck index, see /rl trucks"), FLinearColor::Red);
		return nullptr;
	}
	const int32 Index = FCString::Atoi(*Arg);
	const TArray<AFGWheeledVehicleIdentifier*>& Vehicles = VehicleSubsystem->GetAllVehicles();
	if (!Vehicles.IsValidIndex(Index) || !Vehicles[Index] || !Vehicles[Index]->GetOwnerVehicle())
	{
		Sender->SendChatMessage(FString::Printf(TEXT("No truck with index %d"), Index), FLinearColor::Red);
		return nullptr;
	}
	return Vehicles[Index]->GetOwnerVehicle();
}

AFGDockingStationIdentifier* ARL_DebugCommand::ResolveStation(UCommandSender* Sender, const FString& Arg) const
{
	const AFGVehicleSubsystem* VehicleSubsystem = AFGVehicleSubsystem::Get(this);
	if (!VehicleSubsystem || !Arg.IsNumeric())
	{
		Sender->SendChatMessage(TEXT("Expected a station index, see /rl stations"), FLinearColor::Red);
		return nullptr;
	}
	const int32 Index = FCString::Atoi(*Arg);
	const TArray<AFGDockingStationIdentifier*>& Stations = VehicleSubsystem->GetDockingStations();
	if (!Stations.IsValidIndex(Index) || !Stations[Index])
	{
		Sender->SendChatMessage(FString::Printf(TEXT("No station with index %d"), Index), FLinearColor::Red);
		return nullptr;
	}
	return Stations[Index];
}

EExecutionStatus ARL_DebugCommand::CmdStations(UCommandSender* Sender)
{
	const AFGVehicleSubsystem* VehicleSubsystem = AFGVehicleSubsystem::Get(this);
	if (!VehicleSubsystem) return EExecutionStatus::UNCOMPLETED;

	const TArray<AFGDockingStationIdentifier*>& Stations = VehicleSubsystem->GetDockingStations();
	Sender->SendChatMessage(FString::Printf(TEXT("%d docking stations:"), Stations.Num()));
	for (int32 i = 0; i < Stations.Num(); ++i)
	{
		const UFGVehiclePathNetwork* Network = VehicleSubsystem->FindNetworkByPathNodeGuid(URL_VehicleLib::GetStationNodeGuid(Stations[i]));
		Sender->SendChatMessage(FString::Printf(TEXT("  [%d] %s network=%d"), i, *URL_VehicleLib::DescribeStation(Stations[i]),
			Network ? Network->GetNetworkID() : -1));
	}
	return EExecutionStatus::COMPLETED;
}

EExecutionStatus ARL_DebugCommand::CmdTrucks(UCommandSender* Sender)
{
	const AFGVehicleSubsystem* VehicleSubsystem = AFGVehicleSubsystem::Get(this);
	if (!VehicleSubsystem) return EExecutionStatus::UNCOMPLETED;

	const TArray<AFGWheeledVehicleIdentifier*>& Vehicles = VehicleSubsystem->GetAllVehicles();
	Sender->SendChatMessage(FString::Printf(TEXT("%d wheeled vehicles:"), Vehicles.Num()));
	for (int32 i = 0; i < Vehicles.Num(); ++i)
	{
		const AFGWheeledVehicle* Vehicle = Vehicles[i] ? Vehicles[i]->GetOwnerVehicle() : nullptr;
		Sender->SendChatMessage(FString::Printf(TEXT("  [%d] %s"), i, *URL_VehicleLib::DescribeVehicle(Vehicle)));
	}
	return EExecutionStatus::COMPLETED;
}

EExecutionStatus ARL_DebugCommand::CmdStatus(UCommandSender* Sender, const TArray<FString>& Args)
{
	if (Args.Num() < 1) { PrintHelp(Sender); return EExecutionStatus::BAD_ARGUMENTS; }
	AFGWheeledVehicle* Vehicle = ResolveTruck(Sender, Args[0]);
	if (!Vehicle) return EExecutionStatus::BAD_ARGUMENTS;

	Sender->SendChatMessage(URL_VehicleLib::DescribeVehicle(Vehicle));
	if (const AFGWheeledVehicleIdentifier* Id = Vehicle->GetVehicleIdentifier())
	{
		const AFGVehicleSubsystem* VehicleSubsystem = AFGVehicleSubsystem::Get(this);
		const TArray<FGuid>& Route = Id->GetVehicleRoute();
		for (int32 i = 0; i < Route.Num(); ++i)
		{
			const AFGDockingStationIdentifier* Station = VehicleSubsystem ? VehicleSubsystem->FindDockingStationIdentifierForPathNodeGuid(Route[i]) : nullptr;
			Sender->SendChatMessage(FString::Printf(TEXT("  wp[%d] %s %s"), i, *Route[i].ToString(),
				Station ? *URL_VehicleLib::DescribeStation(Station) : TEXT("(not a station)")));
		}
	}
	return EExecutionStatus::COMPLETED;
}

EExecutionStatus ARL_DebugCommand::CmdGoto(UCommandSender* Sender, const TArray<FString>& Args)
{
	if (Args.Num() < 2) { PrintHelp(Sender); return EExecutionStatus::BAD_ARGUMENTS; }
	AFGWheeledVehicle* Vehicle = ResolveTruck(Sender, Args[0]);
	if (!Vehicle) return EExecutionStatus::BAD_ARGUMENTS;

	TArray<FGuid> Route;
	for (int32 i = 1; i < Args.Num(); ++i)
	{
		const AFGDockingStationIdentifier* Station = ResolveStation(Sender, Args[i]);
		if (!Station) return EExecutionStatus::BAD_ARGUMENTS;
		Route.Add(URL_VehicleLib::GetStationNodeGuid(Station));
	}

	FString Error;
	if (!URL_VehicleLib::SetRouteAndEnableAutopilot(Vehicle, Route, Error))
	{
		Sender->SendChatMessage(Error, FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}
	if (!Error.IsEmpty())
	{
		Sender->SendChatMessage(Error, FLinearColor::Yellow);
	}
	if (ARL_TransportSubsystem* Subsystem = ARL_TransportSubsystem::Get(this))
	{
		Subsystem->WatchVehicle(Vehicle);
	}
	Sender->SendChatMessage(FString::Printf(TEXT("Route with %d waypoint(s) set. Watching truck."), Route.Num()));
	return EExecutionStatus::COMPLETED;
}

EExecutionStatus ARL_DebugCommand::CmdStop(UCommandSender* Sender, const TArray<FString>& Args)
{
	if (Args.Num() < 1) { PrintHelp(Sender); return EExecutionStatus::BAD_ARGUMENTS; }
	AFGWheeledVehicle* Vehicle = ResolveTruck(Sender, Args[0]);
	if (!Vehicle) return EExecutionStatus::BAD_ARGUMENTS;

	if (AFGWheeledVehicleIdentifier* Id = Vehicle->GetVehicleIdentifier())
	{
		Id->SetAutopilotEnabled(false);
	}
	if (ARL_TransportSubsystem* Subsystem = ARL_TransportSubsystem::Get(this))
	{
		Subsystem->UnwatchVehicle(Vehicle);
	}
	Sender->SendChatMessage(TEXT("Autopilot disabled."));
	return EExecutionStatus::COMPLETED;
}

EExecutionStatus ARL_DebugCommand::CmdPath(UCommandSender* Sender, const TArray<FString>& Args)
{
	if (Args.Num() < 2) { PrintHelp(Sender); return EExecutionStatus::BAD_ARGUMENTS; }
	const AFGDockingStationIdentifier* From = ResolveStation(Sender, Args[0]);
	const AFGDockingStationIdentifier* To = ResolveStation(Sender, Args[1]);
	if (!From || !To) return EExecutionStatus::BAD_ARGUMENTS;

	const TSubclassOf<AFGWheeledVehicle> TruckClass = URL_VehicleLib::GetDefaultTruckClass();
	FRL_RoadRoute Route;
	if (!URL_VehicleLib::FindRoadRoute(this, URL_VehicleLib::GetStationNodeGuid(From), URL_VehicleLib::GetStationNodeGuid(To), TruckClass, Route))
	{
		Sender->SendChatMessage(TEXT("No road route found for the truck between those stations"), FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}

	const float MaxSpeed = AFGWheeledVehicle::GetMaxAutopilotSpeedForVehicleType(TruckClass); // units not documented in the header; printed raw
	const TSubclassOf<UFGItemDescriptor> Coal = URL_VehicleLib::GetDebugFuelDescriptor();
	const float CoalEnergy = Coal ? UFGItemDescriptor::GetEnergyValue(Coal) : 0.0f;
	Sender->SendChatMessage(FString::Printf(TEXT("Route: %d nodes, %.0f m, energy %.1f MJ (= %.2f coal), max autopilot speed %.1f"),
		Route.Nodes.Num(), Route.DistanceCm / 100.0f, Route.EnergyMJ, CoalEnergy > 0.0f ? Route.EnergyMJ / CoalEnergy : 0.0f, MaxSpeed));
	return EExecutionStatus::COMPLETED;
}

EExecutionStatus ARL_DebugCommand::CmdSpawn(UCommandSender* Sender, const TArray<FString>& Args)
{
	if (Args.Num() < 1) { PrintHelp(Sender); return EExecutionStatus::BAD_ARGUMENTS; }
	const AFGDockingStationIdentifier* StationId = ResolveStation(Sender, Args[0]);
	AFGBuildableDockingStation* Station = StationId ? StationId->GetStation() : nullptr;
	if (!Station) return EExecutionStatus::BAD_ARGUMENTS;

	const int32 Fuel = Args.Num() >= 2 && Args[1].IsNumeric() ? FCString::Atoi(*Args[1]) : 20;

	FString Error;
	AFGWheeledVehicle* Vehicle = URL_VehicleLib::SpawnVehicleAtStation(Station, URL_VehicleLib::GetDefaultTruckClass(),
		URL_VehicleLib::GetDebugFuelDescriptor(), Fuel, Error);
	if (!Vehicle)
	{
		Sender->SendChatMessage(FString::Printf(TEXT("Spawn failed: %s"), *Error), FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}
	if (ARL_TransportSubsystem* Subsystem = ARL_TransportSubsystem::Get(this))
	{
		Subsystem->WatchVehicle(Vehicle);
	}
	Sender->SendChatMessage(FString::Printf(TEXT("Spawned: %s"), *URL_VehicleLib::DescribeVehicle(Vehicle)));
	return EExecutionStatus::COMPLETED;
}

EExecutionStatus ARL_DebugCommand::CmdDespawn(UCommandSender* Sender, const TArray<FString>& Args)
{
	if (Args.Num() < 1) { PrintHelp(Sender); return EExecutionStatus::BAD_ARGUMENTS; }
	AFGWheeledVehicle* Vehicle = ResolveTruck(Sender, Args[0]);
	if (!Vehicle) return EExecutionStatus::BAD_ARGUMENTS;

	if (ARL_TransportSubsystem* Subsystem = ARL_TransportSubsystem::Get(this))
	{
		Subsystem->UnwatchVehicle(Vehicle);
	}
	URL_VehicleLib::DespawnVehicle(Vehicle);
	Sender->SendChatMessage(TEXT("Truck despawned."));
	return EExecutionStatus::COMPLETED;
}
