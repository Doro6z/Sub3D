Le bon découpage pour Sub3D :

USubFloodComponent
= vérité gameplay
= niveaux d’eau, volumes, portes ouvertes, débit, égalisation

Niagara Fluid
= représentation visuelle locale du flux
= intensité, direction, vitesse, mousse, splash

Ton document est globalement sur la bonne voie : partir de Grid3D_FLIP_Hose, réduire fortement le grid, exposer FlowIntensity01 et FlowDirection, et éviter le hard-pop avec une désactivation différée sont de bonnes décisions.
Niagara Fluids fournit justement des templates temps réel pour fluides, dont des setups de type FLIP adaptés aux liquides dynamiques, donc Grid3D_FLIP_Hose est un meilleur point de départ que Shallow Water ou un simple sprite emitter pour ce cas.

Architecture recommandée
1. Gameplay : calculer le flux côté flood system

Tu dois calculer un état visuel simple :

struct FDoorWaterFlowVisualState
{
    bool bShouldShowFlow = false;

    float DeltaHeightCm = 0.0f;      // SurfaceA - SurfaceB
    float AbsDeltaHeightCm = 0.0f;

    float FlowIntensity01 = 0.0f;    // 0 = rien, 1 = violent
    FVector FlowDirectionWorld;      // de compartiment haut vers compartiment bas

    float SourceSpeedCmS = 0.0f;
};

Le calcul minimal :

float DeltaH = WaterSurfaceA_Z - WaterSurfaceB_Z;
float AbsDeltaH = FMath::Abs(DeltaH);

if (!bDoorOpen || AbsDeltaH < StopThresholdCm)
{
    FlowIntensity01 = 0.0f;
}
else
{
    FVector Direction = DeltaH > 0.0f ? DirAtoB : -DirAtoB;

    float Raw01 = FMath::GetMappedRangeValueClamped(
        FVector2D(5.0f, 120.0f),   // 5 cm = début visible, 120 cm = flux très fort
        FVector2D(0.0f, 1.0f),
        AbsDeltaH
    );

    FlowIntensity01 = Raw01 * Raw01 * (3.0f - 2.0f * Raw01); // smoothstep
    SourceSpeedCmS = FMath::Lerp(150.0f, 900.0f, FlowIntensity01);
}

Valeurs de départ réalistes pour ton prototype :

StartThresholdCm : 5 à 8 cm
StopThresholdCm  : 2 à 3 cm
MaxVisualDelta   : 100 à 150 cm
SourceSpeed      : 150 → 900 cm/s

Ne cherche pas une simulation hydraulique parfaite au début. Le joueur doit lire :

gros delta = gros jet
delta faible = flux calme
égalité = arrêt progressif
2. Niagara : créer un asset spécialisé

Crée :

Content/Sub3D/VFX/NS_DoorWaterCascade

À partir de :

Niagara Fluids / Grid3D_FLIP_Hose

Ne modifie pas le template plugin directement.

Réglages de base
Paramètre	Valeur recommandée
Grid Resolution	24 pour prototype, 32 max pour hero shot
Domain Size	environ 250 × 180 × 220 cm
Particle Lifetime	0.8 à 1.4 s
Local Space	true
Fixed Bounds	true
Gravity	980 cm/s²
Renderer	Mesh Renderer + matériau Single Layer Water
Foam	sprite emitter léger, 10–30% du rate principal

La résolution est critique : le coût d’un grid 3D monte très vite avec la résolution. Ton doc propose 24³, ce qui est cohérent pour une cascade courte de porte, mais il faudra profiler en PIE, pas dans le preview Niagara.

3. User Parameters Niagara à exposer

Expose au minimum :

User.FlowIntensity01     float
User.FlowDirectionLocal  Vector
User.SourceSpeedCmS      float
User.SourceWidthCm       float
User.SourceHeightCm      float

Tu peux commencer avec seulement :

User.FlowIntensity01
User.FlowDirectionLocal
User.SourceSpeedCmS

Puis tu ajoutes la taille de source plus tard.

Bindings conseillés

Dans le Niagara System :

Spawn Rate =
MaxSpawnRate * pow(User.FlowIntensity01, 1.35)

Exemple :

MaxSpawnRate = 6000 à 9000

Velocity :

Velocity =
normalize(User.FlowDirectionLocal) * User.SourceSpeedCmS
+ small random cone spread

Foam :

FoamSpawnRate =
MainSpawnRate * lerp(0.10, 0.30, User.FlowIntensity01)

Density / thickness :

DensityMultiplier =
lerp(0.25, 1.0, User.FlowIntensity01)

Le point important : ne pilote pas seulement le spawn rate. Sinon, à faible intensité, tu obtiens juste moins de particules, pas un vrai flux plus fin. Il faut piloter à la fois :

spawn rate
velocity
density/thickness
foam amount
4. Placement dans la porte

Pour un premier système fiable :

NiagaraComponent attaché à la porte
Local X = axe de traversée de la porte
Local Z = vertical
Source position = bas / milieu de l’ouverture

MVP :

Source = rectangle horizontal dans le bas de la porte

Ne cherche pas encore à faire une ouverture parfaitement submergée sur toute la hauteur. Ça viendra après.

Logique simple :

FVector FlowDirectionLocal =
    DoorComponent->GetComponentTransform()
    .InverseTransformVectorNoScale(FlowDirectionWorld)
    .GetSafeNormal();

Puis :

NiagaraComp->SetVariableFloat(TEXT("User.FlowIntensity01"), SmoothedIntensity);
NiagaraComp->SetVariableVec3(TEXT("User.FlowDirectionLocal"), FlowDirectionLocal);
NiagaraComp->SetVariableFloat(TEXT("User.SourceSpeedCmS"), SourceSpeedCmS);
5. Lifecycle : surtout pas de hard pop

Ton document identifie correctement le risque : appeler Deactivate() + SetVisibility(false) dès que le flux passe sous threshold va créer un pop visuel.

Le pattern propre :

Flux actif :
    Activate si besoin
    Set FlowIntensity01 > 0

Flux sous threshold :
    garder le component actif
    Set FlowIntensity01 = 0
    laisser les particules mourir naturellement

Après 1.5–2.0 secondes sans flux :
    DeactivateImmediate

Pseudo-code :

void UDoorFloodVfxComponent::UpdateCascade(float DeltaTime, const FDoorWaterFlowVisualState& State)
{
    const bool bHasVisualFlow = State.FlowIntensity01 > 0.02f;

    if (bHasVisualFlow)
    {
        LastActiveFlowTime = GetWorld()->GetTimeSeconds();

        if (!NiagaraComp->IsActive())
        {
            NiagaraComp->Activate(true);
        }
    }

    const float TargetIntensity = bHasVisualFlow ? State.FlowIntensity01 : 0.0f;

    const float InterpSpeed = TargetIntensity > CurrentIntensity ? 10.0f : 4.0f;

    CurrentIntensity = FMath::FInterpTo(
        CurrentIntensity,
        TargetIntensity,
        DeltaTime,
        InterpSpeed
    );

    NiagaraComp->SetVariableFloat(TEXT("User.FlowIntensity01"), CurrentIntensity);

    if (NiagaraComp->IsActive())
    {
        NiagaraComp->SetVariableVec3(TEXT("User.FlowDirectionLocal"), State.FlowDirectionWorld);
        NiagaraComp->SetVariableFloat(TEXT("User.SourceSpeedCmS"), State.SourceSpeedCmS);
    }

    const float TimeSinceFlow = GetWorld()->GetTimeSeconds() - LastActiveFlowTime;

    if (!bHasVisualFlow && TimeSinceFlow > 2.0f)
    {
        NiagaraComp->DeactivateImmediate();
    }
}

À adapter : FlowDirectionWorld doit être converti en local avant d’être envoyé si ton Niagara est en Local Space.

6. Collision : ne mise pas tout dessus

Pour un flux de porte, la collision FLIP parfaite avec le sol et les murs est un bonus, pas le socle du système.

À faire :

1. D’abord : flux lisible sans collision parfaite.
2. Ensuite : collision sol/murs via distance fields / collision setup.
3. Si instable : ajouter un kill plane ou une courte lifetime.
4. Ajouter mousse/splash au sol pour vendre l’impact.

Niagara peut utiliser des collisions GPU via scène/depth/global distance field selon les modules et setups, mais c’est un point fragile à valider sur ton asset exact, surtout avec des meshes intérieurs générés/procéduraux.

Pour Sub3D, je recommande ce compromis :

Le FLIP donne le volume d’eau en mouvement.
Les sprites de mousse donnent l’impact au sol.
Le vrai niveau d’eau du compartiment reste ton plane / mesh de flood gameplay.
7. Règle de budget

Ne mets pas une simulation FLIP par micro-porte sans contrôle.

Implémente un manager ou une limite simple :

MaxSimultaneousDoorCascades = 2 à 4

Priorité :

1. Porte proche caméra
2. Porte dans le compartiment du joueur
3. Plus gros delta de hauteur
4. Porte visible

Si plus de portes actives :

Portes prioritaires : Niagara Fluid
Portes secondaires : sprite/mesh fallback
Portes lointaines : rien ou simple foam decal
Implémentation MVP en 6 étapes
Étape 1 — Créer le Niagara asset
Duplicate Grid3D_FLIP_Hose
→ Content/Sub3D/VFX/NS_DoorWaterCascade

Réglages :

Grid Resolution = 24
Domain Size = 250 × 180 × 220 cm
Local Space = true
Fixed Bounds = true
Particle Lifetime = 1.0 s
Étape 2 — Ajouter les User Parameters
FlowIntensity01
FlowDirectionLocal
SourceSpeedCmS
Étape 3 — Binder les paramètres
SpawnRate = 8000 * pow(FlowIntensity01, 1.35)
Velocity = normalize(FlowDirectionLocal) * SourceSpeedCmS
FoamRate = SpawnRate * 0.2
Density = lerp(0.25, 1.0, FlowIntensity01)
Étape 4 — C++ côté porte

Créer ou refactor :

UDoorFloodVfxComponent

Responsabilité :

- recevoir DeltaHeight / FlowDirection depuis USubFloodComponent
- convertir en FlowIntensity01
- smoother l’intensité
- envoyer les User Parameters Niagara
- gérer activation / drain / DeactivateImmediate
Étape 5 — Test map dédiée

Créer une map de test avec :

Compartment A : niveau eau haut
Door
Compartment B : niveau eau bas
Debug text : DeltaH, Intensity, SourceSpeed, Niagara active

Tu dois valider ces cas :

DeltaH = 0 cm      → pas de FX
DeltaH = 10 cm     → petit ruissellement
DeltaH = 50 cm     → flux clair
DeltaH = 120 cm    → jet violent
Door close         → arrêt progressif, pas de pop
Submarine moving   → FX reste attaché à la porte
Étape 6 — Profiling

En PIE :

stat GPU
stat Niagara
Niagara Debugger

Critère GO / NO-GO :

1 cascade active  : acceptable
2 cascades actives: acceptable
4 cascades actives: à profiler strictement
Verdict sur ton doc

Ton doc est bon comme base R&D, mais je le durcirais ainsi pour la prod :

À garder
- Grid3D_FLIP_Hose comme base
- Grid Resolution 24
- Local Space true
- FlowIntensity01
- FlowDirection
- désactivation différée au lieu de hard pop
- gameplay flood autoritaire séparé du FX
À corriger / simplifier
- Ne pas compter sur Niagara pour résoudre l’égalisation.
- Ne pas rendre la collision FLIP obligatoire pour le MVP.
- Ne pas multiplier les cascades sans manager de budget.
- Ne pas exposer trop de paramètres au début.
- Ne pas viser une simulation d’eau physiquement exacte : viser une lecture gameplay forte.
Version cible simple
USubFloodComponent calcule :
    DeltaHeight
    Direction
    Intensity

UDoorFloodVfxComponent affiche :
    Niagara Fluid local à la porte
    intensité smoothée
    drain naturel
    arrêt propre

Niagara asset fait :
    jet/cascade court
    mousse légère
    lifetime courte
    bounds fixes

C’est le bon niveau d’ambition pour ton First Playable : visuellement fort, lisible, piloté par le gameplay, sans laisser Niagara devenir le cœur du système de flood.