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

	// ── Master toggles ────────────────────────────────────────
	/** Master switch for the unified motion-chain instrumentation. When ON, all
	 *  motion-related log toggles are forced on (sub movement, sub interp pacing,
	 *  flood, crew movement, crew jitter, crew env state, interior frame), AND the
	 *  aggregate "Motion chain tick" line is emitted once per render frame from each
	 *  locally-controlled crew so server/client/peer streams correlate by frame.
	 *
	 *  Authority-max plan: see
	 *  reports/plans/2026-04-27_unified_motion_chain_master_refactor.md §7. */
	UPROPERTY(Config, EditAnywhere, Category = "MotionChain")
	bool bLogPresentationChain = false;

	/** Per-tick crew motion chain TRACE: captures the crew capsule state at 4 points
	 *  in each tick (PRE-rebase, POST-rebase, POST-CMC, POST-extract), prints the
	 *  delta at each handoff, AND emits a separate EVENT line when MovementBase /
	 *  MovementMode / Floor changes. Lets you see exactly what happens at the
	 *  precise frame a stair / floor-loss / breach trigger fires.
	 *  Volume: ~30 lines/sec when crew is locally controlled. Use for short repros. */
	UPROPERTY(Config, EditAnywhere, Category = "MotionChain")
	bool bLogMotionChainTrace = false;

	// Read-helpers ride OR with the master toggle, so a single click in Project
	// Settings turns the whole chain on without touching the per-component toggles.
	bool ShouldLogSubMovement() const           { return bLogSubMovement           || bLogPresentationChain; }
	bool ShouldLogSubInterpPacing() const       { return bLogSubInterpPacing       || bLogPresentationChain; }
	bool ShouldLogFlood() const                 { return bLogFlood                 || bLogPresentationChain; }
	bool ShouldLogCrewMovement() const          { return bLogCrewMovement          || bLogPresentationChain; }
	bool ShouldLogCrewJitter() const            { return bLogCrewJitter            || bLogPresentationChain; }
	bool ShouldLogCrewEnvironmentState() const  { return bLogCrewEnvironmentState  || bLogPresentationChain; }

	// ── Crew ──────────────────────────────────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Crew")
	bool bLogCrewMovement = false;

	UPROPERTY(Config, EditAnywhere, Category = "Crew")
	bool bDrawCrewMovement = false;

	/** On-screen HUD block: EmbarkState, CurrentCompartment, Grid/World/Sub pose. Validation harness for the crew architecture. */
	UPROPERTY(Config, EditAnywhere, Category = "Crew")
	bool bDrawCrewGridAuthority = false;

	UPROPERTY(Config, EditAnywhere, Category = "Crew")
	bool bLogCrewEnvironmentState = false;

	/** Per-tick log of crew position in world + sub-local frame, movement base, falling flag. For diagnosing airborne/walkable-transition jitter. */
	UPROPERTY(Config, EditAnywhere, Category = "Crew")
	bool bLogCrewJitter = false;

	/** Warn when crew relative-frame velocity exceeds this. Chosen above max plausible ground speed (sprint ~500 cm/s) so frame hitches at running speed aren't false-flagged. */
	UPROPERTY(Config, EditAnywhere, Category = "Crew", meta = (ClampMin = "10.0"))
	float CrewJitterWarnVelocityCmPerSec = 1200.f;

	// Crew animation debug: validation layer for movement state, water contact, procedural pose, and IK.
	UPROPERTY(Config, EditAnywhere, Category = "Crew|AnimationDebug")
	bool bLogCrewAnimWarnings = false;

	UPROPERTY(Config, EditAnywhere, Category = "Crew|AnimationDebug")
	bool bDrawCrewAnimDebug = false;

	UPROPERTY(Config, EditAnywhere, Category = "Crew|AnimationDebug")
	bool bShowCrewAnimDebugPanel = false;

	UPROPERTY(Config, EditAnywhere, Category = "Crew|AnimationDebug")
	bool bCaptureCrewAnimContributions = false;

	UPROPERTY(Config, EditAnywhere, Category = "Crew|AnimationDebug")
	bool bDrawCrewAnimBones = false;

	UPROPERTY(Config, EditAnywhere, Category = "Crew|AnimationDebug")
	bool bDrawCrewAnimIK = false;

	UPROPERTY(Config, EditAnywhere, Category = "Crew|AnimationDebug")
	bool bDrawCrewAnimFrameAxes = false;

	UPROPERTY(Config, EditAnywhere, Category = "Crew|AnimationDebug", meta = (ClampMin = "0.01", ClampMax = "5.0"))
	float CrewAnimDebugSampleIntervalSeconds = 0.1f;

	UPROPERTY(Config, EditAnywhere, Category = "Crew|AnimationDebug", meta = (ClampMin = "1", ClampMax = "600"))
	int32 CrewAnimDebugRecentSnapshotCount = 600;

	UPROPERTY(Config, EditAnywhere, Category = "Crew|AnimationDebug", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float CrewAnimArmRestWarningDeg = 60.f;

	// ── Submarine | Movement ──────────────────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Movement")
	bool bLogSubMovement = false;

	/** Per-render-frame pose pacing log: dt, Sub.X, dx_render, alpha (non-auth) or simAcc (auth), snapshot age. */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Movement")
	bool bLogSubInterpPacing = false;

	/** Diagnostic kill-switch: keep the authoritative submarine actor root on the fixed-step sim pose instead of applying render interpolation to that same root. */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Movement")
	bool bDisableSubRootVisualInterpolation = false;

	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Movement")
	bool bLogSubCollisionSweeps = false;

	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Movement")
	bool bLogSubHullCollisions = false;

	// ── Submarine | Flood ─────────────────────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Flood")
	bool bLogFlood = false;

	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Flood", meta = (ClampMin = "0.1"))
	float FloodLogIntervalSeconds = 1.f;

	/** In-game wireframe of UCompartmentVolumeComponent boxes + CompartmentId label. */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Flood")
	bool bDrawCompartmentVolumes = false;

	/** In-game wireframe of water surface per compartment + label "CompartmentId H=XXXcm L=0.XX". */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Flood")
	bool bDrawCompartmentWater = false;

	/** Diagnostic kill-switch: do not spawn per-compartment UFloodWaterPlaneComponent visuals. Flood sim still runs. */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Flood")
	bool bDisableFloodWaterPlanes = false;

	/** Diagnostic kill-switch: do not spawn USubHullBoundaryComponent for active breaches. Flood sim still runs. */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Flood")
	bool bDisableBreachBoundaries = false;

	/** Draw a red solid box at every active breach's world position + label with CompartmentId
	 *  and inflow rate. Editor + PIE; compiled out in Shipping. */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Flood")
	bool bDrawBreachMarkers = false;

	/** Half-extent (cm) of the breach marker red box. */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Flood", meta = (ClampMin = "1.0"))
	float BreachMarkerHalfExtentCm = 25.f;

	/** Draw a debug sphere + circle at every UFloodWaterPlaneComponent::InjectAt call site.
	 *  Independent from the cap mesh (which is hidden when the compartment is empty), so injects
	 *  are observable even when no water is rendered. Editor + PIE; compiled out in Shipping. */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Flood")
	bool bDrawWaterInjectMarkers = true;

	/** How long the inject debug marker stays on screen (seconds). */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Flood", meta = (ClampMin = "0.05", ClampMax = "10.0"))
	float WaterInjectMarkerLifetime = 1.5f;

	/** Draw the CompartmentId text at each compartment volume's center. Use to verify which
	 *  CompartmentA / CompartmentB to set on a placed door. */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Flood")
	bool bDrawCompartmentLabels = false;

	/** Draw a colored sphere + label at each connection's LocalTransform (from the DA). Color
	 *  by ConnectionType: Door=red, Hatch=orange, ExteriorHatch=blue, Open=green. Label format:
	 *  "ConnectionId | CompartmentA ↔ CompartmentB". The sphere shows where the DA expected
	 *  the door to be — your manually-placed BP can be elsewhere, but its DoorId must match
	 *  the text shown here. */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Flood")
	bool bDrawConnectionMarkers = false;

	// ── Submarine | Hull ──────────────────────────────────────
	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Hull")
	bool bDrawHull = false;

	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Hull")
	bool bDrawHullSheets = false;

	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Hull")
	bool bDrawHullSheetCellsAlways = false;

	UPROPERTY(Config, EditAnywhere, Category = "Submarine|Hull")
	bool bLogHullVisualBreaches = false;

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
