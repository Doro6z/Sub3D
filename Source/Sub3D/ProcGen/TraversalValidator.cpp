#include "TraversalValidator.h"

EValidationResult UTraversalValidator::ValidateLayout(const TArray<FChunkInstance>& Instances, float& OutScore) const
{
    UE_LOG(LogTemp, Display, TEXT("--- PASS 4: VALIDATION START ---"));
    OutScore = 1.0f;
    if (Instances.Num() == 0) 
    {
        UE_LOG(LogTemp, Error, TEXT("UTraversalValidator: No instances to validate!"));
        return EValidationResult::HardFail;
    }

    // 1. Check Hard Rules for every chunk
    for (const FChunkInstance& Instance : Instances)
    {
        if (!CheckHardRules(Instance))
        {
            UE_LOG(LogTemp, Warning, TEXT("UTraversalValidator: HARD FAIL on chunk %s"), *Instance.ChunkID.ToString());
            return EValidationResult::HardFail;
        }
    }

    // 2. Score Soft Rules
    OutScore = ScoreSoftRules(Instances);
    UE_LOG(LogTemp, Log, TEXT("UTraversalValidator: Final Score = %.2f (Min required = %.2f)"), OutScore, MinLayoutScore);

    if (OutScore < MinLayoutScore)
    {
        UE_LOG(LogTemp, Warning, TEXT("UTraversalValidator: SOFT FAIL (Score %.2f < Min %.2f)"), OutScore, MinLayoutScore);
        return EValidationResult::SoftFail;
    }

    UE_LOG(LogTemp, Display, TEXT("--- PASS 4: VALIDATION SUCCESS ---"));
    return EValidationResult::Valid;
}

bool UTraversalValidator::CheckHardRules(const FChunkInstance& Instance) const
{
    // Check Connector In
    if (Instance.ConnectorIn.UsableWidth < MinUsableWidth)
    {
        UE_LOG(LogTemp, Verbose, TEXT("  HardRule Check: %s ConnectorIn UsableWidth %.1f < %.1f"), *Instance.ChunkID.ToString(), Instance.ConnectorIn.UsableWidth, MinUsableWidth);
        return false;
    }

    // Check Connector Out
    if (Instance.ConnectorOut.UsableWidth < MinUsableWidth)
    {
        UE_LOG(LogTemp, Verbose, TEXT("  HardRule Check: %s ConnectorOut UsableWidth %.1f < %.1f"), *Instance.ChunkID.ToString(), Instance.ConnectorOut.UsableWidth, MinUsableWidth);
        return false;
    }

    return true;
}

float UTraversalValidator::ScoreSoftRules(const TArray<FChunkInstance>& Instances) const
{
    float Score = 1.0f;
    int32 ConsecutiveChokes = 0;

    for (const FChunkInstance& Instance : Instances)
    {
        // Is this a choke? (Narrow tunnel)
        // Note: For now we detect it based on ID or simple width threshold
        bool bIsChoke = (Instance.ConnectorIn.UsableWidth < MinUsableWidth * 1.5f);

        if (bIsChoke)
        {
            ConsecutiveChokes++;
        }
        else
        {
            ConsecutiveChokes = 0;
        }

        // Penalty for too many consecutive chokes (fatigue effect)
        if (ConsecutiveChokes > 2)
        {
            Score -= 0.2f;
        }
    }

    return FMath::Clamp(Score, 0.0f, 1.0f);
}
