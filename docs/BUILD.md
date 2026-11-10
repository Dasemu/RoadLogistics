# Compilación y empaquetado

La compilación se hace en **GitHub Actions** (`.github/workflows/build.yml`), sin necesidad del PC.
El workflow replica el CI oficial de SML: descarga el editor CSS para Linux, integra Wwise, instala MSVC
bajo Wine, compila `FactoryEditor` (comprobación de compilación) y empaqueta el mod para Win64 con
`RunUAT PackagePlugin` (lo mismo que hace Alpakit). El zip resultante queda como *artifact* de la ejecución.

## Configuración única (5–10 minutos, desde el navegador)

### 1. Acceso al motor de Satisfactory (Unreal Engine CSS)
El motor está en el repo privado `satisfactorymodding/UnrealEngine`. Para tener acceso:
1. Vincula tu cuenta de GitHub a tu cuenta de Epic Games: <https://www.unrealengine.com/en-US/ue-on-github>
   (Perfil de Epic → *Apps and Accounts* → GitHub → *Link*).
2. Acepta la invitación a la organización **EpicGames** que llega por email (o en <https://github.com/EpicGames>).
3. Comprueba que puedes abrir <https://github.com/satisfactorymodding/UnrealEngine/releases> con tu cuenta.

### 2. Token de GitHub (`UE_TOKEN`)
1. <https://github.com/settings/tokens> → *Generate new token* → **Generate new token (classic)**.
2. Nombre: `RoadLogistics CI`. Caducidad: la que prefieras. Scope: marca solo **`repo`**.
   (Un token *fine-grained* no sirve: el repo del motor no es tuyo.)
3. Copia el token.

### 3. Cuenta de Wwise (`WWISE_EMAIL`, `WWISE_PASSWORD`)
Crea una cuenta gratuita en <https://www.audiokinetic.com/> (botón *Register*). El proyecto de SML necesita el
SDK de Wwise para compilar; el CI lo descarga con estas credenciales.

### 4. Guardar los secretos en el repo
<https://github.com/dasemu/roadlogistics/settings/secrets/actions> → *New repository secret*, tres veces:

| Nombre | Valor |
|---|---|
| `UE_TOKEN` | el token del paso 2 |
| `WWISE_EMAIL` | email de Audiokinetic |
| `WWISE_PASSWORD` | contraseña de Audiokinetic |

Los secretos están cifrados; nadie (ni yo) puede leerlos, solo los usa el workflow.

## Uso
- Cada `push` a `main` o `claude/**` que cambie código lanza el build. También se puede lanzar a mano:
  pestaña **Actions** → *Build* → *Run workflow*.
- Resultado: en la ejecución terminada, sección *Artifacts* → **RoadLogistics** (zip).
- La primera ejecución es la más lenta (compila Wine y descarga MSVC; después quedan en caché).
  El build completo del proyecto de SML es pesado: si superase el límite de 6 h de un runner gratuito,
  lo dividiremos en varios trabajos.
- Si el repo es privado, cada ejecución consume minutos de Actions de tu cuota.

## Instalar el mod para probar (en tu PC)
1. Instala **SML 3.12** (o superior compatible) con el Satisfactory Mod Manager (SMM).
2. Descomprime el zip del artifact (contiene otro zip `RoadLogistics-Windows.zip` o similar; descomprímelo también)
   en `<carpeta del juego>\FactoryGame\Mods\RoadLogistics\` de forma que exista
   `...\Mods\RoadLogistics\RoadLogistics.uplugin`.
3. Arranca el juego. En el log (`%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log`) debe aparecer
   `LogRoadLogistics: RoadLogistics module started`.

Limitación actual: solo se empaqueta el cliente Win64 (sirve para un jugador y para partidas multijugador
alojadas por un jugador). El empaquetado para servidor dedicado se añadirá más adelante.

## Compilar en local (alternativa)
Con el Starter Project instalado según <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/index.html>,
clona este repo en `<StarterProject>\Mods\RoadLogistics` y:
```
"<UE>\Engine\Build\BatchFiles\Build.bat" FactoryEditor Win64 Development -project="<StarterProject>\FactoryGame.uproject"
```
y empaqueta con Alpakit desde el editor.
