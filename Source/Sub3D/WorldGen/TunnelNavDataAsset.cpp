#include "TunnelNavDataAsset.h"

void UTunnelNavDataAsset::ResetData()
{
	SchemaVersion = 1;
	BuildHash = 0;
	GenSpec = FRouteGenSpec();
	Seeds = FRouteSeedCascade();
	BuildSettings = FTunnelNavBuildSettings();
	Endpoints = FTunnelNavEndpointSnapshot();
	Nodes.Reset();
	Edges.Reset();
	Samples.Reset();
	RequiredClearanceCm = 0.f;
	bValidationPass = false;
	ValidationReport = FRouteValidationReport();
}

