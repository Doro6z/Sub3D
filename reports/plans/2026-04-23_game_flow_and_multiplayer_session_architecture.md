# Architecture : Multiplayer Game Flow & Session Contracts

**Date** : 2026-04-23
**Status** : Proposed architecture and execution plan
**Authority-max reference** : `C:\Dev\Sub3D\reports\plans\2026-04-10_first_playable_strategic_analysis.md`

> [!IMPORTANT]
> This document defines the exact boundaries, level transitions, and class ownership for the multiplayer flow: Main Menu -> Lobby (Character Creation & Ready) -> In-Game (Submarine Spawn).
> It adheres to standard Unreal Engine Listen Server architecture.

---

## 1. Goal

Establish a robust, debuggable, and clear contract system for the game loop, supporting up to 2 players (Host + Client). The flow must guarantee that players only spawn in the submarine once both are connected, customized, and ready.

## 2. Global Flow Overview

The game is divided into three distinct execution phases, each tied to a specific Level (`UWorld`) and `AGameMode` to strictly separate concerns.

1. **Phase 1: Main Menu** (Local only)
2. **Phase 2: Lobby / Pre-game** (Networked Listen Server, physical stub space)
3. **Phase 3: In-Game** (Networked Listen Server, submarine simulation)

---

## 3. Class Contracts & Ownership

### 3.1 Persistent Layer (Cross-Level)
*   **`USubGameInstance`**
    *   **Role**: The only class that survives level transitions.
    *   **Responsibilities**:
        *   Online Session Management (Create Session, Find Sessions, Join Session).
        *   Storing local player preferences (Settings).
        *   *No gameplay logic. No replication (GameInstance is local to each machine).*

### 3.2 Phase 1 : Main Menu
*   **Level**: `L_MainMenu`
*   **GameMode**: `ASubMainMenuGameMode` (Local only)
*   **PlayerController**: `ASubMainMenuPlayerController`
    *   **Role**: UI interaction only. No pawn spawned.
    *   **Flow**:
        *   Player clicks "Host Game" -> `USubGameInstance->CreateSession()` -> on success, `UWorld::ServerTravel("L_Lobby?listen")`.
        *   Player clicks "Join Game" -> `USubGameInstance->FindSessions()` -> `USubGameInstance->JoinSession()` -> Client travels to Host's `L_Lobby`.

### 3.3 Phase 2 : Lobby & Character Stub
*   **Level**: `L_Lobby` (A small, cheap 3D room to visualize the character).
*   **GameMode**: `ASubLobbyGameMode`
    *   **Role**: Wait for players, manage "Ready" states, prevent early start.
    *   **Flow**: Tracks connected `ASubLobbyPlayerController`. When the Host clicks "Start", checks if all PlayerStates have `bIsReady == true`. If yes, triggers `UWorld::ServerTravel("L_SubmarineProto")`.
*   **PlayerController**: `ASubLobbyPlayerController`
    *   **Role**: Handles UI for character creation (stub) and the "Ready" button.
*   **PlayerState**: `ASubLobbyPlayerState`
    *   **Role**: Replicates `bIsReady` and `FSubCharacterCosmeticData` (or similar struct for chosen appearance/loadout) to all clients.
*   **Pawn**: `ASubLobbyCharacter` (Optional, simplified pawn)
    *   **Role**: A visual dummy that updates its meshes based on the PlayerState data. No movement logic required, just an idle animation.

### 3.4 Phase 3 : In-Game & Submarine Spawn
*   **Level**: `L_SubmarineProto` (or whatever the default map is).
*   **GameMode**: `ASubGameMode` (The main gameplay mode)
    *   **Role**: Authoritative game rules and precise spawn orchestration.
    *   **Spawn Flow**:
        1.  `OnPostLogin`: Wait until players are fully loaded.
        2.  `ChoosePlayerStart`: Find `PlayerStart` tags explicitly placed *inside* the Submarine blueprint or in the level, bound to the sub's interior.
        3.  `SpawnDefaultPawnAtTransform`: Spawns `ASubCrewCharacter`.
        4.  Extracts cosmetic data from the incoming `PlayerState` (which persists through seamless travel, or is passed via options string / GameInstance) and applies it to the `ASubCrewCharacter`.
*   **PlayerController**: `ASubPlayerController`
    *   **Role**: Gameplay inputs, HUD management.

---

## 4. Execution Plan (Detailed Implementation Steps)

Ce plan est conçu pour être exécuté étape par étape de manière séquentielle. Chaque phase doit être validée en Play In Editor (PIE) avec `NetMode = Play As Listen Server` et 2 joueurs (Host + Client) avant de passer à la suivante.

### Step 1 : GameInstance et Data Foundations
- [ ] Créer le struct C++ `FCrewAppearanceState` (si pas déjà fait via le plan d'animation) contenant les IDs des meshes choisis (Corps, Visage, Cheveux, etc.).
- [ ] Créer la classe C++ `USubGameInstance` héritant de `UGameInstance`.
- [ ] Ajouter une propriété `FCrewAppearanceState LocalPlayerAppearance` dans `USubGameInstance` pour stocker le choix du joueur local.
- [ ] Ajouter les fonctions de session basiques (stub pour le moment, via IP locale ou Subsystem Null) : `HostGame()` et `JoinGame(FString IPAddress)`.
- [ ] Dans Project Settings -> Maps & Modes, définir `USubGameInstance` comme GameInstance Class.

### Step 2 : Phase 1 - Main Menu
- [ ] Créer les classes C++ `ASubMainMenuGameMode` et `ASubMainMenuPlayerController`.
- [ ] Créer le niveau `/Game/Maps/L_MainMenu`.
- [ ] Créer un UserWidget `WBP_MainMenu` avec deux boutons : "Host" et "Join".
- [ ] Dans `ASubMainMenuPlayerController::BeginPlay`, créer et afficher le widget, et configurer l'Input Mode en `UI Only` avec la souris visible.
- [ ] Lier le bouton "Host" pour appeler `USubGameInstance::HostGame()`.
    - L'implémentation de `HostGame()` doit faire un `UWorld::ServerTravel("L_Lobby?listen")`.
- [ ] Lier le bouton "Join" (avec un champ texte pour l'IP) pour appeler `USubGameInstance::JoinGame(IP)`.
    - L'implémentation de `JoinGame()` doit appeler `PlayerController->ClientTravel(IP, TRAVEL_Absolute)`.

### Step 3 : Phase 2 - Lobby / Character Stub (C++ & BP)
- [ ] Créer les classes C++ `ASubLobbyGameMode`, `ASubLobbyPlayerController`, `ASubLobbyPlayerState`.
- [ ] Créer la classe `ASubLobbyCharacter` (Pawn simple, hérite de `ACharacter` ou `APawn`).
- [ ] Créer le niveau `/Game/Maps/L_Lobby`.
- [ ] Dans `L_Lobby`, placer 2 `PlayerStart` et configurer le WorldSettings pour utiliser `ASubLobbyGameMode`.
- [ ] Dans `ASubLobbyPlayerState` :
    - [ ] Ajouter `UPROPERTY(Replicated) bool bIsReady;`
    - [ ] Ajouter `UPROPERTY(ReplicatedUsing=OnRep_Appearance) FCrewAppearanceState SelectedAppearance;`
    - [ ] Ajouter les RPC `Server_SetReady(bool bReady)` et `Server_UpdateAppearance(FCrewAppearanceState NewAppearance)`.
- [ ] Dans `ASubLobbyGameMode` :
    - [ ] Surcharger `PostLogin` pour assigner les couleurs/apparitions par défaut si non fournies.
    - [ ] Ajouter une fonction `CheckAllPlayersReady()` appelée à chaque fois qu'un joueur change son état "Ready". Si tous les `PlayerStates` sont prêts, activer le bouton "Launch" pour le Host (ou lancer un compte à rebours).
- [ ] Créer le widget `WBP_Lobby` avec :
    - [ ] Un menu de sélection (stub) pour modifier l'apparence (qui appelle `Server_UpdateAppearance`).
    - [ ] Un bouton "Ready" (qui appelle `Server_SetReady`).
    - [ ] Un bouton "Launch Mission" (visible uniquement si le joueur a l'autorité / est l'Host).
- [ ] Le bouton "Launch Mission" appelle le GameMode pour exécuter : `GetWorld()->ServerTravel("/Game/Maps/L_SubmarineProto");`.
- [ ] Pour le `ASubLobbyCharacter` :
    - [ ] Instancier le composant `UCrewAppearanceComponent` pour afficher le mesh Mannequin + les Static Meshes voxel choisis par le joueur.
    - [ ] Brancher le `OnRep_Appearance` du `PlayerState` pour rafraîchir ce pawn visuellement.

### Step 4 : Phase 3 - In-Game Spawn Orchestration
- [ ] Définir la map principale `/Game/Maps/L_SubmarineProto`.
- [ ] Mettre à jour `ASubGameMode` (la classe existante) :
    - [ ] Activer `bUseSeamlessTravel = true;` (optionnel mais recommandé pour éviter de redéconnecter les clients).
    - [ ] Surcharger `ChoosePlayerStart_Implementation` pour ne pas utiliser le `PlayerStart` par défaut, mais plutôt trouver le `ASubmarineBase` dans la map.
    - [ ] Trouver un emplacement valide à l'intérieur du sous-marin (ex: interroger un `UCompartmentVolumeComponent` ou un tag `PlayerSpawn` dans le BP_Submarine).
- [ ] Dans `ASubGameMode::SpawnDefaultPawnAtTransform` :
    - [ ] Spawner le `ASubCrewCharacter`.
    - [ ] Extraire les données cosmétiques (`FCrewAppearanceState`) du `PlayerState` (qui a été transféré du Lobby).
    - [ ] Appliquer ces données au `UCrewAppearanceComponent` du perso fraîchement spawn.
- [ ] Fix du Spawn Jitter/Falling :
    - [ ] Immédiatement après le spawn du character, forcer l'appel à `EnterOnFootInSubmarine` pour amorcer le `GridSpaceTransform` avant même le premier tick du sous-marin.
- [ ] Dans `ASubPlayerController` :
    - [ ] Basculer l'Input Mode en `Game Only`, cacher le curseur.

---

## 5. Submarine Spawn Edge Cases

To prevent the "Falling through the world" issue on load:
1. **The Submarine must exist**: `ASubmarineBase` should either be placed in the level or spawned synchronously in `AGameMode::BeginPlay()`.
2. **Rebase at T0**: When `ASubGameMode` spawns the `ASubCrewCharacter`, it must immediately call `CrewMov->InitializeForSubmarine()` and populate `GridSpaceTransform` so the first CMC tick knows exactly where the floor is.

> **Decision**: We do NOT use standard UE `PlayerStart` nodes floating in the void. We place `PlayerStart` nodes as Child Components *inside* the `BP_Submarine` hierarchy, or we spawn players based on `UCompartmentVolumeComponent` locations.
