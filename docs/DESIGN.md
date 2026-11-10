# RoadLogistics — Diseño (estado real)

> Este documento describe la arquitectura **tal como está implementada**. Se actualiza en cada fase.
> Referencias de API vanilla/SML: `docs/RESEARCH.md`.

## Estado: Fase 1 (prueba de concepto)

### Repositorio
El repo es **solo el plugin**; se coloca en `<StarterProject>/Mods/RoadLogistics/`.
```
RoadLogistics.uplugin            descriptor SML (depende de SML ^3.12.0)
Config/                          PluginSettings.ini, AccessTransformers.ini (vacío)
Source/RoadLogistics/
  RoadLogistics.Build.cs
  Public|Private/
    RoadLogisticsModule          módulo C++ (log de arranque)
    RL_Log                       categoría de log LogRoadLogistics
    RL_GameWorldModule           módulo raíz de mundo (100 % C++)
    RL_TransportSubsystem        subsistema de servidor (AModSubsystem)
    RL_VehicleLib                utilidades sin estado sobre la API de Vehicle Paths
    Debug/RL_DebugCommand        comando de chat /rl (solo depuración)
.github/workflows/build.yml      CI: compila y empaqueta (ver docs/BUILD.md)
docs/                            RESEARCH, DESIGN, BUILD, EDITOR_STEPS, TEST_PLAN
```

### Componentes
- **`URL_GameWorldModule : UGameWorldModule`** — `bRootModule = true` en el constructor. SML descubre módulos raíz
  nativos (`PluginModuleLoader::FindRootModulesOfType`), así que **no hace falta ningún asset**. Registra
  `ARL_TransportSubsystem` (en `ModSubsystems`) y `ARL_DebugCommand` (en `mChatCommands`).
- **`ARL_TransportSubsystem : AModSubsystem, IFGSaveInterface`** — `ReplicationPolicy = SpawnOnServer`,
  tick cada 1 s. En la Fase 1 solo contiene un *watcher* de depuración que informa por chat/log de cambios de estado de
  los camiones vigilados (autopiloto, estado de error, waypoint, atraque, combustible).
  *Nombre:* la especificación pedía `URL_TransportSubsystem`, pero UHT exige el prefijo `A` para cualquier `AActor`
  (y `AModSubsystem` lo es), por eso se llama `ARL_`.
- **`URL_VehicleLib`** — funciones estáticas:
  - `FindRoadRoute`: `UFGVehiclePathNetwork::FindVehiclePath` + suma de `SegmentLength` y
    `GetVehicleFuelConsumptionForSegment` → distancia (cm) y energía (MJ).
  - `SpawnVehicleAtStation`: spawnea `BP_Truck` sobre el primer tramo que **sale** del nodo de atraque de una estación
    (a 8 m del nodo, con la rotación de la tangente del spline), asigna `Recipe_Truck`, añade combustible y fuerza el
    cálculo del tramo actual si `BeginPlay` no lo hizo.
  - `DespawnVehicle`: desatraca, apaga autopiloto, expulsa jugadores, `Destroy()`.
  - `SetRouteAndEnableAutopilot`: `AFGWheeledVehicleIdentifier::SetVehicleRoute` + `SetAutopilotEnabled(true)`.
- **`ARL_DebugCommand : AChatCommandInstance`** — `/rl stations|trucks|status|goto|stop|path|spawn|despawn`.
  Se ejecuta en el servidor; los índices son los de `AFGVehicleSubsystem::GetDockingStations()` / `GetAllVehicles()`.

### Autoridad y red
Toda la lógica está en el servidor (`HasAuthority()` en comando, librería y subsistema). Los clientes ven los camiones
porque son vehículos vanilla replicados; aún no se replica nada propio.

### Guardado
El subsistema implementa `IFGSaveInterface` (`ShouldSave = true`) pero en la Fase 1 no tiene estado persistente;
la lista de vigilancia de depuración es transitoria a propósito.

## Arquitectura prevista (fases siguientes, resumen)
- `ARL_Depot` y `ARL_LogisticsStation`: subclases C++ de `AFGBuildableDockingStation` (reutilizan la malla vanilla
  vía Blueprint hijo). Sobrescriben `Factory_TickProducing` para transferir exactamente la cantidad del pedido a los
  camiones del mod y retenerlos/soltarlos con `ForceUndockActor`.
- Ruta de un viaje: `[depósito, proveedor, consumidor]` (cíclica en vanilla ⇒ vuelve al depósito); al atracar en el
  depósito se despawnea.
- `FRL_Trip` con máquina de estados `Spawning → ToSource → Loading → ToDestination → Unloading → Returning → Despawning`,
  pedidos, reservas y estadísticas como `UPROPERTY(SaveGame)` del subsistema.
