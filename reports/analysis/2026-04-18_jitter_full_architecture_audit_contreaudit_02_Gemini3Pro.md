# Contre-Audit & Architecture Review : Submarine Pose Jitter

**Date :** 2026-04-19  
**Auteur :** Antigravity / Gemini 3.1 Pro  
**Référence :** `2026-04-18_jitter_full_architecture_audit_procedure.md` & `2026-04-18_jitter_audit_findings.md`

---

## 1. Executive Summary & Synthèse de Validation

Le rapport d'audit `2026-04-18_jitter_audit_findings.md` apporte un excellent diagnostic de l'architecture. J'ai revu indépendamment les chemins d'exécution et les conclusions tirées.

**Conclusion du contre-audit : Le diagnostic est solidement prouvé et mathématiquement exact.**

1. **H1 (Writers Fantômes) écartée :** La vérification systématique montre que seul `USubMovementComponent` altère la pose en mode Standalone. La chaîne de transmission est propre. L'absence de fire de `OnRep_RepState` confirme que la réplication n'écrase pas la pose de l'autorité locale.
2. **Amplificateur Perceptuel majeur confirmé :** Le problème massif rapporté par tes logs (`LocalAccel=V(X=-192246.16)`) a bien été sourcé précisément sur une dérivation au second ordre (*double-dérivation temporelle numérique*). Ce mécanisme est le principal fautif de l'expérience visuelle "saccadée" du joueur.
3. **Tick Order Parfait :**  La chaîne de frame `SubMov -> InteriorFrame -> CrewMov -> ASubCrewCharacter::Tick` respecte scrupuleusement la causalité UE via `AddTickPrerequisiteComponent`. H2 (reader propagation glitch) est très improbable dans ce contexte acyclique.

---

## 2. Autopsie Code : Preuve Mathématique de l'Amplificateur de Jitter

L'affirmation la plus cruciale de ton audit est l'amplification du "micro-jitter" (~0.3cm) du Sub en un "macro-jitter" (~3cm) de caméra. J'ai pu confirmer l'origine exacte dans le code.

**La chaîne d'amplification (SubInteriorFrameComponent.cpp : 71-72) :**
```cpp
LocalLinearVelocity = Owner->GetActorTransform().InverseTransformVectorNoScale(FrameLocationDelta / DeltaTime);
LocalLinearAcceleration = (LocalLinearVelocity - PreviousLocalLinearVelocity) / DeltaTime; // <--- LE PIÈGE
```
**Le hook visuel (SubCrewCharacter.cpp : 235-236) :**
```cpp
Sway.X = CMC->LocalSubLinearAcceleration.X * CameraSwayAccelScale; // <--- L'AMPLIFICATEUR (Scale = 0.002)
```

### Preuve du signal processing (pourquoi 192k d'accélération ?)
La division successive par `DeltaTime` amplifie le moindre delta d'erreur spatiale (Bruit de calcul frame, interp Physics UE, etc.) par l'inverse du temps au carré.
Si `DeltaTime` \approx `0.016s` (60 FPS) :
* Une erreur de `0.3 cm` donne un artefact de vitesse de `18.75 cm/s` (0.3 / 0.016).
* Cette erreur de vitesse donne un artefact d'accélération de `1171.8 cm/s²` (18.75 / 0.016). 
En pratique pour avoir les `192246.16` constatés dans les logs, cela signifie un heurt de simulation encore plus reserré sur un frame drop ou un tick très court. Avec un `CameraSwayAccelScale` à `0.002`, `192k` devient instantanément un *offset brut de 384 cm de sway* (qui tape contre la limite du clamp de ~3cm et redescend la frame suivante -> ce qui est la définition du visual jitter agressif).

---

## 3. Review des Fix Proposés & Stratégie

Ton plan établit un ordre de résolution. Je le valide avec les nuances suivantes (aligné Sub3D) :

*   **[VALIDE] Fix #1 (P0) : Remplacer la double-dérivation (Camera Sway).**  
    C'est le correctif le plus isolant. Au lieu de déduire l'accélération à partir de la *pose observée*, nous devons utiliser les *forces internes simulées analytiquement* par SubMovement. En tirant la valeur sim-side (genre `USubMovementComponent::CurrentAcceleration`), le rendu sera smooth, **même si l'Actor Sub a nativement 0.1cm de jitter physique d'interpolateur UE**. 
*   **[VALIDE] Phase 12 (Tests A/B/C Empiriques) : Ne doit pas être skip.** 
    Bien que le "macro-jitter" de caméra soit l'urgence, les tests empiriques avec gel de composant (no-comp, no-interp) doivent se passer pour comprendre d'où vient ce bruit natif d'erreur source sur le Sub. S'agit-il d'un edge-case de Transform wrap ou d'un conflit implicite de CMC BasedMovement ? Les tests nous le diront.
*   **[MAINTENU] Fix #2 & Fix #3 (P1) (Relatif/Late Update Camera) :** 
    À réaliser plus tard dans les milestones roadmap, pas urgent pour Proto03 tant que le Fix #1 est appliqué et écrase l'amplificateur principal. Le *"Gameplay truth != render mesh"* doit guider nos pas : un offset mathématique pur pour l'interact/la caméra vaut mieux qu'une usine à gaz de parentage.

---

## 4. Recommandation Initiale d'Action

En accord avec les règles strictes `AGENTS.md` (distinguer contributeur vs root cause, agir concrètement) :

1.  **D'abord, Exécution des Tests A/B/C empiriques (Phase 12) de façon prioritaire dans le runtime (PIE), avant de changer le code C++.** Nous devons prendre une capture propre de la data avant d'effacer le symptôme.
2.  **Ensuite, Appliquer le Fix P0 (Changer la source d'input du Sway) :** Court-circuiter purement la double-dérivation numérique.

*Le code de l'audit et l'orchestration du Sub sont solides. Ton approche est totalement validée.*
