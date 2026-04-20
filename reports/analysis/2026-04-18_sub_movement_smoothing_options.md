# Submarine Movement Smoothing Options — 2026-04-18

Référence d'autorité projet : [reports/plans/2026-04-10_first_playable_strategic_analysis.md](/C:/Dev/Sub3D/reports/plans/2026-04-10_first_playable_strategic_analysis.md)

## Objet

Lister les options concrètes pour réduire voire éliminer la saccade du sous-marin :
- en standalone
- côté client distant
- en priorisant le feeling et la stabilité visuelle
- sans contrainte forte anti-cheat

Cette note ne propose pas une refonte réseau complète. Elle borne les changements au path actuel de `USubMovementComponent`.

---

## Constat actuel

Le test PIE collision sur `MovementCollisionProxy` est bon. Le sous-marin bloque bien contre le world.

Le problème observé est :
- à vitesse élevée, le sous-marin saccade quand il est en contact avec un obstacle
- la saccade est visible aussi quand le sub pousse en `forward` contre un mesh et commence à pivoter au rudder
- en standalone, ce n'est pas un problème de réplication client. Le path actif est le path authority local

Code concerné :
- simulation authority + extrapolation visuelle locale : [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:105)
- résolution de collision / slide : [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:384)
- interpolation client remote : [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:643)
- état répliqué actuel : [SubmarineRuntimeTypes.h](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineRuntimeTypes.h:88)

---

## Diagnostic

### Likely root cause

Le path de contact actuel produit un mouvement visuellement discontinu :

1. simulation fixed-step authority
2. détection de hit
3. `advance to hit`
4. `depenetration`
5. `slide` appliqué sans second sweep
6. rotation appliquée ensuite sans test de clearance
7. extrapolation visuelle authority entre deux fixed steps

Les points les plus critiques sont :
- `SlideDelta` appliqué sans re-sweep : [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:478)
- rotation appliquée après la résolution de contact, sans gérer le cas "je tourne alors que je suis déjà appuyé sur un obstacle" : [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:512)
- extrapolation visuelle authority même quand on est en contact : [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:176)

### Possible contributor

Le fixed-step actuel est `60 Hz` : [SubMovementComponent.h](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.h:172)

Ce n'est probablement pas la cause principale. À `20 km/h`, le déplacement par step reste modéré. Augmenter la fréquence seule peut réduire un peu le problème, mais ne corrige pas une mauvaise résolution de contact.

### Deferred concern

Le path client remote réplique un snapshot de mouvement simple, puis :
- interpole jusqu'au snapshot
- extrapole ensuite avec la dernière vitesse reçue

Réfs :
- [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:643)
- [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:722)

Ce path sera plus propre une fois le path authority local stabilisé. Le traiter avant la physique locale risque de lisser un bug au lieu de le corriger.

---

## Principe de décision

Le projet vise ici du coop chill. La priorité est :

1. éviter la saccade visible
2. garder un feedback de collision crédible
3. accepter un peu plus de lag visuel remote si cela supprime le jitter
4. ne pas introduire de système lourd de prédiction/rollback

Conclusion :
- on privilégie une présentation stable à une exactitude stricte
- côté client distant, un peu plus d'interpolation et un peu moins d'extrapolation sont acceptables

---

## Options

## Option A — Désactiver l'extrapolation visuelle authority pendant le contact

### Changement

Dans le path authority local :
- si `bHadBlockingHit == true`, ne pas appliquer l'extrapolation visuelle en fin de tick
- garder l'acteur à la pose autoritative du dernier fixed step

Code concerné :
- `bHadBlockingHit` est calculé dans [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:395)
- l'extrapolation authority est faite dans [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:176)

### Effet attendu

- réduit fortement le sawtooth visuel quand le sub est appuyé contre une paroi
- améliore immédiatement le standalone
- améliore aussi les snapshots server envoyés aux clients, car la pose présentée localement n'oscille plus

### Risque

Faible. Patch local. Aucun changement de contrat réseau.

### Priorité

P1

---

## Option B — Remplacer le slide sans re-sweep par un slide itératif avec re-sweep

### Changement

Remplacer le path actuel :
- `advance`
- `depenetration`
- `slide once without sweep`

par un path borné, par exemple `2` ou `3` itérations max :
- sweep
- avance jusqu'au hit
- projection du reste sur le plan de contact
- re-sweep du reste

Code concerné :
- [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:464)

### Effet attendu

- réduit le chatter contre les bords
- réduit les petits passages "je rentre / je sors / je recolle" d'un frame à l'autre
- améliore le comportement de glisse le long du mur

### Risque

Moyen.
- un peu plus de coût CPU
- nécessite de bien borner le nombre d'itérations
- demande un test PIE sur mur simple, angle rentrant, angle sortant, route mesh

### Priorité

P1

---

## Option C — Rendre la rotation contact-aware

### Changement

Quand un contact bloquant persiste :
- amortir `YawRateDegPerSec`
- ou réduire la rotation appliquée ce tick
- ou tester la rotation prévue et la limiter si elle crée une pénétration

Code concerné :
- calcul yaw : [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:283)
- application rotation : [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:512)

### Effet attendu

- supprime une grosse partie de la saccade "full thrust + mur + rudder"
- garde le comportement voulu de pivot, mais évite le couple "rotation dans l'obstacle puis depenetration"

### Risque

Moyen.
- tuning à faire pour ne pas tuer le feeling de pivot

### Priorité

P1

---

## Option D — Ajouter un flag de contact dans `FSubmarineNetState`

### Changement

Étendre [FSubmarineNetState](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineRuntimeTypes.h:88) avec au minimum :
- `bool bHadBlockingHit`

Optionnel si utile ensuite :
- `FVector_NetQuantizeNormal ContactNormal`

Le serveur remplit ce flag quand le sub est en contact au step courant. Le client l'utilise pour choisir un mode de présentation plus stable.

### Effet attendu

Permet au client distant de savoir :
- quand il doit arrêter d'extrapoler
- quand il doit rallonger légèrement la durée d'interpolation
- quand il doit amortir les micro-corrections

### Risque

Faible à moyen.
- léger changement de payload réseau
- nécessite garder le struct coherent

### Priorité

P2

---

## Option E — Côté client distant, désactiver l'extrapolation pendant le contact

### Changement

Dans `InterpolateClient()` :
- si le dernier snapshot dit `bHadBlockingHit`
- rester en mode interpolation pure
- ne pas faire la phase 2 d'extrapolation par vitesse

Code concerné :
- [SubMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.cpp:750)

### Effet attendu

- moins de jitter côté observateur distant quand le sub racle un mur
- moins d'overshoot suivi de snap de correction

### Coût / tradeoff

- ajoute un peu de retard visuel
- mais dans ce contexte coop, ce tradeoff est favorable

### Priorité

P2

---

## Option F — Augmenter `NetUpdateFrequency`

### Changement

Augmenter le `NetUpdateFrequency` de `ASubmarineBase` au-dessus de la valeur actuelle.

Code concerné :
- constructor de `ASubmarineBase` dans [SubmarineBase.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubmarineBase.cpp:146)

### Effet attendu

- snapshots plus fréquents côté client
- moins de distance entre deux corrections réseau

### Limite

Ce n'est utile qu'après stabilisation du path authority.
Si le serveur produit déjà une pose "qui broute", envoyer cette pose plus souvent n'élimine pas le problème de base.

### Priorité

P2

---

## Option G — Augmenter `FixedSimulationHz`

### Changement

Monter `FixedSimulationHz` de `60` vers `90`, voire `120`.

Code concerné :
- [SubMovementComponent.h](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubMovementComponent.h:172)

### Effet attendu

- réduit la taille du step physique
- réduit l'écart d'extrapolation authority entre deux sim steps

### Limite

Ce n'est pas une solution principale. Si le slide et la rotation au contact restent brutaux, le problème devient simplement plus fin.

### Priorité

P3

---

## Option H — Camera smoothing uniquement

### Changement

Ajouter du smoothing sur la vue crew/caméra sans corriger la physique sous-jacente.

Code connexe :
- [SubCrewMovementComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubCrewMovementComponent.cpp:582)
- [SubInteriorFrameComponent.cpp](/C:/Dev/Sub3D/Source/Sub3D/Submarine/SubInteriorFrameComponent.cpp:43)

### Effet attendu

Peut masquer une partie de la saccade pour l'occupant.

### Limite

Ne corrige pas la vraie cause si le hull continue à chatter contre le mur.

### Priorité

P4

---

## Recommandation

Ordre recommandé :

1. Option A
2. Option B
3. Option C
4. Test PIE standalone
5. Test client distant
6. Option D
7. Option E
8. Option F
9. Option G seulement si nécessaire

Cette séquence est la plus cohérente avec le besoin actuel :
- éliminer la saccade d'abord à la source
- ensuite seulement lisser la présentation remote

---

## Recommandation ferme

Pour ce projet, la meilleure stratégie est :

- corriger la résolution de contact côté authority
- répliquer un peu plus d'information de contact
- rendre le client distant plus conservateur, avec moins d'extrapolation

Il ne faut pas :
- compter sur `FixedSimulationHz` seul
- compter sur la caméra seule
- lancer une refonte complète de prédiction client

---

## Proposition d'implémentation

### Passage 1

- couper l'extrapolation authority pendant `bHadBlockingHit`
- ajouter un debug log compact "contact / no contact"

Objectif :
- valider si la majorité du jitter disparaît déjà

### Passage 2

- remplacer le slide par un slide itératif `sweep -> advance -> resweep`
- borner à `2` itérations

Objectif :
- stabiliser le glissement contre paroi

### Passage 3

- réduire la rotation appliquée quand le sub est en contact

Objectif :
- supprimer la saccade "mur + rudder"

### Passage 4

- étendre `FSubmarineNetState` avec `bHadBlockingHit`
- désactiver l'extrapolation client pendant contact

Objectif :
- lisser le client distant avec un tradeoff assumé sur la latence visuelle

---

## Ce que cette note tranche

Oui, ces changements peuvent :
- augmenter la fiabilité perçue
- améliorer le feeling côté client distant
- réduire fortement la saccade
- voire l'éliminer si les options A + B + C sont bien faites

Le plus important est que la première moitié du gain ne vient pas du réseau.
Elle vient du fait que le serveur local doit d'abord produire un mouvement de contact propre.
