#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RoomWaterBakedData.generated.h"

/**
 * Une slice horizontale du compartiment échantillonnée à une hauteur Z donnée.
 * Stocke un Signed Distance Field 2D (un float par cellule) plutôt qu'un mask binaire :
 *   - négatif = intérieur du sub, magnitude = distance cm à la surface la plus proche
 *   - positif = extérieur, magnitude = distance cm à la surface la plus proche
 *   - 0 = sur la surface
 *
 * Ce signed distance est calculé au bake par : (1) parity raycast vertical pour le signe,
 * (2) 8 raycasts XY cardinaux+diagonaux pour la magnitude. Permet à Marching Squares de placer
 * le contour entre cellules à la position interpolée du zéro (sub-cell precision), au lieu du
 * stair-step de cellule entière du mask binaire.
 */
USTRUCT(BlueprintType)
struct FCompartmentSlice
{
    GENERATED_BODY()

    /** Hauteur Z (en local au CompartmentVolume) à laquelle cette slice a été échantillonnée. */
    UPROPERTY(VisibleAnywhere)
    float SliceZ_Local = 0.0f;

    UPROPERTY(VisibleAnywhere)
    int32 GridWidth = 0;

    UPROPERTY(VisibleAnywhere)
    int32 GridHeight = 0;

    /** Signed distance field. Taille = GridWidth * GridHeight. Convention : négatif = intérieur. */
    UPROPERTY(VisibleAnywhere)
    TArray<float> SignedDistance;

    /** Polygone du contour extrait par Marching Squares (paires de points = segments). En local space 2D, plan XY. */
    UPROPERTY(VisibleAnywhere)
    TArray<FVector2D> ContourPolygon;
};

/**
 * Mesh procédural pré-calculé. Vertices stockés à Z=0 ; le runtime translate via SetRelativeLocation
 * pour positionner la nappe d'eau à la hauteur courante.
 */
USTRUCT(BlueprintType)
struct FCachedWaterMesh
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere)
    TArray<FVector> Vertices;

    UPROPERTY(VisibleAnywhere)
    TArray<int32> Triangles;

    UPROPERTY(VisibleAnywhere)
    TArray<FVector> Normals;

    UPROPERTY(VisibleAnywhere)
    TArray<FVector2D> UV0;
};

/**
 * Résultat du bake d'un compartiment : stack de silhouettes par hauteur, cap meshes pré-générés
 * (un par slice), liste des segments d'ouverture pour le skirt.
 */
UCLASS(BlueprintType)
class SUB3DWATERPROTO_API URoomWaterBakedData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, Category = "Bake Source")
    FName SourceRoomId;

    /** Coin min du BoxComponent en local (au moment du bake). */
    UPROPERTY(VisibleAnywhere, Category = "Bake Source")
    FVector LocalBoundsMin = FVector::ZeroVector;

    /** Coin max du BoxComponent en local (au moment du bake). */
    UPROPERTY(VisibleAnywhere, Category = "Bake Source")
    FVector LocalBoundsMax = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, Category = "Slices")
    TArray<FCompartmentSlice> Slices;

    /** Cap meshes par slice (Z=0 dans le template, translaté au runtime). */
    UPROPERTY(VisibleAnywhere, Category = "Cached Meshes")
    TArray<FCachedWaterMesh> CapMeshesPerSlice;

    /** Segments d'ouverture détectés (proches du bord du Box) pour le skirt. */
    UPROPERTY(VisibleAnywhere, Category = "Openings")
    TArray<FVector2D> OpeningSegmentStarts;

    UPROPERTY(VisibleAnywhere, Category = "Openings")
    TArray<FVector2D> OpeningSegmentEnds;
};
