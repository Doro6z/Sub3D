#include "SubSonarComponent.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogSonar, Log, All);

namespace
{
constexpr int32 MaxNetworkSafeReplicatedSonarPoints = 1024;
}

USubSonarComponent::USubSonarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void USubSonarComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USubSonarComponent, SonarPoints);
	DOREPLIFETIME(USubSonarComponent, RecentPingTimestamps);
}

void USubSonarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	TickContinuousPing(DeltaTime);
	TickCullExpiredPoints();
}

bool USubSonarComponent::TryFirePing()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - LastPingTime < PingCooldownS)
	{
		UE_LOG(LogSonar, Verbose, TEXT("[%s] SonarPing rejected — cooldown (%.2fs remaining)"),
			*GetOwner()->GetName(), PingCooldownS - (Now - LastPingTime));
		return false;
	}

	ExecutePingRaycasts();
	OnPingFired(LastPingTime, SonarPoints.Num());

	UE_LOG(LogSonar, Log, TEXT("[%s] SonarPing fired | Points=%d | Time=%.2f"),
		*GetOwner()->GetName(), SonarPoints.Num(), LastPingTime);

	return true;
}

void USubSonarComponent::StartContinuousPing()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	bContinuousPingActive = true;
	ContinuousPingAccumulator = 0.f;
	TryFirePing();
}

void USubSonarComponent::StopContinuousPing()
{
	bContinuousPingActive = false;
	ContinuousPingAccumulator = 0.f;
}

void USubSonarComponent::ExecutePingRaycasts()
{
	UWorld* World = GetWorld();
	if (!World || !GetOwner())
	{
		return;
	}

	const FVector Origin = GetOwner()->GetActorLocation();
	const FQuat SubQuat = GetOwner()->GetActorQuat();
	const float HalfAngleRad = FMath::DegreesToRadians(FMath::Clamp(PingHalfAngleDeg, 1.f, 90.f));
	const float CosHalfAngle = FMath::Cos(HalfAngleRad);
	const float PingTime = World->GetTimeSeconds();

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bReturnPhysicalMaterial = false;
	QueryParams.bTraceComplex = false;

	if (bIgnoreAttachedActors)
	{
		TArray<AActor*> AttachedActors;
		GetOwner()->GetAttachedActors(AttachedActors, true);
		for (AActor* AttachedActor : AttachedActors)
		{
			if (AttachedActor)
			{
				QueryParams.AddIgnoredActor(AttachedActor);
			}
		}
	}

	const int32 HCount = FMath::Max(PingRayCountHorizontal, 1);
	const int32 VCount = FMath::Max(PingRayCountVertical, 1);
	const int32 RayCount = HCount * VCount;
	static constexpr float GoldenAngleRad = 2.39996323f;

	TArray<FSonarHitPoint> NewPoints;
	NewPoints.Reserve(RayCount);

	const auto AddHitPoint = [&](const FHitResult& Hit) -> bool
	{
		const AActor* HitActor = Hit.GetActor();
		if (!Hit.bBlockingHit || !HitActor)
		{
			return false;
		}

		if (Hit.Distance < MinAcceptedHitDistanceCm)
		{
			return false;
		}

		if (HitActor == GetOwner())
		{
			return false;
		}

		if (bIgnoreAttachedActors && HitActor->IsAttachedTo(GetOwner()))
		{
			return false;
		}

		FSonarHitPoint& Point = NewPoints.AddDefaulted_GetRef();
		Point.WorldLocation = Hit.ImpactPoint;
		Point.DistanceCm = Hit.Distance;
		Point.PingTimestamp = PingTime;
		Point.Normal = Hit.ImpactNormal;
		Point.LocalLocationAtPing = GetOwner()->GetActorTransform().InverseTransformPosition(Hit.ImpactPoint);
		return true;
	};

	if (bOmnidirectionalPing)
	{
		const float VerticalHalfAngleDeg = FMath::Clamp(OmniPingVerticalHalfAngleDeg, 5.f, 89.f);
		for (int32 VerticalIdx = 0; VerticalIdx < VCount; ++VerticalIdx)
		{
			const float VerticalAlpha = (VCount > 1) ? (static_cast<float>(VerticalIdx) / static_cast<float>(VCount - 1)) : 0.5f;
			const float PitchDeg = FMath::Lerp(-VerticalHalfAngleDeg, VerticalHalfAngleDeg, VerticalAlpha);

			for (int32 HorizontalIdx = 0; HorizontalIdx < HCount; ++HorizontalIdx)
			{
				const float YawDeg = (360.f * static_cast<float>(HorizontalIdx)) / static_cast<float>(HCount);
				const FVector LocalDir = FRotator(PitchDeg, YawDeg, 0.f).Vector();
				const FVector WorldDir = SubQuat.RotateVector(LocalDir.GetSafeNormal());
				const FVector End = Origin + WorldDir * PingMaxRangeCm;

				FHitResult Hit;
				if (World->LineTraceSingleByChannel(Hit, Origin, End, PingTraceChannel, QueryParams))
				{
					AddHitPoint(Hit);
				}
			}
		}
	}
	else
	{
		for (int32 RayIdx = 0; RayIdx < RayCount; ++RayIdx)
		{
			const float U = (static_cast<float>(RayIdx) + 0.5f) / static_cast<float>(RayCount);
			const float CosTheta = FMath::Lerp(1.f, CosHalfAngle, U);
			const float SinTheta = FMath::Sqrt(FMath::Max(0.f, 1.f - CosTheta * CosTheta));
			const float PhiRad = GoldenAngleRad * static_cast<float>(RayIdx);

			const FVector LocalDir(
				CosTheta,
				SinTheta * FMath::Cos(PhiRad),
				SinTheta * FMath::Sin(PhiRad));

			const FVector WorldDir = SubQuat.RotateVector(LocalDir);
			const FVector End = Origin + WorldDir * PingMaxRangeCm;

			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Origin, End, PingTraceChannel, QueryParams))
			{
				AddHitPoint(Hit);
			}
		}
	}

	MergePingPoints(MoveTemp(NewPoints), PingTime);
	LastPingTime = PingTime;
	RecentPingTimestamps.Add(PingTime);
	while (RecentPingTimestamps.Num() > 6)
	{
		RecentPingTimestamps.RemoveAt(0, 1, EAllowShrinking::No);
	}
}

void USubSonarComponent::TickCullExpiredPoints()
{
	UWorld* World = GetWorld();
	if (!World || SonarPoints.Num() == 0)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float LifetimeTotal = PointPeakDurationS + PointFadeDurationS;
	const float SafeSpeed = FMath::Max(PropagationSpeedCmS, 1.f);

	const int32 RemovedCount = SonarPoints.RemoveAll([&](const FSonarHitPoint& P)
	{
		const float RevealTime = P.PingTimestamp + P.DistanceCm / SafeSpeed;
		return (Now - RevealTime) > LifetimeTotal;
	});

	if (RemovedCount > 0)
	{
		UE_LOG(LogSonar, Verbose, TEXT("[%s] SonarCull removed %d expired points | Remaining=%d"),
			*GetOwner()->GetName(), RemovedCount, SonarPoints.Num());
	}
}

void USubSonarComponent::TickContinuousPing(float DeltaTime)
{
	if (!bContinuousPingActive)
	{
		return;
	}

	const float TickInterval = FMath::Max(ContinuousPingIntervalS, 0.05f);
	ContinuousPingAccumulator += FMath::Max(DeltaTime, 0.f);
	while (ContinuousPingAccumulator >= TickInterval)
	{
		TryFirePing();
		ContinuousPingAccumulator -= TickInterval;
	}
}

void USubSonarComponent::MergePingPoints(TArray<FSonarHitPoint>&& NewPoints, float PingTime)
{
	if (!bAccumulatePointsAcrossPings)
	{
		SonarPoints = MoveTemp(NewPoints);
		return;
	}

	const float RefreshRadiusSq = FMath::Square(FMath::Max(PointRefreshRadiusCm, 1.f));
	int32 AddedCount = 0;
	int32 RefreshedCount = 0;

	for (FSonarHitPoint& NewPoint : NewPoints)
	{
		bool bOverlapsExisting = false;
		for (int32 ExistingIdx = 0; ExistingIdx < SonarPoints.Num(); ++ExistingIdx)
		{
			const float DistSq = FVector::DistSquared(
				FVector(SonarPoints[ExistingIdx].WorldLocation),
				FVector(NewPoint.WorldLocation));
			if (DistSq <= RefreshRadiusSq)
			{
				bOverlapsExisting = true;
				break;
			}
		}

		NewPoint.PingTimestamp = PingTime;
		NewPoint.PreviousDistanceCm = -1.f;
		NewPoint.PreviousPingTimestamp = -1.f;
		SonarPoints.Add(NewPoint);
		++AddedCount;
		if (bOverlapsExisting)
		{
			++RefreshedCount;
		}
	}

	const int32 ConfiguredMaxPoints = FMath::Max(MaxRetainedPoints, 64);
	const int32 SafeMaxPoints = FMath::Min(ConfiguredMaxPoints, MaxNetworkSafeReplicatedSonarPoints);
	if (ConfiguredMaxPoints > MaxNetworkSafeReplicatedSonarPoints && !bLoggedReplicationPointCap)
	{
		UE_LOG(
			LogSonar,
			Warning,
			TEXT("[%s] MaxRetainedPoints=%d exceeds the safe replicated sonar budget. Clamping to %d to avoid oversized net bunches."),
			*GetOwner()->GetName(),
			ConfiguredMaxPoints,
			MaxNetworkSafeReplicatedSonarPoints);
		bLoggedReplicationPointCap = true;
	}

	if (SonarPoints.Num() > SafeMaxPoints)
	{
		SonarPoints.Sort([](const FSonarHitPoint& A, const FSonarHitPoint& B)
		{
			return A.PingTimestamp < B.PingTimestamp;
		});

		const int32 ToRemove = SonarPoints.Num() - SafeMaxPoints;
		SonarPoints.RemoveAt(0, ToRemove, EAllowShrinking::No);
	}

	UE_LOG(
		LogSonar,
		Verbose,
		TEXT("[%s] SonarMerge | New=%d Added=%d Refreshed=%d Total=%d"),
		*GetOwner()->GetName(),
		NewPoints.Num(),
		AddedCount,
		RefreshedCount,
		SonarPoints.Num());
}

void USubSonarComponent::OnRep_SonarPoints()
{
	OnPingSonarPointsUpdated();
}
