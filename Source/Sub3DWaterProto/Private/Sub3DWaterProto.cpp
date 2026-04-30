#include "Sub3DWaterProto.h"

#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDeviceFile.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"

#include "RoomActor.h"
#include "RoomWaterRenderer.h"
#include "RoomWaterBakedData.h"
#include "RoomWaterDebugDrawer.h"

DEFINE_LOG_CATEGORY(LogWaterProto);

static FOutputDeviceFile* GWaterProtoLogFile = nullptr;

void FSub3DWaterProtoModule::StartupModule()
{
    const FString WaterProtoLogPath = FPaths::ProjectLogDir() / TEXT("WaterProto.log");
    GWaterProtoLogFile = new FOutputDeviceFile(
        *WaterProtoLogPath,
        /*bDisableBackup*/ false,
        /*bAppendIfExists*/ false,
        /*bCreateWriterLazily*/ false);
    if (GWaterProtoLogFile)
    {
        GWaterProtoLogFile->SetSuppressEventTag(false);
        GLog->AddOutputDevice(GWaterProtoLogFile);
        UE_LOG(LogWaterProto, Display, TEXT("=== WaterProto log started — file: %s ==="), *WaterProtoLogPath);
    }
}

void FSub3DWaterProtoModule::ShutdownModule()
{
    if (GWaterProtoLogFile)
    {
        UE_LOG(LogWaterProto, Display, TEXT("=== WaterProto log ended ==="));
        GLog->RemoveOutputDevice(GWaterProtoLogFile);
        GWaterProtoLogFile->TearDown();
        delete GWaterProtoLogFile;
        GWaterProtoLogFile = nullptr;
    }
}

IMPLEMENT_MODULE(FSub3DWaterProtoModule, Sub3DWaterProto)

// ─────────────────────────────────────────────────────────────────────────────
// Console commands
// ─────────────────────────────────────────────────────────────────────────────

static FAutoConsoleCommandWithWorld GCmdProtoDebugAll(
    TEXT("proto.WaterDebugAll"),
    TEXT("Dessine un snapshot debug pour TOUTES les ARoomActor de la map. Logue un dump complet."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World) { return; }
        int32 Count = 0;
        for (TActorIterator<ARoomActor> It(World); It; ++It)
        {
            if (It->WaterRenderer)
            {
                It->WaterRenderer->DrawDebugSnapshot();
                ++Count;
            }
        }
        UE_LOG(LogWaterProto, Display, TEXT("proto.WaterDebugAll: %d rooms processed"), Count);
    }));

static FAutoConsoleCommandWithWorld GCmdProtoReset(
    TEXT("proto.WaterReset"),
    TEXT("Reset le heightfield de toutes les ARoomActor (force = 0)."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World) { return; }
        int32 Count = 0;
        for (TActorIterator<ARoomActor> It(World); It; ++It)
        {
            if (It->WaterRenderer)
            {
                It->WaterRenderer->ResetHeightfield();
                ++Count;
            }
        }
        UE_LOG(LogWaterProto, Display, TEXT("proto.WaterReset: %d rooms reset"), Count);
    }));

static FAutoConsoleCommandWithWorldAndArgs GCmdProtoSetLevel(
    TEXT("proto.SetLevel"),
    TEXT("Force un niveau d'eau normalisé (0..1) sur une room. Usage: proto.SetLevel <RoomId> <0..1>"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (!World || Args.Num() < 2)
        {
            UE_LOG(LogWaterProto, Warning, TEXT("Usage: proto.SetLevel <RoomId> <0..1>"));
            return;
        }
        const FName RoomId(*Args[0]);
        const float Level = FCString::Atof(*Args[1]);
        for (TActorIterator<ARoomActor> It(World); It; ++It)
        {
            if (It->RoomId == RoomId)
            {
                It->WaterLevelNormalized = FMath::Clamp(Level, 0.f, 1.f);
                UE_LOG(LogWaterProto, Display, TEXT("Set %s level = %.3f"), *RoomId.ToString(), Level);
                return;
            }
        }
        UE_LOG(LogWaterProto, Warning, TEXT("Room %s not found"), *RoomId.ToString());
    }));

static FAutoConsoleCommandWithWorldAndArgs GCmdProtoInject(
    TEXT("proto.Inject"),
    TEXT("Injecte une perturbation au centre de la room. Usage: proto.Inject <RoomId> [Force=10] [Radius=80]"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (!World || Args.Num() < 1)
        {
            UE_LOG(LogWaterProto, Warning, TEXT("Usage: proto.Inject <RoomId> [Force] [Radius]"));
            return;
        }
        const FName RoomId(*Args[0]);
        const float Force = (Args.Num() >= 2) ? FCString::Atof(*Args[1]) : 10.f;
        const float Radius = (Args.Num() >= 3) ? FCString::Atof(*Args[2]) : 80.f;
        for (TActorIterator<ARoomActor> It(World); It; ++It)
        {
            if (It->RoomId == RoomId && It->WaterRenderer && It->BakedData)
            {
                const FVector Mid = (It->BakedData->LocalBoundsMin + It->BakedData->LocalBoundsMax) * 0.5f;
                It->WaterRenderer->InjectAt(FVector2D(Mid.X, Mid.Y), Force, Radius);
                UE_LOG(LogWaterProto, Display, TEXT("Injected in %s @ center (force=%.1f radius=%.1f)"),
                    *RoomId.ToString(), Force, Radius);
                return;
            }
        }
        UE_LOG(LogWaterProto, Warning, TEXT("Room %s not found"), *RoomId.ToString());
    }));

static FAutoConsoleCommandWithWorld GCmdProtoDumpBake(
    TEXT("proto.DumpBakeStats"),
    TEXT("Logue les stats détaillées de tous les BakedData en mémoire."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World) { return; }
        int32 Count = 0;
        for (TActorIterator<ARoomActor> It(World); It; ++It)
        {
            if (It->BakedData)
            {
                URoomWaterDebugDrawer::DumpBakedDataToLog(It->BakedData);
                ++Count;
            }
        }
        UE_LOG(LogWaterProto, Display, TEXT("proto.DumpBakeStats: %d rooms dumped"), Count);
    }));
