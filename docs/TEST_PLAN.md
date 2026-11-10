# Plan de pruebas

## Fase 1 — Prueba de concepto: un camión vanilla conducido por el mod

### Preparación
1. Instala el mod según `docs/BUILD.md` → *Instalar el mod para probar*.
2. Usa (o crea) una partida con: una red de **vehicle paths** de camión, **al menos 2 estaciones de camión**
   conectadas a esa red, y **un camión** colocado sobre un path. No hace falta que el camión tenga ruta.
3. Abre el chat (tecla `Enter`). Todos los comandos empiezan por `/rl`.

### Pruebas

| # | Acción | Resultado esperado |
|---|---|---|
| 1 | Arrancar el juego y cargar la partida | En el log aparece `LogRoadLogistics: RoadLogistics module started`. Sin errores de SML sobre RoadLogistics. |
| 2 | `/rl` | Muestra la línea de ayuda con los subcomandos. |
| 3 | `/rl stations` | Lista `[i] 'Nombre' at X,Y,Z network=N`. Las estaciones conectadas comparten el mismo `network`. |
| 4 | `/rl trucks` | Lista tus camiones con `autopilot`, `status`, `fuel`, `onPath`. |
| 5 | `/rl path 0 1` | `Route: N nodes, D m, energy E MJ (= C coal), max autopilot speed S`. La distancia debe ser parecida a la real por carretera. Si dice *No road route*, las estaciones no están en la misma red. |
| 6 | Da combustible al camión. `/rl goto <camión> 0 1` | **Criterio de éxito de la fase:** el camión activa el autopiloto, sale solo y llega a la estación 0 por la red. En el chat aparecen mensajes `[RL] ...` con `autopilot=on status=None`, cambios de `wp=`, y `DOCKED at ...` / `UNDOCKED from ...` al atracar en cada estación. Luego sigue a la 1 y vuelve a la 0 en bucle. |
| 7 | `/rl status <camión>` | Muestra el estado y los waypoints `wp[0]`, `wp[1]` con los nombres de las estaciones. |
| 8 | `/rl goto <camión> 0` (una sola estación) | Anota qué `status=` aparece (esperamos `TooFewStations`; es un dato para el diseño, no un fallo). |
| 9 | `/rl stop <camión>` | `autopilot=off`; el camión se detiene. |
| 10 | `/rl spawn 0 20` | Aparece un camión nuevo **sobre el path que sale de la estación 0**, a unos 8 m. El chat dice `Spawned: ... onPath=yes fuel=20`. |
| 11 | `/rl trucks` y luego `/rl goto <nuevo> 1 0` | El camión spawneado conduce solo a la estación 1 y vuelve a la 0. |
| 12 | `/rl despawn <nuevo>` | El camión desaparece. `/rl trucks` ya no lo lista. El resto de camiones sigue funcionando (no se quedan bloqueados en un cruce que usaba el borrado). |
| 13 | Con un camión en ruta: guardar, salir al menú, cargar | El camión sigue su ruta tras cargar (estado vanilla). La vigilancia `[RL]` se pierde tras cargar; es normal en esta fase. |
| 14 | (Opcional) Multijugador: un amigo se une como cliente | El cliente ve moverse los camiones de las pruebas 6 y 11. Los comandos `/rl` funcionan para el anfitrión. |

### Qué enviarme
- Para cada prueba: ✅ / ❌ y lo que viste.
- En especial: el `status=` de las pruebas 6, 8 y 11, si `onPath` fue `yes` en la 10, y cualquier error.
- Las líneas del log con `LogRoadLogistics` (`%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log`).

### Si algo falla
- **Prueba 10 con `onPath=no` o prueba 11 con `status=NotOnPath`:** el spawn no queda "sobre el path". Ajustaré
  la posición/altura del spawn o el cálculo del tramo actual.
- **Prueba 6 sin movimiento con `status=None`:** puede faltar combustible o que la ruta no se aplique en caliente.
- **Prueba 12 deja camiones bloqueados:** `Destroy()` no libera reservas; probaré `TurnOffAndDestroy()`.
