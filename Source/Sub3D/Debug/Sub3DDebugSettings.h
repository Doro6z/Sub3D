#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Sub3DDebugSettings.generated.h"

/**
 * Project-wide centralized debug toggles for Sub3D.
 * Exposed under Project Settings > Game > Sub3D Debug.
 * Components read via GetDefault<USub3DDebugSettings>()->b...
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Sub3D Debug"))
class SUB3D_API USub3DDebugSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName("Game"); }

	// ── Crew ──────────────────────────────────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Crew")
	bool bLogCrewMovement = false;

	UPROPERTY(Config, EditAnywhere, Category = "Crew")
	bool bDrawCrewMovement = false;

	UPROPERTY(Config, EditAnywhere, Category = "Crew")
	bool bLogCrewEnvironmentState = false;

	/** Per-tick log of crew position in world + sub-local frame, movement base, falling flag. For diagnosing airborne/walkable-transition jitter. */
	UPROPERTY(Config, EditAnywhere, Category = "Crew")
	bool bLogCrewJitter = false;

	/** Warn when crew relative-frame velocity exceeds this. Chosen above max plausible ground speed (sprint ~500 cm/s) so frame hitches at running speed aren't false-flagged. */
	UPROPERTY(Config, EditAnywhere, Category = "Crew", meta = (ClampMin = "10.0"))
	float CrewJitterWarnVelocityCmPerSec = 1200.f;

	/** When true, snap crew world-pos back to expected Rel if single-tick relative speed exceeds CrewTetherVelocityCmPerSec. Fixes sub-world-jump jitter (hitch recovery / network snapshot) that CMC MovementBase fails to carry. */
	UPROPERTY(Config, EditAnywhere, Category = "Crew")
	bool bEnableCrewTether = true;

	/** Threshold above which the crew tether fires. Set above max plausible foot speed (~500 cm/s) so only sub-jump-driven jitter triggers. */
	UPROPERTY(Config, EditAnywhere, Category = "Crew", meta = (ClampMin = "10.0"))
	float CrewTetherVelocityCmPerSec = 800.f;

	/** Log when the crew tether fires. */
	UPROPERTY(Config, EditAnywhere, Category = "Crew")
	bool bLogCrewTether = true;

	// ── Submarine | Movement ──────────────────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Movement")
	bool bLogSubMovement = false;

	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Movement")
	bool bLogSubCollisionSweeps = false;

	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Movement")
	bool bLogSubHullCollisions = false;

	// ── Submarine | Hull ──────────────────────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Hull")
	bool bDrawHull = false;

	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Hull")
	bool bDrawHullSheets = false;

	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Hull")
	bool bDrawHullSheetCellsAlways = false;

	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Hull")
	bool bLogHullVisualBreaches = false;

	// ── Submarine | Interior Frame ────────────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|InteriorFrame")
	bool bLogInteriorFrame = false;

	UPROPERTY(Config, EditAnywhere, Category = "Submarine|InteriorFrame")
	bool bDrawInteriorFrame = false;

	// ── Helm ──────────────────────────────────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Helm")
	bool bLogHelmNavigationDisplay = false;

	// ── Interaction ───────────────────────────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Interaction")
	bool bDrawInteractionTrace = false;

	// ── Sonar ─────────────────────────────────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Sonar")
	bool bLogSonar = false;

	// ── Tunnel Navigation ─────────────────────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "TunnelNavigation")
	bool bDrawTunnelNavigation = false;

	UPROPERTY(Config, EditAnywhere, Category = "TunnelNavigation")
	bool bLogTunnelNavigation = false;

	// ── Compiler ──────────────────────────────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Compiler")
	bool bLogCompilerExteriorCollisionProxy = false;
};
