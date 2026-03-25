#include "CampaignGraphAsset.h"

const FCampaignSegmentDescriptor* UCampaignGraphAsset::FindSegmentByID(FName SegmentID) const
{
	return Segments.FindByPredicate([SegmentID](const FCampaignSegmentDescriptor& Segment)
	{
		return Segment.SegmentID == SegmentID;
	});
}

int32 UCampaignGraphAsset::FindSegmentIndexByID(FName SegmentID) const
{
	return Segments.IndexOfByPredicate([SegmentID](const FCampaignSegmentDescriptor& Segment)
	{
		return Segment.SegmentID == SegmentID;
	});
}

int32 UCampaignGraphAsset::CountIncomingConnections(FName SegmentID) const
{
	int32 IncomingCount = 0;
	for (const FCampaignSegmentDescriptor& Segment : Segments)
	{
		for (const FName NextID : Segment.NextSegmentIDs)
		{
			if (NextID == SegmentID)
			{
				IncomingCount++;
			}
		}
	}
	return IncomingCount;
}

bool UCampaignGraphAsset::IsValidGraph(FString& OutError) const
{
	OutError.Reset();

	if (Segments.Num() == 0)
	{
		OutError = TEXT("Campaign graph has no segments.");
		return false;
	}

	if (RootSegmentID.IsNone())
	{
		OutError = TEXT("Campaign graph RootSegmentID is None.");
		return false;
	}

	if (TerminalSegmentID.IsNone())
	{
		OutError = TEXT("Campaign graph TerminalSegmentID is None.");
		return false;
	}

	TSet<FName> SeenIDs;
	for (const FCampaignSegmentDescriptor& Segment : Segments)
	{
		if (Segment.SegmentID.IsNone())
		{
			OutError = TEXT("Campaign graph contains a segment with empty SegmentID.");
			return false;
		}

		if (SeenIDs.Contains(Segment.SegmentID))
		{
			OutError = FString::Printf(TEXT("Duplicate SegmentID '%s' in campaign graph."), *Segment.SegmentID.ToString());
			return false;
		}
		SeenIDs.Add(Segment.SegmentID);
	}

	if (!SeenIDs.Contains(RootSegmentID))
	{
		OutError = FString::Printf(TEXT("RootSegmentID '%s' not found in graph."), *RootSegmentID.ToString());
		return false;
	}

	if (!SeenIDs.Contains(TerminalSegmentID))
	{
		OutError = FString::Printf(TEXT("TerminalSegmentID '%s' not found in graph."), *TerminalSegmentID.ToString());
		return false;
	}

	for (const FCampaignSegmentDescriptor& Segment : Segments)
	{
		for (const FName NextID : Segment.NextSegmentIDs)
		{
			if (!SeenIDs.Contains(NextID))
			{
				OutError = FString::Printf(
					TEXT("Segment '%s' points to missing NextSegmentID '%s'."),
					*Segment.SegmentID.ToString(),
					*NextID.ToString());
				return false;
			}
		}
	}

	return true;
}
