#include "Tools/SubmarineNavigationTestService.h"

#include "Editor.h"
#include "Engine/World.h"
#include "PlayInEditorDataTypes.h"

namespace Sub3DWave6
{
bool FSubmarineNavigationTestService::RunPIENavigationSmoke(UWorld* InEditorWorld)
{
    if (!GEditor || !InEditorWorld)
    {
        return false;
    }

    // Wave 6: explicit smoke trigger only; no gameplay logic here.
    FRequestPlaySessionParams PlayParams;
    PlayParams.SessionDestination = EPlaySessionDestinationType::InProcess;
    PlayParams.WorldType = EPlaySessionWorldType::PlayInEditor;
    GEditor->RequestPlaySession(PlayParams);
    return true;
}
} // namespace Sub3DWave6
