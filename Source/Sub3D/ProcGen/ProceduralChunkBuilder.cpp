#include "ProceduralChunkBuilder.h"
#include "ChunkLibrary.h"

TArray<UProceduralMeshComponent*> UProceduralChunkBuilder::BuildAll(
    const TArray<FChunkInstance>& Instances,
    const UChunkLibrary* Library,
    AActor* ParentActor) const
{
    TArray<UProceduralMeshComponent*> Result;
    if (!Library || !ParentActor) return Result;

    for (int32 i = 0; i < Instances.Num(); ++i)
    {
        const FChunkInstance& Instance = Instances[i];
        
        // Find template to get CarveParams
        const FChunkTemplate* Template = Library->Templates.FindByPredicate(
            [&](const FChunkTemplate& T) { return T.ChunkID == Instance.ChunkID; });

        if (Template)
        {
            UProceduralMeshComponent* Mesh = BuildChunk(Instance, Template->CarveParams, ParentActor, i);
            if (Mesh)
            {
                Result.Add(Mesh);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ProceduralChunkBuilder: Template not found for chunk %s"), *Instance.ChunkID.ToString());
        }
    }

    return Result;
}

UProceduralMeshComponent* UProceduralChunkBuilder::BuildChunk(
    const FChunkInstance& Instance,
    const FChunkCarveParams& Params,
    AActor* ParentActor,
    int32 SectionIndex) const
{
    TArray<FVector> Verts, Normals;
    TArray<int32> Tris;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    // 1. Generate Base Shape
    switch (Params.PrimaryShape)
    {
    case ECarveShape::Spheroid:
        GenerateSpheroid(Params, Instance.ChunkSeed, Verts, Tris, Normals, UVs);
        break;
    case ECarveShape::OvalTunnel:
    case ECarveShape::VerticalDrop:
    default:
        GenerateOvalTube(Params, Instance.ChunkSeed, Verts, Tris, Normals, UVs);
        break;
    }

    // 2. Apply Displacement (Passing WorldTransform and Params for continuity/fading)
    ApplySimplex(Verts, Normals, Instance.WorldTransform, Params, Instance.ChunkSeed);

    // 3. Create Component
    UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(ParentActor);
    PMC->RegisterComponent();
    PMC->AttachToComponent(ParentActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    
    // Set Transform
    PMC->SetWorldTransform(Instance.WorldTransform);

    // Create Mesh Section
    PMC->CreateMeshSection(SectionIndex, Verts, Tris, Normals, UVs, Colors, Tangents, true);
    
    // Collision Config
    PMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PMC->SetCollisionResponseToAllChannels(ECR_Block);
    
    // Material (Placeholder or default)
    // PMC->SetMaterial(0, ...);

    UE_LOG(LogTemp, Verbose, TEXT("ProceduralChunkBuilder: Built chunk %s [%d] — %d verts, %d tris"), 
        *Instance.ChunkID.ToString(), SectionIndex, Verts.Num(), Tris.Num() / 3);

    return PMC;
}

void UProceduralChunkBuilder::GenerateOvalTube(
    const FChunkCarveParams& Params, int32 Seed,
    TArray<FVector>& OutVerts, TArray<int32>& OutTris,
    TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs) const
{
    int32 R = Params.RadialSegments;
    int32 L = Params.LengthSegments;
    
    float HalfY = Params.BaseExtents.Y / 2.0f;
    float HalfZ = Params.BaseExtents.Z / 2.0f;

    // Vertices
    for (int32 i = 0; i <= L; ++i)
    {
        float X = (float)i * Params.BaseExtents.X / (float)L;
        for (int32 j = 0; j < R; ++j)
        {
            float Angle = (float)j * 2.0f * PI / (float)R;
            float cosA = FMath::Cos(Angle);
            float sinA = FMath::Sin(Angle);

            float Y = HalfY * cosA;
            float Z = HalfZ * sinA;

            OutVerts.Add(FVector(X, Y, Z));
            
            // Inward Normal: Normal to an ellipse (approx) pointing to center
            // Simplified: point towards X axis
            FVector Normal = FVector(0, -cosA, -sinA); 
            Normal.Normalize();
            OutNormals.Add(Normal);

            OutUVs.Add(FVector2D((float)i / (float)L, (float)j / (float)R));
        }
    }

    // Triangles (CCW from inside)
    for (int32 i = 0; i < L; ++i)
    {
        for (int32 j = 0; j < R; ++j)
        {
            int32 NextJ = (j + 1) % R;
            
            int32 v0 = i * R + j;
            int32 v1 = i * R + NextJ;
            int32 v2 = (i + 1) * R + j;
            int32 v3 = (i + 1) * R + NextJ;

            // Tri 1
            OutTris.Add(v0);
            OutTris.Add(v2);
            OutTris.Add(v1);

            // Tri 2
            OutTris.Add(v2);
            OutTris.Add(v3);
            OutTris.Add(v1);
        }
    }
}

void UProceduralChunkBuilder::GenerateSpheroid(
    const FChunkCarveParams& Params, int32 Seed,
    TArray<FVector>& OutVerts, TArray<int32>& OutTris,
    TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs) const
{
    int32 R = Params.RadialSegments;
    int32 L = Params.LengthSegments;

    float RadiusX = Params.BaseExtents.X / 2.0f;
    float RadiusY = Params.BaseExtents.Y / 2.0f;
    float RadiusZ = Params.BaseExtents.Z / 2.0f;

    // UV Sphere vertices
    for (int32 i = 0; i <= L; ++i)
    {
        float Phi = (float)i * PI / (float)L; // 0 to PI
        float sinP = FMath::Sin(Phi);
        float cosP = FMath::Cos(Phi);

        for (int32 j = 0; j <= R; ++j)
        {
            float Theta = (float)j * 2.0f * PI / (float)R; // 0 to 2PI
            float sinT = FMath::Sin(Theta);
            float cosT = FMath::Cos(Theta);

            // Unit sphere coords
            float UX = sinP * cosT;
            float UY = sinP * sinT;
            float UZ = cosP;

            // Scaled coords
            OutVerts.Add(FVector(UX * RadiusX, UY * RadiusY, UZ * RadiusZ));
            
            // Inward Normal: -Position on unit sphere
            FVector Normal = FVector(-UX, -UY, -UZ);
            Normal.Normalize();
            OutNormals.Add(Normal);

            OutUVs.Add(FVector2D((float)j / (float)R, (float)i / (float)L));
        }
    }

    // Triangles (CCW from inside)
    for (int32 i = 0; i < L; ++i)
    {
        for (int32 j = 0; j < R; ++j)
        {
            int32 v0 = i * (R + 1) + j;
            int32 v1 = i * (R + 1) + j + 1;
            int32 v2 = (i + 1) * (R + 1) + j;
            int32 v3 = (i + 1) * (R + 1) + j + 1;

            // Tri 1
            OutTris.Add(v0);
            OutTris.Add(v2);
            OutTris.Add(v1);

            // Tri 2
            OutTris.Add(v2);
            OutTris.Add(v3);
            OutTris.Add(v1);
        }
    }
}

void UProceduralChunkBuilder::ApplySimplex(
    TArray<FVector>& Verts,
    const TArray<FVector>& InwardNormals,
    const FTransform& ChunkWorldTransform,
    const FChunkCarveParams& Params,
    int32 Seed) const
{
    float Amplitude = Params.NoiseAmplitude;
    float Frequency = Params.NoiseFrequency;
    int32 Octaves = Params.NoiseOctaves;

    if (Verts.Num() != InwardNormals.Num() || Amplitude <= 0.01f) return;

    for (int32 i = 0; i < Verts.Num(); ++i)
    {
        // Sample noise based on WORLD position for continuity across chunks
        FVector WorldPos = ChunkWorldTransform.TransformPosition(Verts[i]);

        float NoiseVal = 0.f;
        float Amp = 1.f;
        float Freq = Frequency;
        float TotalAmp = 0.f;

        for (int32 Oct = 0; Oct < Octaves; ++Oct)
        {
            NoiseVal += Amp * Simplex3D(
                WorldPos.X * Freq,
                WorldPos.Y * Freq,
                WorldPos.Z * Freq,
                Seed + Oct);
            
            TotalAmp += Amp;
            Amp *= 0.5f;
            Freq *= 2.0f;
        }

        // Fading at boundaries to ensure perfect snapping:
        // We fade the noise in 5m (500cm) regions at the chunk ends
        float Fade = 1.0f;
        float FadeDist = 500.0f; 

        // Local X logic: tunnels start at 0 and go to Length
        float LocalX = Verts[i].X;
        
        // Spheroid is centered: X is in [-HalfLen, HalfLen]
        // OvalTube is at start: X is in [0, Length]
        // VerticalDrop is also at start.
        
        // We can detect boundaries by looking at the X min/max of the chunk
        // For simplicity, we fade near the snapping planes (X=0, X=Length for tubes)
        if (Params.PrimaryShape != ECarveShape::Spheroid)
        {
            if (LocalX < FadeDist) Fade = LocalX / FadeDist;
            else if (LocalX > Params.BaseExtents.X - FadeDist) Fade = (Params.BaseExtents.X - LocalX) / FadeDist;
        }
        else
        {
            float HalfLen = Params.BaseExtents.X / 2.0f;
            float DistFromCenter = FMath::Abs(LocalX);
            if (DistFromCenter > HalfLen - FadeDist) Fade = (HalfLen - DistFromCenter) / FadeDist;
        }
        
        Fade = FMath::Clamp(Fade, 0.0f, 1.0f);

        // Apply displacement
        Verts[i] -= InwardNormals[i] * NoiseVal * Amplitude * Fade;
    }
}

float UProceduralChunkBuilder::Simplex3D(float X, float Y, float Z, int32 Seed) const
{
    // Real Simplex is complex to implement locally, we use a smooth Gradient/Perlin-ish sum of sines
    // with different phases to avoid grid patterns.
    
    auto GetOffset = [](int32 S) -> float {
        uint32 h = (uint32)S;
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;
        return (float)(h & 0xFFFF) / 65535.0f * 100.0f;
    };

    float OffX = GetOffset(Seed);
    float OffY = GetOffset(Seed + 101);
    float OffZ = GetOffset(Seed + 202);

    // Sum of sines with non-integer relationships creates a natural look
    float Noise = FMath::Sin(X + OffX) + 
                  FMath::Sin(Y + OffY) + 
                  FMath::Sin(Z + OffZ) +
                  FMath::Sin(X * 0.5f + Y * 0.5f + OffZ) * 0.5f;

    return Noise * 0.25f; // Scale to approx [-1, 1]
}
