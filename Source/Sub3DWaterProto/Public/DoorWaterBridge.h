#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "DoorWaterBridge.generated.h"

class URoomWaterRenderer;
class UProceduralMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInterface;
class ASubDoorActor;

/**
 * Composant attaché à une ASubDoorActor (ou tout Actor positionné à l'embrasure d'une porte
 * entre deux compartiments). Pilote un mesh de "raccord d'eau" + un Niagara de courant
 * quand la porte est ouverte et qu'il y a un delta de niveau entre les deux salles.
 *
 * Lit l'état de la porte par polling de bClosed sur ASubDoorActor parent à chaque tick
 * (pas d'écoute de delegate pour rester découplé). Si l'Actor parent n'est pas une porte,
 * l'état "ouvert" peut être piloté manuellement via OnDoorStateChanged depuis BP.
 *
 * Convention transform : la porte est en yaw-only (X = "à travers la porte", Y = largeur,
 * Z = vertical world). Pour un sub avec pitch/roll, un override SetWorldRotation est nécessaire.
 */
UCLASS(ClassGroup = (Sub3DWaterProto), meta = (BlueprintSpawnableComponent,
    PrioritizeCategories = "Bridge"))
class SUB3DWATERPROTO_API UDoorWaterBridge : public USceneComponent
{
    GENERATED_BODY()

public:
    UDoorWaterBridge();

    /**
     * Override manuel du compartiment côté A. Utilisé seulement si l'Actor parent n'est PAS un
     * ASubDoorActor (testing standalone). Si parent est une porte, ce champ est ignoré et la
     * résolution se fait automatiquement via ASubDoorActor::CompartmentA. Laisser None par défaut.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Resolution")
    FName CompartmentAOverride = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Resolution")
    FName CompartmentBOverride = NAME_None;

    /**
     * Renderers résolus par lookup FName (RoomId match) au BeginPlay. Lecture seule.
     * En portage Sub3D : ce lookup deviendra un appel sur le manager du sub (qui maintient
     * une map CompartmentId → URoomWaterRenderer). Ici on walke TActorIterator<ARoomActor>.
     */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bridge|Resolved")
    TWeakObjectPtr<URoomWaterRenderer> RendererA;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bridge|Resolved")
    TWeakObjectPtr<URoomWaterRenderer> RendererB;

    /** Matériau du raccord d'eau au niveau de l'embrasure. M_CompartmentWater convient. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
    TObjectPtr<UMaterialInterface> WaterMaterial;

    /** NiagaraSystem du flux de courant (ex: NS_DoorFlow). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge")
    TObjectPtr<UNiagaraSystem> FlowEffect;

    /** Largeur de l'embrasure en cm (axe Y local de la porte). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge",
        meta = (ClampMin = "10.0", ClampMax = "500.0"))
    float DoorWidth = 100.f;

    /** Profondeur de l'embrasure en cm (axe X local de la porte, "à travers"). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge",
        meta = (ClampMin = "5.0", ClampMax = "200.0"))
    float DoorDepth = 30.f;

    /** Hauteur de l'embrasure en cm (info pour le clamp visuel, axe Z). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge",
        meta = (ClampMin = "10.0", ClampMax = "500.0"))
    float DoorHeight = 200.f;

    /** Delta minimal entre les deux niveaux (cm) pour déclencher le FX de courant. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge",
        meta = (ClampMin = "0.1"))
    float MinDeltaForFlowFx = 10.f;

    /** Force d'injection appliquée aux deux salles à l'ouverture de la porte (slosh). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge",
        meta = (ClampMin = "0.0"))
    float OpenSloshForce = 3.0f;

    /** Force d'injection continue côté arrivée du courant quand la porte coule. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge",
        meta = (ClampMin = "0.0"))
    float FlowReceiverForce = 0.5f;

    /**
     * Notification d'état porte. Appelée automatiquement par le polling de l'ASubDoorActor
     * parent. Peut aussi être appelée manuellement depuis BP si la porte n'est pas un
     * ASubDoorActor (testing standalone).
     */
    UFUNCTION(BlueprintCallable, Category = "Bridge")
    void OnDoorStateChanged(bool bClosed);

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* TickFunc) override;

private:
    UPROPERTY()
    TObjectPtr<UProceduralMeshComponent> BridgeMesh;

    UPROPERTY()
    TObjectPtr<UNiagaraComponent> FlowFxComp;

    /** Cache du parent ASubDoorActor pour le polling de bClosed. Null si parent != door. */
    TWeakObjectPtr<ASubDoorActor> CachedDoor;

    /** État interne synchronisé via OnDoorStateChanged. */
    bool bDoorOpen = false;

    /** Dernière valeur observée de bClosed pour détecter les transitions. */
    bool bLastObservedClosed = true;

    /** Initial frame guard pour appliquer l'état de départ une fois. */
    bool bInitialStateApplied = false;

    /** Crée BridgeMesh + FlowFxComp si pas encore là (idempotent). */
    void EnsureSubComponents();

    /**
     * Résout RendererA/B par lookup FName.
     *  - Si parent ASubDoorActor : utilise CompartmentA/B de la porte (auto, zero-config).
     *  - Sinon : utilise CompartmentAOverride/BOverride.
     *  - Walke TActorIterator<ARoomActor> pour matcher RoomId → ARoomActor.WaterRenderer.
     * Log un warning si non résolu.
     */
    void ResolveRenderers();

    /** Polling de bClosed sur l'ASubDoorActor parent. Trigger OnDoorStateChanged à transition. */
    void PollDoorState();

    /** Recalcule la position et la visibilité du BridgeMesh selon les niveaux d'eau courants. */
    void UpdateBridgeMesh();

    /** Module la vitesse + direction du Niagara, ou le désactive si delta trop faible. */
    void UpdateFlowFx();

    /** World Z du niveau d'eau d'un renderer (component world Z + waterLocalZ). */
    static float GetRendererWaterWorldZ(const URoomWaterRenderer* Renderer);
};
