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

## Fase 2 (diseño aprobado): flota física en bahías de garaje

Decisión del 2026-11-10: **los camiones no se spawnean ni se destruyen.** Son camiones vanilla fabricados con su
receta normal (cuestan materiales) y viven físicamente en bahías de garaje. Esto sustituye al "contador + combustible"
de la especificación original y elimina los riesgos de spawn/despawn de `RESEARCH.md` §9.

### Edificios
- **`ARL_GarageBay : AFGBuildableDockingStation`** — edificio pequeño para **un** camión. Su nodo de atraque
  (`GetDockingPathNode()`) es la "plaza de aparcamiento". Guarda `UPROPERTY(SaveGame) AssignedTruck`.
  Malla/holograma en un Blueprint hijo (paso de editor).
- **`ARL_Depot : AFGBuildableFactory`** (oficina) — inventario de combustible compartido (entrada por cinta), panel de
  flota y configuración. Agrupa las bahías que están en la **misma red de paths** y dentro de un radio configurable
  (por defecto 100 m). Límite de flota = número de bahías enlazadas (y un máximo configurable por depósito).
  Más flota ⇒ más bahías ⇒ más espacio.

### Ciclo de vida de un camión
| Estado | Cómo se representa |
|---|---|
| **Asignar** | El jugador conduce un camión hasta una bahía libre y atraca manualmente (atraque manual vanilla, `AFGWheeledVehicle::Server_ManuallyDockAtStation` vía la tecla de atracar) o lo construye sobre su path. `ARL_GarageBay::DockActor` (override) lo adopta si la bahía está libre. |
| **Aparcado (libre)** | Atracado en su bahía. `ARL_GarageBay::Factory_TickProducing` (override) **no** llama a `ForceUndockActor` ⇒ el camión queda retenido. Se rellena de combustible desde el depósito. |
| **Despachar** | Se calcula el combustible del ciclo bahía→proveedor→consumidor→bahía (`URL_VehicleLib::FindRoadRoute`, energía × margen), se carga, `SetVehicleRoute([bahía, proveedor, consumidor])` y `ForceUndockActor()`. |
| **En ruta** | Autopiloto vanilla, reservas de tramos vanilla. Atraque en proveedor/consumidor interceptado por `ARL_LogisticsStation` (Fase 3). |
| **Volver** | La ruta vanilla es cíclica ⇒ tras el consumidor vuelve solo a su bahía, atraca y queda aparcado. |
| **Desasignar** | Un jugador se sube al camión, el camión se destruye, o se desmonta la bahía ⇒ viaje abortado (reserva liberada), bahía libre, aviso por chat. El camión **nunca** desaparece por culpa del mod. |

### Cambios respecto a la especificación original
- "Spawn al despachar / despawn al volver" → **salir de / volver a su bahía**.
- "Meter camiones como ítem" → **atracar el camión en una bahía** (los camiones no son ítems de inventario en vanilla).
- Fase 6 "depósito destruido: soltar camiones como ítems" → los camiones quedan **físicos y libres** donde estén, con el
  autopiloto apagado y un aviso.
- Estado de la máquina de viaje: `Departing → ToSource → Loading → ToDestination → Unloading → Returning → Parked`
  (`Spawning`/`Despawning` desaparecen).

### Por validar al empezar la Fase 2
1. Que la bahía pueda retener indefinidamente un camión atracado (no llamar a `Super::Factory_TickProducing`).
2. Qué hace el autopiloto al soltar un camión cuya ruta empieza en su bahía (esperado: va al waypoint 1).
3. Qué pasa con un camión atracado al desactivar su autopiloto (para el aparcamiento de larga duración).

## Arquitectura prevista (fases siguientes, resumen)
- `ARL_LogisticsStation`: subclase C++ de `AFGBuildableDockingStation` (reutilizan la malla vanilla
  vía Blueprint hijo). Sobrescriben `Factory_TickProducing` para transferir exactamente la cantidad del pedido a los
  camiones del mod y retenerlos/soltarlos con `ForceUndockActor`.
- `FRL_Trip` con máquina de estados `Departing → ToSource → Loading → ToDestination → Unloading → Returning → Parked`,
  pedidos, reservas y estadísticas como `UPROPERTY(SaveGame)` del subsistema.
