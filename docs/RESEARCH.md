# RoadLogistics — Fase 0: Investigación

> Fuente: Starter Project de SML, rama `master`, commit `a7e8550` ("Update headers to CL517554", 2026-11-04).
> `FactoryGame.uproject` → `"EngineAssociation": "5.6.1-CSS"`. SML `3.12.0` (`Mods/SML/SML.uplugin`, `GameVersion >=491125`).
>
> Todas las rutas son relativas a la raíz del Starter Project. Los números de línea corresponden a ese commit.

## 0. Advertencia metodológica importante

Los `.cpp` de `Source/FactoryGame/Private/**` del Starter Project **son stubs** (cuerpos vacíos que devuelven
valores por defecto). Ejemplo: `Source/FactoryGame/Private/WheeledVehicles/FGWheeledVehicleIdentifier.cpp`
contiene `void AFGWheeledVehicleIdentifier::SetVehicleRoute(const TArray<FGuid>& newVehicleRoute){ }`.

Consecuencia: **las firmas y la accesibilidad están verificadas** (clases `FACTORYGAME_API`, funciones públicas),
pero **el comportamiento** solo se puede deducir de nombres y comentarios de los headers. Todo lo marcado como
*[inferido]* debe confirmarse en el juego en la Fase 1.

---

## 1. Red de Vehicle Paths: clases y consulta del grafo

| Concepto | Clase | Header |
|---|---|---|
| Nodo (cruce / extremo) | `AFGVehiclePathNode : AFGBuildable` | `Source/FactoryGame/Public/WheeledVehicles/FGVehiclePathNode.h:21` |
| Nodo independiente visible (waypoint) | `AFGStandaloneVehiclePathNode : AFGVehiclePathNode` | `.../WheeledVehicles/FGStandaloneVehiclePathNode.h:13` |
| Tramo (spline dirigido nodo→nodo) | `AFGVehiclePathSegment : AFGBuildable` | `.../WheeledVehicles/FGVehiclePathSegment.h:150` |
| Red conexa (grafo) | `UFGVehiclePathNetwork : UObject` | `.../WheeledVehicles/FGVehicleSubsystem.h:104` |
| Subsistema global | `AFGVehicleSubsystem : AFGSubsystem` | `.../WheeledVehicles/FGVehicleSubsystem.h:276` |
| Preset de vehículo (qué tramos puede usar) | `UFGVehiclePathPreset : UDataAsset` | `.../WheeledVehicles/FGVehiclePathPreset.h:14` |
| Grafo A* interno | `FVehiclePathNavigationGraph` (struct, no UObject) | `.../WheeledVehicles/FGVehiclePathfinding.h:21` |

Los tramos son **dirigidos**: un tramo tiene `GetStartNode()` y `GetEndNode()`; un nodo tiene tramos que llegan y que salen.

### Consulta a nivel de actor (solo servidor o actores replicados)
```cpp
// FGVehiclePathNode.h
FGuid GetPathNodeGUID() const;                                              // :49
const TArray<AFGVehiclePathSegment*>& GetArrivingConnections() const;      // :53
const TArray<AFGVehiclePathSegment*>& GetLeavingConnections() const;       // :57
void GetConnectedNodes( TArray<AFGVehiclePathNode*>& out ) const;          // :70
AFGVehiclePathSegment* FindConnectionArrivingToNode( const AFGVehiclePathNode* endNode ) const; // :74
int32 GetPathNetworkID() const;                                            // :84 (volátil, solo servidor)

// FGVehiclePathSegment.h
USplineComponent* GetSplineComponent() const;                              // :192
AFGVehiclePathNode* GetStartNode() const;                                  // :217
AFGVehiclePathNode* GetEndNode() const;                                    // :221
bool IsPathValidForPreset( const UFGVehiclePathPreset* ) const;            // :209
const TArray<AFGWheeledVehicle*>& GetVehicles() const;                     // :236
bool IsJunctionBlock() const;                                              // :251
```

### Consulta a nivel de red (servidor y clientes, datos compactos)
```cpp
// FGVehicleSubsystem.h
static AFGVehicleSubsystem* AFGVehicleSubsystem::Get( const UObject* worldContext );           // :297
const TArray<UFGVehiclePathNetwork*>& GetAllPathNetworks() const;                             // :304
UFGVehiclePathNetwork* FindNetworkByPathNodeGuid( const FGuid& ) const;                        // :314
const TArray<AFGDockingStationIdentifier*>& GetDockingStations() const;                       // :318
const TArray<AFGWheeledVehicleIdentifier*>& GetAllVehicles() const;                           // :322
AFGVehiclePathNode* FindReplicatedPathNodeByGuid( const FGuid& ) const;                        // :326
AFGDockingStationIdentifier* FindDockingStationIdentifierForPathNodeGuid( const FGuid& ) const;// :330

// UFGVehiclePathNetwork
const TArray<FVehiclePathNetworkNodeData>& GetPathNodes() const;      // :121  {PathNodeGuid, PathNodeLocation}
const TArray<FVehiclePathNetworkSegmentData>& GetPathSegments() const;// :125  {FromNodeIndex, ToNodeIndex, SegmentLength, SplinePoints, VehicleTraversabilityMask, VehicleSegmentVirtualLength}
int32 FindPathNodeIndexByGuid( const FGuid& ) const;                  // :137
int32 FindPathIndexBetweenPathNodes( int32 fromNodeIndex, int32 toNodeIndex ) const; // :141
bool  CanVehicleTraverseSegment( const FVehiclePathNetworkSegmentData&, const UFGVehiclePathPreset* ) const; // :145
float GetVehicleFuelConsumptionForSegment( const FVehiclePathNetworkSegmentData&, const TSubclassOf<AFGWheeledVehicle>& ) const; // :149
void  PopulateNetworkStations( TArray<AFGDockingStationIdentifier*>& ) const;   // :161
void  PopulateNetworkVehicles( TArray<AFGWheeledVehicleIdentifier*>& ) const;   // :165
```
`FVehiclePathNetworkNodeData` y `FVehiclePathNetworkSegmentData` están en `FGVehicleSubsystem.h:27` y `:44`.

**Conclusión:** el grafo es totalmente consultable desde C++ con API pública.

---

## 2. Cómo recibe destinos el autopiloto en 1.2

El autopiloto **no** usa horarios con condiciones como los trenes: usa una **lista ordenada de waypoints
(GUIDs de nodos de path)** guardada en el *identificador* del vehículo, que recorre en bucle.

`AFGWheeledVehicleIdentifier` — `Source/FactoryGame/Public/WheeledVehicles/FGWheeledVehicleIdentifier.h:32`
(se obtiene con `AFGWheeledVehicle::GetVehicleIdentifier()`, `FGWheeledVehicle.h:79`)

```cpp
const TArray<FGuid>& GetVehicleRoute() const;                 // :82
bool  IsAutopilotEnabled() const;                             // :86
EVehicleAutopilotErrorStatus GetAutopilotErrorStatus() const; // :89
bool  IsCurrentlyDocking() const;                             // :93
FGuid GetCurrentFromPathNodeGUID() const;                     // :97
FGuid GetCurrentToPathNodeGUID() const;                       // :101
int32 GetCurrentTargetWaypointIndex() const;                  // :105
bool  CanEnableAutopilot() const;                             // :121
void  SetAutopilotEnabled( bool );                            // :125  UFUNCTION(BlueprintCallable)
void  RemoveWaypointAtIndex( int32 );                         // :131  UFUNCTION(BlueprintCallable)
void  AddWaypoint( const FGuid& );                            // :135  UFUNCTION(BlueprintCallable)
void  InsertWaypoint( int32, const FGuid& );                  // :139  UFUNCTION(BlueprintCallable)
void  SetVehicleRoute( const TArray<FGuid>& );                // :143  UFUNCTION(BlueprintCallable)
FOnVehicleAutopilotStatusChangedDelegate OnVehicleAutopilotStatusChanged; // :160 (BlueprintAssignable)
```
Estados de error (`FGWheeledVehicleIdentifier.h:18`):
`None, StationUnreachable, NotOnPath, TooFewStations, NoFuel, Deadlocked`.

- **¿Modificable en tiempo de ejecución desde C++?** Sí: son funciones públicas de una clase exportada
  (`FACTORYGAME_API`). La ruta se guarda (`UPROPERTY(SaveGame, Replicated) mVehicleRoute`, `:182`) y lleva un
  *changelist* (`GetVehicleRouteChangelist()`, `:150`) que el autopiloto usa para recalcular
  (`UFGVehicleAutopilotComponent::mCurrentRouteChangelist`, `FGVehicleAutopilotComponent.h:359`).
- **El waypoint de una estación** es el GUID del nodo de atraque de la estación:
  `AFGDockingStationIdentifier::GetPathNodeGUID()` (`FGDockingStationIdentifier.h:64`) o
  `AFGBuildableDockingStation::GetDockingPathNode()->GetPathNodeGUID()`.
- *[inferido]* `TooFewStations` sugiere que la ruta necesita **≥ 2 estaciones**. Nuestras rutas tendrán 3
  (depósito, proveedor, consumidor), así que no es un problema.
- *[inferido]* La ruta es **cíclica** (vuelve al índice 0). Para nosotros es útil: `[depósito, proveedor, consumidor]`
  produce exactamente depósito → proveedor → consumidor → depósito.
- *[inferido]* Al llegar a un waypoint que es estación, el vehículo **atraca** (`FVehicleAutopilotComponentSimData::ServerDockingStationToDock`,
  `FGVehicleAutopilotComponent.h:128`) y espera a que la estación lo suelte.

La lista de objetivos antigua (`AFGDrivingTargetList`, `UFGTargetPointLinkedList`, `FGTargetPointLinkedList.h`) está
marcada como **Deprecated**, solo para cargar partidas antiguas.

---

## 3. Pathfinding: ruta y distancia entre estaciones

```cpp
// FGVehicleSubsystem.h:157 — UFUNCTION(BlueprintCallable), público
bool UFGVehiclePathNetwork::FindVehiclePath( const FGuid& fromNodeGuid, const FGuid& toNodeGuid,
                                             const UFGVehiclePathPreset* vehicleType,
                                             TArray<FGuid>& out_vehiclePath ) const;
```
Devuelve la secuencia de GUIDs de nodos (A* sobre `FVehiclePathNavigationGraph`, `FGVehiclePathfinding.h:34`).

**Distancia por carretera** (sin API dedicada, pero componible con API pública):
```
for i in 1..N-1:
  a = net->FindPathNodeIndexByGuid(path[i-1]); b = net->FindPathNodeIndexByGuid(path[i])
  seg = net->GetPathSegmentAtIndex( net->FindPathIndexBetweenPathNodes(a, b) )
  distancia += seg.SegmentLength                                   // cm
  energía   += net->GetVehicleFuelConsumptionForSegment(seg, TruckClass)   // MW·s (MJ)
```
- Preset del camión: `AFGWheeledVehicle::GetPathPathPresetForVehicleType(TSubclassOf<AFGWheeledVehicle>)` (`FGWheeledVehicle.h:136`).
- Velocidad máxima de autopiloto: `AFGWheeledVehicle::GetMaxAutopilotSpeedForVehicleType(...)` (`FGWheeledVehicle.h:144`) → ETA estimada.
- Energía de un combustible: `UFGItemDescriptor::GetEnergyValue(TSubclassOf<UFGItemDescriptor>)` (`Resources/FGItemDescriptor.h:169`) → **coste de combustible por viaje calculable antes de despachar**.
- Mismo red: dos estaciones solo son conectables si `FindNetworkByPathNodeGuid` devuelve la misma red.

**Conclusión:** sí hay pathfinding accesible; podemos elegir proveedor por distancia real por carretera.

---

## 4. Estación de camiones

`AFGBuildableDockingStation : AFGBuildableFactory` (abstracta) — `Source/FactoryGame/Public/Buildables/FGBuildableDockingStation.h:63`.
La estación vanilla es el Blueprint `/Game/FactoryGame/Buildable/Factory/TruckStation/Build_TruckStation` (placeholder presente en `Content/`).

Inventarios y estado (todos públicos):
```cpp
AFGDockingStationIdentifier* GetStationIdentifier() const;  // :98
AFGVehiclePathNode* GetDockingPathNode() const;             // :102
UFGInventoryComponent* GetFuelInventory() const;            // :106
UFGInventoryComponent* GetInventory() const;                // :110  (inventario de carga/descarga)
AActor* GetDockedActor() const;                             // :114
virtual bool DockActor( AActor* actor );                    // :122  UFUNCTION(BlueprintCallable)
virtual void ForceUndockActor();                            // :126  UFUNCTION(BlueprintCallable)
virtual void SetIsInLoadMode( bool );  virtual bool GetIsInLoadMode() const; // :130 / :134
virtual bool IsLoadUnloading() const;                       // :146
virtual void Factory_TickProducing( float dt ) override;    // :78
```

**Eventos de atraque/desatraque:** la estación **no expone delegados** de atraque. Lo que hay:
- `virtual bool DockActor(AActor*)` y `virtual void ForceUndockActor()` → se pueden **sobrescribir en una subclase** o
  **hookear con SML** (`SUBSCRIBE_METHOD_VIRTUAL`, `Mods/SML/Source/SML/Public/Patching/NativeHookManager.h:601`).
- En el vehículo: `IFGDockableInterface::WasDocked/WasUndocked` (`FGWheeledVehicle.h:65-66`, `FGDockableInterface.h:40`),
  y eventos BP `ServerNotifyVehicleDocked/Undocked` (protegidos, `FGWheeledVehicle.h:205/209`).
- Sondeo barato: `AFGWheeledVehicle::GetCurrentlyDockingStationActor()` (`:107`) y `AFGWheeledVehicleIdentifier::IsCurrentlyDocking()`.

**Control de la transferencia:** la carga/descarga vanilla ocurre en `Factory_TickProducing` (virtual) mediante
`Factory_LoadUnloadDockedInventory` (protegida, no virtual, `:242`). Una subclase propia puede sobrescribir
`Factory_TickProducing`: si el actor atracado es un camión del mod, hace **nuestra** transferencia exacta y llama a
`ForceUndockActor()` cuando proceda (o lo retiene para "esperar a que haya sitio"); si no, `Super::`.

Inventario del camión: `AFGWheeledVehicle::GetStorageInventory()` (`:91`), `GetFuelInventory()` (`:87`).
API de inventario (`Source/FactoryGame/Public/FGInventoryComponent.h`): `GetNumItems` (:415), `AddStack` (:271),
`Remove` (:336), `RemoveFromIndex` (:347), `HasEnoughSpaceForStack(s)` (:425/:429), `GetInventoryStacks` (:451),
`GetSizeLinear` (:443), `SetAllowedItemOnIndex` (:510). Tamaño de pila: `UFGItemDescriptor::GetStackSize` (`FGItemDescriptor.h:251`).

---

## 5. Spawnear un camión sobre un path con combustible y autopiloto

No hay una función "spawn vehicle" de alto nivel; el juego construye vehículos desde el holograma
(`AFGVehicleHologram::ConstructVehicle`, protegida, `Hologram/FGVehicleHologram.h:41`). Procedimiento propuesto,
**solo con API pública**:

1. Clase: `/Game/FactoryGame/Buildable/Vehicle/Truck/BP_Truck.BP_Truck_C` (placeholder presente en `Content/`).
   Alternativa: `UFGVehicleDescriptor::GetVehicleClass(Desc_Truck)` (`Resources/FGVehicleDescriptor.h:26`).
2. Transform: un punto del spline de un tramo que **sale** del nodo de atraque del depósito
   (`GetDockingPathNode()->GetLeavingConnections()[0]->GetSplineComponent()`), con la rotación de la tangente.
   Debe cumplir los umbrales de "en el path": `mMaxDistanceFromPathToConsiderVehicleOnPath = 200 cm` y
   `mMaxAngleBetweenVehicleAndPathToConsiderVehicleOnPath = 10°` (`FGWheeledVehicle.h:307/311`).
3. `World->SpawnActorDeferred<AFGWheeledVehicle>(...)`, `SetBuiltWithRecipe(Recipe_Truck)` (`FGVehicle.h:238`,
   para que desmontar devuelva lo correcto), `FinishSpawning`.
4. *[inferido]* En `BeginPlay` el vehículo crea su identificador (`CreateWheeledVehicleIdentifier`, `:260`) y su tramo actual
   (`SetCurrentVehiclePathSegment(..., bIsFromBeginPlay)`, `:162`). Si no lo detecta, existe
   `UpdateCurrentVehiclePathSegmentFromVehicleLocation()` (`:184`, público).
5. Combustible: `GetFuelInventory()->AddStack(FInventoryStack(n, FuelDesc))`.
6. `GetVehicleIdentifier()->SetVehicleRoute({depot, source, dest})` y `SetAutopilotEnabled(true)`.

**Reserva de tramos:** la hace el propio autopiloto, automáticamente, para cualquier vehículo con el autopiloto activo
(`ReserveVehiclePathBlocks_Parallel`, `FGVehicleAutopilotComponent.h:228`, protegida; bloques en `FVehiclePathBlock`,
`FGVehiclePathSegment.h:146`). Nuestros camiones son camiones vanilla, así que participan en el mismo sistema de reservas y
detección de bloqueos (`mAutopilotWaitTimeToDeadlock = 120 s`, `:313` → estado `Deadlocked`). **No hay que tocar las reservas.**

**Lejos del jugador:** los vehículos pasan a "modo proxy" (`IsVehicleInProxyMode`, `FGWheeledVehicle.h:178`: "low LOD vehicles
in the distance when idle or self-driving") y el autopiloto se simula en el `AFGVehicleSubsystem::TickVehicleAutopilot`
(`FGVehicleSubsystem.h:393`). *[inferido]* Por tanto los camiones circulan aunque no haya nadie cerca.

---

## 6. Despawn limpio

1. Si está atracado: `station->ForceUndockActor()`.
2. `identifier->SetAutopilotEnabled(false)`; `vehicle->KickAllPlayers()` (`FGVehicle.h:268`, `BlueprintAuthorityOnly`).
3. Vaciar/contabilizar `GetStorageInventory()` y `GetFuelInventory()` (combustible sobrante vuelve al depósito).
4. `vehicle->Destroy()` en el servidor. `AFGWheeledVehicle` sobrescribe `Destroyed()` y `EndPlay()` (`:35/:33`) y
   *[inferido]* da de baja el identificador, el tramo actual y las reservas (`UFGVehicleAutopilotComponent::EndPlay`, `:153`).
   Existe también `AFGVehicle::TurnOffAndDestroy()` (`FGVehicle.h:171`, público), pero su semántica exacta es desconocida;
   lo probaremos como alternativa si `Destroy()` deja restos.

---

## 7. Registro en SML

- **Módulo de mundo:** `UGameWorldModule` (`Mods/SML/Source/SML/Public/Module/GameWorldModule.h:8`) con
  `mSchematics` (:20), `mResearchTrees` (:27), `mChatCommands` (:33), `ModSubsystems` (:63).
  Descubrimiento (`Mods/SML/Source/SML/Private/ModLoading/PluginModuleLoader.cpp:32-66`): busca **clases nativas** y
  Blueprints con `bRootModule = true` (`Module/ModModule.h:46`). ⇒ **el módulo raíz puede ser 100 % C++**
  (subclase con `bRootModule = true` en el constructor).
- **Subsistemas:** `AModSubsystem : AFGSubsystem` (`Subsystem/ModSubsystem.h:27`) con `ReplicationPolicy`
  (`SpawnOnServer`, `SpawnOnServer_Replicate`, `SpawnOnClient`, `:8`); acceso con
  `USubsystemActorManager::GetSubsystemActor<T>()` (`Subsystem/SubsystemActorManager.h:47`). Guardado: implementar
  `IFGSaveInterface` (`Source/FactoryGame/Public/FGSaveInterface.h`, `ShouldSave` :111) + `UPROPERTY(SaveGame)`.
- **Esquemas / recetas:** `UModContentRegistry::RegisterSchematic` (`Registry/ModContentRegistry.h:179`),
  `RegisterRecipe` (:202). Lo normal es listar los esquemas en `mSchematics`; las recetas se registran a través de los
  desbloqueos del esquema. `UFGSchematic` (`FGSchematic.h`: `mType` :202, `mTechTier` :230, `mCost` :234,
  `mUnlocks` :251, `mSchematicDependencies` :263) y `UFGRecipe` (`FGRecipe.h:40`: `mIngredients` :157, `mProduct` :161,
  `mProducedIn` :181) **pueden definirse en C++** (subclases con valores en el constructor), aunque la costumbre es usar assets.
- **Buildables:** clase C++ (`AFGBuildable…`) + **Blueprint hijo** con mallas + descriptor (`UFGBuildingDescriptor`) +
  receta. Las mallas/iconos/hologramas requieren editor → `docs/EDITOR_STEPS.md`.
- **Comandos de chat:** `AChatCommandInstance` (`Command/ChatCommandInstance.h:20`), sobrescribir
  `ExecuteCommand_Implementation(UCommandSender*, const TArray<FString>&, const FString&)` (:84); registrar en `mChatCommands`.
- **Hooks:** `SUBSCRIBE_METHOD`, `SUBSCRIBE_METHOD_VIRTUAL(_AFTER)`, `SUBSCRIBE_UOBJECT_METHOD` (`Patching/NativeHookManager.h:577-643`).
- **Config:** `UModConfiguration` (`Configuration/ModConfiguration.h:30`, `RootSection` instanciado) + `UConfigManager`
  (`Configuration/ConfigManager.h`). Normalmente es un asset BP; se puede leer desde C++ con un struct generado.

---

## 8. Compilación: qué se puede hacer en la nube

- El motor necesario es **Unreal Engine 5.6.1-CSS**, distribuido como *release* del repo **privado**
  `satisfactorymodding/UnrealEngine` (requiere cuenta de GitHub vinculada a Epic). Desde esta sesión:
  `git ls-remote https://github.com/satisfactorymodding/UnrealEngine.git` → *could not read Username* (sin acceso).
- El propio CI de SML (`.github/workflows/build.yml`) **compila en Linux**: descarga
  `UnrealEngine-CSS-Editor-Linux.tar.zst.*` con un token, integra **Wwise** (requiere cuenta Audiokinetic:
  `WWISE_EMAIL/WWISE_PASSWORD`), instala **MSVC bajo Wine** (`msvc-wine`) y ejecuta
  `Build.sh FactoryEditor Linux Development` + `RunUAT.sh PackagePlugin ... -platform=Win64 -serverplatform=Win64+Linux`.
- ⇒ **Es posible compilar sin tu PC**, pero necesito: (a) un token de GitHub con acceso a esas releases,
  (b) credenciales de Wwise, (c) disco: este contenedor tiene ~31 GB libres, probablemente insuficiente
  (motor + MSVC + Wwise + intermedios). La opción más robusta es un **workflow de GitHub Actions en
  `dasemu/roadlogistics`** que replique el CI de SML con secretos del repo; yo leo los logs del CI y corrijo errores.
  Coste: cada build tarda bastante (minutos de Actions; en repo privado consumen cuota).

---

## 9. Veredicto

**El diseño "camiones reales sobre la red vanilla" es VIABLE.** Todas las piezas tienen API pública:

| Necesidad | API | Estado |
|---|---|---|
| Consultar grafo | `UFGVehiclePathNetwork`, `AFGVehiclePathNode/Segment` | ✅ verificado |
| Dar destinos en runtime | `AFGWheeledVehicleIdentifier::SetVehicleRoute` + `SetAutopilotEnabled` | ✅ firma / ⚠️ comportamiento por probar |
| Ruta y distancia | `UFGVehiclePathNetwork::FindVehiclePath` + `SegmentLength` | ✅ |
| Coste de combustible | `GetVehicleFuelConsumptionForSegment` + `GetEnergyValue` | ✅ |
| Interceptar atraque | override de `DockActor` / `Factory_TickProducing` en subclase, o hook SML | ✅ |
| Transferencia exacta | `UFGInventoryComponent` | ✅ |
| Spawn sobre path | `SpawnActorDeferred` + transform sobre spline | ⚠️ por probar (detección "en el path") |
| Reservas de tramos | automáticas del autopiloto | ✅ (no hay que tocarlas) |
| Despawn | `Destroy()` en servidor | ⚠️ por probar (limpieza) |

**Riesgos que valida la Fase 1** (si alguno falla, se activa el plan B para esa parte):
1. Un camión spawneado por código queda "en el path" y acepta el autopiloto (sin `NotOnPath`).
2. `SetVehicleRoute` aplicado en servidor en caliente hace que el camión recalcule y salga.
3. El camión atraca en cada estación de la ruta y podemos retenerlo/soltarlo con `ForceUndockActor`.
4. `Destroy()` libera reservas e identificador sin errores.

### Plan B (por si la Fase 1 fallara)
Transporte **abstracto**: sin vehículo físico. Al despachar se calcula `ETA = distancia_por_carretera / velocidad_media`
(con `FindVehiclePath`; si no hay red, línea recta × factor 1.3), se descuentan los ítems del proveedor y el combustible del
depósito, y al vencer el temporizador se añaden al consumidor. Conserva toda la lógica de pedidos, reservas, umbrales,
estadísticas y guardado (la máquina de estados se reduce a `InTransit`). Opcionalmente un **camión "fantasma"**
cosmético (actor sin física que interpola por el spline) para que se vea moverse.
Un **híbrido** también es posible: camión real si el spawn funciona, abstracto como *fallback* si un viaje se atasca.

## 10. Decisiones que necesito de ti antes de la Fase 1
1. **Compilación**: ¿GitHub Actions con secretos (recomendado), en este contenedor (si cabe), o en tu PC?
2. **Estructura del repo**: propongo que el repo sea **solo el plugin** (`Mods/RoadLogistics/`), que clonas dentro de
   `Mods/` de tu Starter Project. El CI clonaría SML y colocaría el plugin.
3. **Estación**: ¿subclase propia de `AFGBuildableDockingStation` (más control; edificio nuevo) o reutilizar las estaciones
   vanilla vía hooks (tus estaciones existentes se vuelven logísticas)? Recomiendo **subclase propia** reutilizando la
   malla vanilla.

## 11. Decisiones tomadas (tras la Fase 0)
1. Compilación en **GitHub Actions** con secretos del repo (`docs/BUILD.md`).
2. El repo contiene **solo el plugin** (`Mods/RoadLogistics/`).
3. Estación logística = **subclase propia** de `AFGBuildableDockingStation` con la malla vanilla.
