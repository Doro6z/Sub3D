# Sub Fluidity Jitter — Debug Instrumentation Handoff — 2026-04-18

Référence : [2026-04-18_sub_fluidity_architecture.md](/C:/Dev/Sub3D/reports/analysis/2026-04-18_sub_fluidity_architecture.md), section Point 1.

---

## Contexte

Point 1 (dual-buffer interpolation) est implémenté et compile. Les tests PIE montrent que **le jitter est toujours présent**, aggravé aux framerates >60, stable à 30 FPS. Les logs `LogSubInteriorFrame` exhibent un pattern `+20.84 cm / -3.74 cm` : l'actor recule visuellement de 3.74 cm entre deux frames consécutives.

**Problème de cohérence :** la math de mon interp ([SubMovementComponent.cpp:245](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:245)) garantit `delta ≥ 0` entre deux ticks (SimAccumulator monotone hors pas de sim, α clampé à [0, 1]). Un delta négatif implique donc **un autre writer sur l'actor root**, ou un bug math que j'ai raté.

L'instrumentation ci-dessous est faite pour trancher entre ces deux hypothèses en 30 secondes de PIE.

---

## Ce qui est dans le code maintenant

### 1. Toggle d'isolation de l'interp

- CVar `sub.DisableVisualInterp` (bool, défaut `false`) : [SubMovementComponent.cpp:21](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:21).
- Quand `true`, le bloc `Lerp(PrevSim, CurrSim, α)` est skippé. L'actor root reste sur CurrSim en fin de tick (même comportement qu'à 30 FPS en temps normal).
- Condition d'entrée dans le bloc interp : [SubMovementComponent.cpp:256](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:256).

### 2. Trace structurée par tick

- CVar `sub.LogVisualInterp` (bool, défaut `false`) : [SubMovementComponent.cpp:30](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:30).
- Quand `true`, emit `SUB_TRACE` par tick authority avec :
  - `DT` / `Accum` / `Alpha` / `Steps` / `Contact` / `Interp`
  - `PreUndo` : actor.Location **avant** l'undo de début de tick
  - `PostUndo` : actor.Location après l'undo (= CurrSim_prev)
  - `PostSim` : actor.Location après le sim loop (= CurrSim_curr)
  - `PostInterp` : actor.Location après le write interp (ou = PostSim si skip)
  - `PrevSim` / `CurrSim` : les deux bornes de la Lerp
  - `ExtDrift` : `PreUndo - LastPostTick` (devrait être 0 — sinon un writer externe a bougé l'actor entre notre write de fin de tick et le début du tick suivant)
  - `TickDelta` : `PostInterp - LastPostTick` (delta visuel total ce tick, devrait être ≥ 0 en croisière avant)

### 3. Détecteurs d'anomalie (Error-level)

- `SUB_TRACE EXTERNAL_WRITE` : quand `|ExtDrift| > sub.TraceExternalDriftThresholdCm` (défaut 0.01 cm). Un writer externe modifie l'actor root.
- `SUB_TRACE BACKWARD` : quand `TickDelta.X < -sub.TraceBackwardThresholdCm` (défaut 0.1 cm). L'actor recule sur X en croisière avant — interdit mathématiquement.

Tous deux loggés au niveau `Error` dans `LogSubMovement`, filtrable au log.

---

## Procédure de test — à exécuter en PIE

### Préparation commune

1. Ouvrir Sub3D dans l'éditeur (`C:/Dev/Sub3D/LaunchEditor.bat`).
2. Ouvrir la carte de test habituelle (Proto03 ou équivalent avec un long couloir d'eau libre).
3. Lancer PIE, embark au helm.
4. Ouvrir la console (`~`).
5. Activer la trace : `sub.LogVisualInterp 1`.
6. Filtrer le log pour ne voir que les anomalies :
   ```
   log LogSubMovement Error
   ```
   Ou tout voir :
   ```
   log LogSubMovement Log
   ```
7. Lock le framerate pour un test reproductible :
   ```
   t.MaxFPS 90
   ```

### Test A — Interp ON (baseline)

**But :** observer le comportement actuel avec Point 1 actif.

1. `sub.DisableVisualInterp 0` (défaut, inutile de taper si pas changé).
2. Thrust = +1, cap droit, 10 secondes.
3. Stop PIE.
4. Dans `Saved/Logs/Sub3D.log`, extraire :
   - **Toutes** les lignes `SUB_TRACE BACKWARD`.
   - **Toutes** les lignes `SUB_TRACE EXTERNAL_WRITE`.
   - **30 lignes consécutives** de `SUB_TRACE` en régime de croisière (après ~3s).

### Test B — Interp OFF (contrôle)

**But :** voir si le jitter disparaît quand on skip le bloc interp.

1. Redémarrer PIE (pour remettre l'état sim à zéro).
2. `sub.LogVisualInterp 1`, `t.MaxFPS 90`.
3. `sub.DisableVisualInterp 1` **avant** d'embarker.
4. Embark, thrust = +1, cap droit, 10 secondes.
5. Observer visuellement : jitter présent ou pas ?
6. Stop PIE.
7. Extraire les mêmes données qu'au Test A.

### Test C — Interp ON, 30 FPS (sanity)

**But :** confirmer qu'à 30 FPS la trace montre α qui sature à 1 et que `bWroteInterp` reste souvent à 0.

1. Redémarrer PIE. Cvars comme Test A.
2. `t.MaxFPS 30`.
3. Thrust = +1, 10 secondes.
4. Stop PIE.
5. Extraire 30 lignes `SUB_TRACE`.

---

## Grille de décision

À remonter : le verdict Test A + Test B + Test C, plus les lignes demandées.

| Test A (Interp ON, 90 FPS) | Test B (Interp OFF, 90 FPS) | Interprétation | Action suivante |
|---|---|---|---|
| Jitter visible + `EXTERNAL_WRITE` loggé | Jitter visible + `EXTERNAL_WRITE` loggé | Un writer externe écrit l'actor entre nos ticks. Point 1 pas fautif. | Chasser le writer avec les positions PreUndo / LastPostTick reportées dans `EXTERNAL_WRITE`. |
| Jitter visible + `BACKWARD` loggé, `EXTERNAL_WRITE` pas loggé | Jitter absent ou fortement réduit | Mon interp produit le backward motion. Bug math de mon côté. | Recalculer la séquence `PrevSim`/`CurrSim`/α à partir des lignes SUB_TRACE qui ont `BACKWARD`. |
| Jitter visible, aucune anomalie loggée | Jitter identique | Le jitter vient d'ailleurs (crew, caméra, widget, animation). Pas lié à la pose du sub. | Élargir le scope d'investigation hors SubMovement. |
| Jitter absent | — | Build désynchro ou test mal exécuté. | Reproduire, vérifier `stat unit`. |

---

## Ce qu'il faut me remonter exactement

Dans un message de suivi, coller **uniquement** :

1. **Résultat visuel Test A** : jitter présent / absent / réduit (une phrase).
2. **Résultat visuel Test B** : jitter présent / absent / réduit (une phrase).
3. **Résultat visuel Test C** : jitter présent / absent / réduit (une phrase).
4. **Du log Test A** :
   - Nombre total de lignes `EXTERNAL_WRITE` sur les 10s.
   - Nombre total de lignes `BACKWARD` sur les 10s.
   - 2-3 lignes `EXTERNAL_WRITE` si présentes (copie brute).
   - 2-3 lignes `BACKWARD` si présentes (copie brute).
   - **10 lignes consécutives** de `SUB_TRACE` en cruise stable.
5. **Du log Test B** : mêmes items que Test A.
6. **Du log Test C** : 10 lignes consécutives de `SUB_TRACE` (pas besoin des anomalies ici).

Ça fait ~50 lignes à coller, je peux faire la soustraction de valeurs à la main et décider.

---

## Tests automation (si tu veux couper court)

Si tu veux éviter de tester manuellement et que je décide tout seul, lance le test automation headless. Il est **pas encore écrit** — je peux te l'écrire en suivant si tu me le demandes. Ce qu'il ferait :

- Spawn `ASubmarineBase` dans un monde vide.
- Force `Velocity = (750, 0, 0)`.
- Tick N fois à `DeltaTime = 1/90` (constant).
- Capture `actor.Location.X` à chaque tick.
- Assert `delta.X >= -0.01 cm` sur tous les ticks.

Commande pour le lancer une fois écrit :

```bash
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "C:/Dev/Sub3D/Sub3D.uproject" \
  -ExecCmds="Automation RunTests Sub3D.Movement.MonotonicForwardCruise" \
  -Unattended -NullRHI -NoSound -Log
```

Avantage : déterministe, pas de variabilité de frame timing. Inconvénient : ne capture pas les interactions avec d'autres composants (animations, réplication si on en simule, etc.) qui peuvent être la source si un writer externe est impliqué.

**Recommandation :** faire Test A + B + C d'abord (30 min total), remonter les chiffres, décider si le test auto vaut le coup.

---

## Angles de debug secondaires si on en a besoin

### Tracer tout `SetActorLocation*` sur le sub

Override `ASubmarineBase::PostActorMove` ou un hook `OnActorMoved` qui log source + call stack. Révèle **immédiatement** tout writer externe si présent. Je peux l'ajouter si Test A / B pointent vers un writer externe.

### Dump CSV

CVar `sub.TraceCSV 1` qui écrit un CSV par tick dans `Saved/Logs/SubTrace.csv`. Plus facile à plotter que les lignes de log. Pas implémenté — à ajouter si les 50 lignes de log ne suffisent pas.

### Draw debug

`DrawDebugSphere` sur PrevSim, CurrSim, PostInterp chaque frame, couleur différente. Visuel direct du cycle sur le sub. Utile en dernier recours.

---

## Référence rapide — CVars ajoutées

| CVar | Type | Défaut | Effet |
|---|---|---|---|
| `sub.DisableVisualInterp` | bool | 0 | Skip le bloc interp. Actor sit sur CurrSim. |
| `sub.LogVisualInterp` | bool | 0 | Emit `SUB_TRACE` par tick + anomalies. |
| `sub.TraceExternalDriftThresholdCm` | float | 0.01 | Seuil de détection `EXTERNAL_WRITE` sur `|ExtDrift|`. |
| `sub.TraceBackwardThresholdCm` | float | 0.1 | Seuil de détection `BACKWARD` sur `TickDelta.X`. |

Toutes togglables à chaud en PIE via la console.
