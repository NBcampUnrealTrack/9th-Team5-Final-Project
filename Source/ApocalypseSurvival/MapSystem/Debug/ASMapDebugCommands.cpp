// 맵 시스템 디버그 콘솔 명령. 모두 서버/호스트에서 실행하며 Shipping 빌드에는 포함하지 않는다.

#include "ApocalypseSurvival.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/IConsoleManager.h"
#include "MapSystem/Vision/ASFogOfWarManager.h"
#include "MapSystem/Vision/ASVisionWorldSubsystem.h"
#include "MapSystem/Zone/Effects/ASZoneEffectComponent.h"
#include "MapSystem/Zone/Manager/ASMapSystemManager.h"

#if !UE_BUILD_SHIPPING
namespace ASMapDebugCommandsPrivate
{
	AASMapSystemManager* FindMapManager(UWorld* World)
	{
		TActorIterator<AASMapSystemManager> It(World);
		AASMapSystemManager* Manager = It ? *It : nullptr;
		if (!Manager)
		{
			UE_LOG(LogApocalypseSurvival, Warning, TEXT("AS.Map: no map manager in this world (commands run on the server/host)."));
		}
		return Manager;
	}

	AASFogOfWarManager* FindFogManager(UWorld* World)
	{
		const UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(World);
		AASFogOfWarManager* Manager = Subsystem ? Subsystem->GetFogOfWarManager() : nullptr;
		if (!Manager || !Manager->HasAuthority())
		{
			UE_LOG(LogApocalypseSurvival, Warning, TEXT("AS.Vision: no authoritative fog manager in this world (commands run on the server/host)."));
			return nullptr;
		}
		return Manager;
	}

	static FAutoConsoleCommandWithWorldAndArgs MapDumpCommand(
		TEXT("AS.Map.Dump"),
		TEXT("Log zone dangers and seed."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (const AASMapSystemManager* Manager = FindMapManager(World))
			{
				Manager->DumpState();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs MapStepCommand(
		TEXT("AS.Map.Step"),
		TEXT("AS.Map.Step <Day> <Day|Night>: send a step directly (bypasses the GameMode clock)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AASMapSystemManager* Manager = FindMapManager(World);
			if (!Manager || Args.Num() != 2)
			{
				return;
			}

			FASMapPhaseContext Context;
			Context.Day = FCString::Atoi(*Args[0]);
			if (Context.Day < 1
				|| (!Args[1].Equals(TEXT("Day"), ESearchCase::IgnoreCase) && !Args[1].Equals(TEXT("Night"), ESearchCase::IgnoreCase)))
			{
				UE_LOG(LogApocalypseSurvival, Warning, TEXT("Usage: AS.Map.Step <Day >= 1> <Day|Night>"));
				return;
			}
			Context.DayPhase = Args[1].Equals(TEXT("Night"), ESearchCase::IgnoreCase) ? EASDayPhase::Night : EASDayPhase::Day;
			Manager->HandlePhaseChanged(Context);
		}));

	static FAutoConsoleCommandWithWorldAndArgs ZoneEffectDumpCommand(
		TEXT("AS.Zone.EffectDump"),
		TEXT("Log tracked zone effects on the server/host."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
			const UASZoneEffectComponent* Component = GameMode ? GameMode->FindComponentByClass<UASZoneEffectComponent>() : nullptr;
			if (Component)
			{
				Component->DumpState();
			}
			else
			{
				UE_LOG(LogApocalypseSurvival, Warning, TEXT("AS.Zone: no zone effect component on the GameMode (server/host only)."));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs VisionWallCommand(
		TEXT("AS.Vision.WallOcclusion"),
		TEXT("AS.Vision.WallOcclusion [0|1]: toggle wall occlusion for all players (default 1)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AASFogOfWarManager* Manager = FindFogManager(World);
			if (!Manager)
			{
				return;
			}
			if (Args.Num() > 1 || (Args.Num() == 1 && Args[0] != TEXT("0") && Args[0] != TEXT("1")))
			{
				UE_LOG(LogApocalypseSurvival, Warning, TEXT("Usage: AS.Vision.WallOcclusion [0|1]"));
				return;
			}
			Manager->SetWallOcclusionEnabled(Args.Num() == 0 || Args[0] == TEXT("1"));
		}));

	static FAutoConsoleCommandWithWorldAndArgs VisionNightCommand(
		TEXT("AS.Vision.Night"),
		TEXT("AS.Vision.Night <0|1>: blend day/night vision on the server."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AASFogOfWarManager* Manager = FindFogManager(World);
			if (!Manager)
			{
				return;
			}
			if (Args.Num() != 1 || (Args[0] != TEXT("0") && Args[0] != TEXT("1")))
			{
				UE_LOG(LogApocalypseSurvival, Warning, TEXT("Usage: AS.Vision.Night <0|1>"));
				return;
			}
			Manager->SetNight(Args[0] == TEXT("1"));
		}));
}
#endif
