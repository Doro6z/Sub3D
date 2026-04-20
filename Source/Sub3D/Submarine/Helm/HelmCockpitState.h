#pragma once

#include "CoreMinimal.h"
#include "HelmCockpitState.generated.h"

// Mode enums shared by the helm cockpit instruments. Stored as transient
// client-side state (not replicated) — the backend CommandState flags are
// derived from the active mode at the moment of switch.
//
// See reports/plans/2026-04-18_helm_cockpit_redesign.md §7.

UENUM(BlueprintType)
enum class EHelmSpeedMode : uint8
{
	// Direct = a discrete telegraph preset is the active target.
	Direct,

	// HoldCruise = AutoSpeed locks the captured ForwardSpeedCmS at the
	// moment the player hit HOLD CRUISE.
	HoldCruise,

	// Stop = AutoSpeed with TargetSpeedCmS = 0. Sub brakes to standstill.
	Stop,
};

UENUM(BlueprintType)
enum class EHelmSteerMode : uint8
{
	// Direct = manual rudder via yoke / keys; recenters on release if hold off.
	Direct,

	// HoldRudder = bRudderHoldEnabled true; rudder stays at last command.
	HoldRudder,

	// AutoHeading = (FP+) target heading drives rudder. Not implemented for FP.
	AutoHeading,
};

UENUM(BlueprintType)
enum class EHelmDiveMode : uint8
{
	// Direct = manual hydroplane + manual ballast.
	Direct,

	// HoldVert = Auto Depth at zero vertical velocity.
	HoldVert,

	// Surface = AutoDepth off; force all tanks to 0% (rapid rise).
	Surface,

	// DiveCommand = AutoDepth off; force all tanks to 100% (rapid sink).
	DiveCommand,
};
