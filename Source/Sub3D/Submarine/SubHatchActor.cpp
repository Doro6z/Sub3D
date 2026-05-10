#include "SubHatchActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

ASubHatchActor::ASubHatchActor()
{
	// Hatch shape: short on Z (cap thickness), wider on X/Y (lying horizontal).
	// Door defaults are tuned for a vertical wall opening; swap to horizontal here so
	// designers don't have to override the collision in BP just to get sensible defaults.
	if (DoorCollision)
	{
		DoorCollision->SetBoxExtent(FVector(45.f, 45.f, 5.f));
	}
}
