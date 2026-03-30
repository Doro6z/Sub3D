#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "SubSonarComponent.h"
#include "SubmarineBase.h"

namespace
{
class FScopedSonarAutomationWorld
{
public:
	FScopedSonarAutomationWorld()
	{
		if (!GEngine)
		{
			return;
		}

		const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("SubSonarTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
		WorldContext = &GEngine->CreateNewWorldContext(EWorldType::Game);
		World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
		if (!World)
		{
			return;
		}

		World->AddToRoot();
		WorldContext->SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
	}

	~FScopedSonarAutomationWorld()
	{
		if (!World || !GEngine)
		{
			return;
		}

		if (World->AreActorsInitialized())
		{
			for (AActor* Actor : TActorRange<AActor>(World))
			{
				if (Actor)
				{
					Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
				}
			}
		}

		GEngine->ShutdownWorldNetDriver(World);
		World->DestroyWorld(true);
		World->SetPhysicsScene(nullptr);
		GEngine->DestroyWorldContext(World);
		World->RemoveFromRoot();
		World = nullptr;
		WorldContext = nullptr;
	}

	UWorld* GetWorld() const
	{
		return World;
	}

private:
	UWorld* World = nullptr;
	FWorldContext* WorldContext = nullptr;
};

ASubmarineBase* SpawnTestSubmarine(UWorld* World, const FVector& Location = FVector::ZeroVector)
{
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ASubmarineBase>(ASubmarineBase::StaticClass(), FTransform(FRotator::ZeroRotator, Location), SpawnParams);
}

AActor* SpawnVisibilityBlockingBox(UWorld* World, const FVector& Center, const FVector& Extent)
{
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Obstacle = World->SpawnActor<AActor>(AActor::StaticClass(), Center, FRotator::ZeroRotator, SpawnParams);
	if (!Obstacle)
	{
		return nullptr;
	}

	UBoxComponent* Box = NewObject<UBoxComponent>(Obstacle, TEXT("SonarTestBlocker"));
	if (!Box)
	{
		Obstacle->Destroy();
		return nullptr;
	}

	Obstacle->SetRootComponent(Box);
	Obstacle->AddInstanceComponent(Box);
	Box->SetBoxExtent(Extent);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Box->SetGenerateOverlapEvents(false);
	Box->RegisterComponent();
	Box->SetWorldLocation(Center);

	return Obstacle;
}

void ConfigureDeterministicSonar(USubSonarComponent* Sonar)
{
	if (!Sonar)
	{
		return;
	}

	Sonar->PingTraceChannel = ECC_Visibility;
	Sonar->PingRayCountHorizontal = 24;
	Sonar->PingRayCountVertical = 12;
	Sonar->PingHalfAngleDeg = 18.f;
	Sonar->PingMaxRangeCm = 12000.f;
	Sonar->PropagationSpeedCmS = 3000.f;
	Sonar->MinAcceptedHitDistanceCm = 50.f;
	Sonar->PointPeakDurationS = 2.f;
	Sonar->PointFadeDurationS = 7.f;
	Sonar->PingCooldownS = 1.5f;
	Sonar->bIgnoreAttachedActors = true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubSonarPingFiresRaycastsTest,
	"Sub3D.Submarine.Sonar.PingFiresRaycasts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubSonarPingFiresRaycastsTest::RunTest(const FString& Parameters)
{
	FScopedSonarAutomationWorld ScopedWorld;
	UWorld* World = ScopedWorld.GetWorld();
	TestNotNull(TEXT("Automation world should exist"), World);

	if (!World)
	{
		return false;
	}

	ASubmarineBase* Sub = SpawnTestSubmarine(World);
	TestNotNull(TEXT("Submarine should spawn"), Sub);

	if (!Sub || !Sub->Sonar)
	{
		return false;
	}

	AActor* Obstacle = SpawnVisibilityBlockingBox(World, FVector(5500.f, 0.f, 0.f), FVector(500.f, 5000.f, 5000.f));
	TestNotNull(TEXT("Visibility obstacle should spawn"), Obstacle);
	ConfigureDeterministicSonar(Sub->Sonar);

	const bool bAccepted = Sub->Sonar->TryFirePing();
	TestTrue(TEXT("First ping should be accepted"), bAccepted);
	TestTrue(TEXT("Ping against obstacle should produce at least one point"), Sub->Sonar->SonarPoints.Num() > 0);

	bool bFoundExpectedDistance = false;
	for (const FSonarHitPoint& Point : Sub->Sonar->SonarPoints)
	{
		if (FMath::Abs(Point.DistanceCm - 5000.f) <= 650.f)
		{
			bFoundExpectedDistance = true;
			break;
		}
	}

	TestTrue(TEXT("At least one sonar hit should be near 5000cm (+/-650cm)"), bFoundExpectedDistance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubSonarCooldownEnforcedTest,
	"Sub3D.Submarine.Sonar.CooldownEnforced",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubSonarCooldownEnforcedTest::RunTest(const FString& Parameters)
{
	FScopedSonarAutomationWorld ScopedWorld;
	UWorld* World = ScopedWorld.GetWorld();
	TestNotNull(TEXT("Automation world should exist"), World);

	if (!World)
	{
		return false;
	}

	ASubmarineBase* Sub = SpawnTestSubmarine(World);
	TestNotNull(TEXT("Submarine should spawn"), Sub);

	if (!Sub || !Sub->Sonar)
	{
		return false;
	}

	AActor* Obstacle = SpawnVisibilityBlockingBox(World, FVector(5500.f, 0.f, 0.f), FVector(500.f, 5000.f, 5000.f));
	TestNotNull(TEXT("Visibility obstacle should spawn"), Obstacle);
	ConfigureDeterministicSonar(Sub->Sonar);
	Sub->Sonar->PingCooldownS = 10.f;

	const bool bFirstAccepted = Sub->Sonar->TryFirePing();
	const int32 PointsAfterFirstPing = Sub->Sonar->SonarPoints.Num();
	const bool bSecondAccepted = Sub->Sonar->TryFirePing();

	TestTrue(TEXT("First ping should be accepted"), bFirstAccepted);
	TestTrue(TEXT("First ping should produce points"), PointsAfterFirstPing > 0);
	TestFalse(TEXT("Second ping should be rejected by cooldown"), bSecondAccepted);
	TestEqual(TEXT("Rejected ping should not mutate sonar point count"), Sub->Sonar->SonarPoints.Num(), PointsAfterFirstPing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubSonarPointsCulledAfterLifetimeTest,
	"Sub3D.Submarine.Sonar.PointsCulledAfterLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubSonarPointsCulledAfterLifetimeTest::RunTest(const FString& Parameters)
{
	FScopedSonarAutomationWorld ScopedWorld;
	UWorld* World = ScopedWorld.GetWorld();
	TestNotNull(TEXT("Automation world should exist"), World);

	if (!World)
	{
		return false;
	}

	ASubmarineBase* Sub = SpawnTestSubmarine(World);
	TestNotNull(TEXT("Submarine should spawn"), Sub);

	if (!Sub || !Sub->Sonar)
	{
		return false;
	}

	AActor* Obstacle = SpawnVisibilityBlockingBox(World, FVector(5500.f, 0.f, 0.f), FVector(500.f, 5000.f, 5000.f));
	TestNotNull(TEXT("Visibility obstacle should spawn"), Obstacle);
	ConfigureDeterministicSonar(Sub->Sonar);

	const bool bAccepted = Sub->Sonar->TryFirePing();
	TestTrue(TEXT("Initial ping should be accepted"), bAccepted);
	TestTrue(TEXT("Initial ping should produce points"), Sub->Sonar->SonarPoints.Num() > 0);

	const float Now = World->GetTimeSeconds();
	const float SafeSpeed = FMath::Max(Sub->Sonar->PropagationSpeedCmS, 1.f);
	const float Lifetime = Sub->Sonar->PointPeakDurationS + Sub->Sonar->PointFadeDurationS;
	for (FSonarHitPoint& Point : Sub->Sonar->SonarPoints)
	{
		Point.PingTimestamp = Now - (Lifetime + (Point.DistanceCm / SafeSpeed) + 1.f);
	}

	Sub->Sonar->TickComponent(0.f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Expired sonar points should be culled"), Sub->Sonar->SonarPoints.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubSonarNoHitBehindOccluderTest,
	"Sub3D.Submarine.Sonar.NoHitBehindOccluder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubSonarNoHitBehindOccluderTest::RunTest(const FString& Parameters)
{
	FScopedSonarAutomationWorld ScopedWorld;
	UWorld* World = ScopedWorld.GetWorld();
	TestNotNull(TEXT("Automation world should exist"), World);

	if (!World)
	{
		return false;
	}

	ASubmarineBase* Sub = SpawnTestSubmarine(World);
	TestNotNull(TEXT("Submarine should spawn"), Sub);

	if (!Sub || !Sub->Sonar)
	{
		return false;
	}

	AActor* NearOccluder = SpawnVisibilityBlockingBox(World, FVector(3500.f, 0.f, 0.f), FVector(500.f, 5000.f, 5000.f));
	AActor* FarWall = SpawnVisibilityBlockingBox(World, FVector(8500.f, 0.f, 0.f), FVector(500.f, 5000.f, 5000.f));
	TestNotNull(TEXT("Near occluder should spawn"), NearOccluder);
	TestNotNull(TEXT("Far wall should spawn"), FarWall);

	ConfigureDeterministicSonar(Sub->Sonar);
	Sub->Sonar->PingHalfAngleDeg = 25.f;

	const bool bAccepted = Sub->Sonar->TryFirePing();
	TestTrue(TEXT("Ping should be accepted"), bAccepted);
	TestTrue(TEXT("Ping should produce points"), Sub->Sonar->SonarPoints.Num() > 0);

	float MaxDistance = 0.f;
	for (const FSonarHitPoint& Point : Sub->Sonar->SonarPoints)
	{
		MaxDistance = FMath::Max(MaxDistance, Point.DistanceCm);
	}

	TestTrue(TEXT("No sonar hit should come from behind the near occluder"), MaxDistance < 5000.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSubSonarAccumulationAcrossPingsTest,
	"Sub3D.Submarine.Sonar.AccumulatesAcrossPings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSubSonarAccumulationAcrossPingsTest::RunTest(const FString& Parameters)
{
	FScopedSonarAutomationWorld ScopedWorld;
	UWorld* World = ScopedWorld.GetWorld();
	TestNotNull(TEXT("Automation world should exist"), World);
	if (!World)
	{
		return false;
	}

	ASubmarineBase* Sub = SpawnTestSubmarine(World);
	TestNotNull(TEXT("Submarine should spawn"), Sub);
	if (!Sub || !Sub->Sonar)
	{
		return false;
	}

	ConfigureDeterministicSonar(Sub->Sonar);
	Sub->Sonar->PingCooldownS = 0.f;
	Sub->Sonar->bAccumulatePointsAcrossPings = true;
	Sub->Sonar->PointRefreshRadiusCm = 160.f;

	AActor* ObstacleA = SpawnVisibilityBlockingBox(World, FVector(5000.f, 0.f, 0.f), FVector(500.f, 3800.f, 3800.f));
	AActor* ObstacleB = SpawnVisibilityBlockingBox(World, FVector(5000.f, 2600.f, 0.f), FVector(500.f, 1800.f, 1800.f));
	TestNotNull(TEXT("Obstacle A should spawn"), ObstacleA);
	TestNotNull(TEXT("Obstacle B should spawn"), ObstacleB);

	const bool bFirstAccepted = Sub->Sonar->TryFirePing();
	TestTrue(TEXT("First ping should be accepted"), bFirstAccepted);
	const int32 FirstCount = Sub->Sonar->SonarPoints.Num();
	TestTrue(TEXT("First ping should produce points"), FirstCount > 0);

	const bool bSecondAccepted = Sub->Sonar->TryFirePing();
	TestTrue(TEXT("Second ping should be accepted"), bSecondAccepted);
	const int32 SecondCount = Sub->Sonar->SonarPoints.Num();

	TestTrue(TEXT("Accumulation mode should preserve existing points across pings"), SecondCount >= FirstCount);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
