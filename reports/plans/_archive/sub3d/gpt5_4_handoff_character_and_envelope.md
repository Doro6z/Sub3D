# Handoff Package for GPT-5.4: Character Control & Single Hull Envelope

Ce document liste les fichiers essentiels (code et documentation) à transmettre à **Codex / GPT-5.4** pour qu'il ait tout le contexte nécessaire à la refonte de votre système de personnage (collisions, FPS feel, support du sol) et de l'enveloppe du sous-marin (single hull, abandon du double hull brouillon).

---

## 1. Documentation d'Architecture (Règles "Proto03" Absolues)

Pour que GPT-5.4 ne s'égare pas dans des architectures inadaptées (comme le full mesh-destruction ou le voxel), il faut **absolument** lui donner ces documents en premier :

- `reports/plans/sub3d/2026-03-21_sub3d_proto_program_and_proto03_execution_handoff.md`
- `reports/adrs/sub3d/20260320-1538-sub3d-proto03-single-hull-and-dynamic-breach-truth.md`
- `reports/session-logs/20260320-1538-sub3d-single-hull-breach-architecture-lock.md`

*(Rappelez-lui la règle d'or: "Single Hull, USubHullComponent is the runtime truth, character is just a probe").*

---

## 2. Pôle "Subcrew & Contrôles" (Mouvement, FPS Feel, Collisions)

Ces fichiers contiennent la logique actuelle du personnage qui traverse le sous-marin. C'est ici que se trouve le problème de "support sur le sol" et le "feel FPS".

- `Source/Sub3D/Submarine/SubCrewCharacter.h` et `.cpp`
  *(Pour l'analyse de la capsule de collision, du setup de la caméra "FPS", et de l'enveloppe physique du crew).*
- `Source/Sub3D/Submarine/SubCrewMovementComponent.h` et `.cpp`
  *(C'est ici qu'il faut analyser pourquoi le Subcrew n'est pas supporté par le sol du sous-marin. Il faut probablement gérer la notion de "Relative/Moving Base" ou un root motion custom).*
- `Source/Sub3D/Submarine/SubPlayerController.h` et `.cpp`
  *(Où les inputs sont gérés, important pour le passage propre des commandes FPS vers le mouvement).*

---

## 3. Pôle "Enveloppe & Coque" (Placement, Single Hull, Vérité Physique)

Pour faire évoluer le prototype vers une "vérité d'enveloppe propre" sans double coques bizarres, GPT-5.4 doit reconcevoir l'interfaçage de ces classes :

- `Source/Sub3D/Submarine/SubmarineBase.h` et `.cpp`
  *(L'acteur central. GPT doit vérifier comment les components statiques et le mask visuel de collision sont attachés).*
- `Source/Sub3D/Submarine/SubHullComponent.h` et `.cpp`
  *(La **vraie et unique** source de vérité du statut de la coque. Gère les dégâts et les brèches selon le standard Proto03).*
- `Source/Sub3D/Submarine/StructuralHullTypes.h`
  *(Définitions des structs, crucial pour que GPT restructure proprement la grille de dommages 2D sans tout casser).*
- `Source/Sub3D/Submarine/SubmarineCompartmentComponent.h` et `.cpp`
  *(Pour la logique d'espace intérieur propre et de flooding).*
- `Source/Sub3D/Submarine/SubmarineLayoutAsset.h`
  *(DataAsset optionnel à passer s'il doit redéfinir comment la forme du sous-marin est nourrie par la data plutôt que par le mesh).*

---

## Instruction de Prompt Suggérée pour accompagner les fichiers

> *"Voici le codebase actuel pour mon projet Sub3D. L'objectif immédiat (Proto03) est strictement d'obtenir un **Single Hull** robuste (pas de découpages de coque bizarres) et de corriger les collisions pour que le personnage (`SubCrewCharacter`) aie un **vrai "FPS feel"** avec des collisions propres sur le sol du sous-marin en mouvement.*
> *Lis attentivement les documents d'architecture fournis pour comprendre les contraintes. Base-toi sur `USubHullComponent` comme vérité absolue. Ton but est de proposer une refonte d'architecture C++ pour ces classes, et d'identifier pourquoi le terrain ne supporte pas actuellement le `SubcrewMovementComponent`."*
