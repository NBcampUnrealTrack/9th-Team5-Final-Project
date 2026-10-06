#include "Map/Vision/ASFogOfWarManager.h"

#include "ApocalypseSurvival.h"
#include "Components/SceneComponent.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Map/Vision/ASFogDisplayComponent.h"
#include "Map/Vision/ASVisionDefinition.h"
#include "Map/Vision/ASVisionEvaluatorComponent.h"
#include "Map/Vision/ASVisionWorldSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace ASVisionManagerPrivate
{
	static FAutoConsoleCommandWithWorldAndArgs WallCommand(
		TEXT("AS.Vision.WallOcclusion"),
		TEXT("AS.Vision.WallOcclusion [0|1]: toggle wall occlusion for all players (default 1)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(World);
			AASFogOfWarManager* Manager = Subsystem ? Subsystem->GetFogOfWarManager() : nullptr;
			if (!Manager || !Manager->HasAuthority())
			{
				UE_LOG(LogApocalypseSurvival, Warning, TEXT("AS.Vision: no authoritative fog manager in this world (commands run on the server/host)."));
				return;
			}
			if (Args.Num() > 1 || (Args.Num() == 1 && Args[0] != TEXT("0") && Args[0] != TEXT("1")))
			{
				UE_LOG(LogApocalypseSurvival, Warning, TEXT("Usage: AS.Vision.WallOcclusion [0|1]"));
				return;
			}
			Manager->SetWallOcclusionEnabled(Args.Num() == 0 || Args[0] == TEXT("1"));
		}));
}

AASFogOfWarManager::AASFogOfWarManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetCanBeDamaged(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	VisionEvaluatorComponent = CreateDefaultSubobject<UASVisionEvaluatorComponent>(TEXT("VisionEvaluator"));
	FogDisplayComponent = CreateDefaultSubobject<UASFogDisplayComponent>(TEXT("FogDisplay"));
}

void AASFogOfWarManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AASFogOfWarManager, VisionDefinition);
	DOREPLIFETIME(AASFogOfWarManager, bWallOcclusionEnabled);
	DOREPLIFETIME(AASFogOfWarManager, ExploredResetCount);
}

void AASFogOfWarManager::ServerSetVisionDefinition(UASVisionDefinition* InVisionDefinition)
{
	if (!HasAuthority())
	{
		return;
	}
	VisionDefinition = InVisionDefinition;
	if (HasActorBegunPlay())
	{
		EnsureConfigured();
	}
	ForceNetUpdate();
}

void AASFogOfWarManager::SetWallOcclusionEnabled(bool bEnabled)
{
	if (!HasAuthority() || bWallOcclusionEnabled == bEnabled)
	{
		return;
	}
	bWallOcclusionEnabled = bEnabled;
	OnWallOcclusionChanged.Broadcast(bEnabled);
	ForceNetUpdate();
	UE_LOG(LogApocalypseSurvival, Log, TEXT("Wall occlusion %s."), bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

void AASFogOfWarManager::OnRep_WallOcclusionEnabled()
{
	OnWallOcclusionChanged.Broadcast(bWallOcclusionEnabled);
}

void AASFogOfWarManager::ResetExploredForAllPlayers()
{
	if (!HasAuthority())
	{
		return;
	}
	++ExploredResetCount;
	ResetExplored();
	ForceNetUpdate();
}

void AASFogOfWarManager::OnRep_ExploredResetCount()
{
	ResetExplored();
}

void AASFogOfWarManager::BeginPlay()
{
	Super::BeginPlay();

	if (UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this))
	{
		Subsystem->SetFogOfWarManager(this);
	}

	// 시야 정의가 복제될 때까지 낮은 빈도로 확인한 뒤 UpdateHz로 전환한다.
	ActiveInterval = 0.25f;
	GetWorldTimerManager().SetTimer(VisionTimerHandle, this, &AASFogOfWarManager::TickVision, ActiveInterval, true);
	EnsureConfigured();
}

void AASFogOfWarManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(VisionTimerHandle);
	FogDisplayComponent->RemoveWorldFog();

	if (UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this))
	{
		if (Subsystem->GetFogOfWarManager() == this)
		{
			Subsystem->SetFogOfWarManager(nullptr);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AASFogOfWarManager::TickVision()
{
	if (!EnsureConfigured(true))
	{
		return;
	}

	if (HasAuthority())
	{
		VisionEvaluatorComponent->ServerEvaluateVisibility();
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		FogDisplayComponent->UpdateLocalMask();
		FogDisplayComponent->UpdateWorldFog();
	}
}

bool AASFogOfWarManager::EnsureConfigured(bool bLogMissingDefinition)
{
	TArray<FString> Errors;
	if (!VisionDefinition)
	{
		// 복제나 같은 프레임의 설정을 기다리고, 서버 타이머에서만 빈 설정을 보고한다.
		if (!HasAuthority() || !bLogMissingDefinition)
		{
			return false;
		}
		Errors.Add(TEXT("VisionDefinition is null."));
	}
	else
	{
		VisionDefinition->ValidateDefinition(Errors);
	}
	if (Errors.Num() > 0)
	{
		if (!bConfigurationErrorLogged)
		{
			bConfigurationErrorLogged = true;
			UE_LOG(LogApocalypseSurvival, Error, TEXT("Vision configuration failed with %d error(s); visibility is not evaluated."), Errors.Num());
			for (const FString& Error : Errors)
			{
				UE_LOG(LogApocalypseSurvival, Error, TEXT("  %s"), *Error);
			}
		}
		FogDisplayComponent->RemoveWorldFog();
		return false;
	}

	if (HasAuthority() && !bWallOcclusionInitialized)
	{
		bWallOcclusionInitialized = true;
		SetWallOcclusionEnabled(VisionDefinition->Settings.bWallOcclusionByDefault);
	}
	VisionEvaluatorComponent->Configure(VisionDefinition);
	FogDisplayComponent->Configure(VisionDefinition);

	const float WantedInterval = 1.0f / FMath::Clamp(VisionDefinition->Settings.UpdateHz, 1.0f, 60.0f);
	if (!FMath::IsNearlyEqual(WantedInterval, ActiveInterval))
	{
		ActiveInterval = WantedInterval;
		GetWorldTimerManager().SetTimer(VisionTimerHandle, this, &AASFogOfWarManager::TickVision, ActiveInterval, true);
	}

	return true;
}

UTexture2D* AASFogOfWarManager::GetFogTexture() const
{
	return FogDisplayComponent->GetFogTexture();
}

bool AASFogOfWarManager::IsLocationExplored(const FVector& Location) const
{
	return FogDisplayComponent->IsLocationExplored(Location);
}

bool AASFogOfWarManager::IsLocationVisibleLocally(const FVector& Location) const
{
	return FogDisplayComponent->IsLocationVisibleLocally(Location);
}

void AASFogOfWarManager::ResetExplored()
{
	FogDisplayComponent->ResetExplored();
}

bool AASFogOfWarManager::CanSourceSeeTarget(
	const UASVisionSourceComponent* Source,
	const UASVisibilityComponent* Target,
	double ServerTime,
	bool bWallOcclusion) const
{
	return VisionEvaluatorComponent->CanSourceSeeTarget(Source, Target, ServerTime, bWallOcclusion);
}
