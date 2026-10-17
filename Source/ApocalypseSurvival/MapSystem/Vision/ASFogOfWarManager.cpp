#include "MapSystem/Vision/ASFogOfWarManager.h"

#include "ApocalypseSurvival.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MapSystem/Vision/ASFogDisplayComponent.h"
#include "MapSystem/Vision/ASVisibilityComponent.h"
#include "MapSystem/Vision/ASVisionDefinition.h"
#include "MapSystem/Vision/ASVisionEvaluatorComponent.h"
#include "MapSystem/Vision/ASVisionFunctionLibrary.h"
#include "MapSystem/Vision/ASVisionSourceComponent.h"
#include "MapSystem/Vision/ASVisionWorldSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

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
	DOREPLIFETIME(AASFogOfWarManager, NightBlend);
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

void AASFogOfWarManager::SetNight(bool bNight, bool bImmediate)
{
	const float Target = bNight ? 1.0f : 0.0f;
	if (!HasAuthority() || Target == NightBlend.ToAlpha)
	{
		return;
	}

	const double Now = UASVisionFunctionLibrary::GetServerTimeSeconds(this);
	const float BlendSeconds = VisionDefinition ? VisionDefinition->Settings.DayNightBlendSeconds : 0.0f;
	NightBlend.FromAlpha = bImmediate ? Target : NightBlend.Evaluate(Now, BlendSeconds);
	NightBlend.ToAlpha = Target;
	NightBlend.StartServerTime = Now;
	ForceNetUpdate();
	UE_LOG(LogApocalypseSurvival, Log, TEXT("Vision night target %.0f (from %.3f, immediate=%s)."),
		Target, NightBlend.FromAlpha, bImmediate ? TEXT("true") : TEXT("false"));
}

float AASFogOfWarManager::GetNightAlpha() const
{
	const float BlendSeconds = VisionDefinition ? VisionDefinition->Settings.DayNightBlendSeconds : 0.0f;
	return NightBlend.Evaluate(UASVisionFunctionLibrary::GetServerTimeSeconds(this), BlendSeconds);
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
	if (UWorld* World = GetWorld(); World && ActorSpawnedDelegateHandle.IsValid())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedDelegateHandle);
	}
	ActorSpawnedDelegateHandle.Reset();

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

	if (HasAuthority() && !bAutoAttachInitialized)
	{
		if (UWorld* World = GetWorld())
		{
			bAutoAttachInitialized = true;
			ActorSpawnedDelegateHandle = World->AddOnActorSpawnedHandler(
				FOnActorSpawned::FDelegate::CreateWeakLambda(this, [this](AActor* Actor)
				{
					AutoAttachVisionComponents(Actor);
				}));

			int32 AttachedCount = 0;
			for (AActor* Actor : TActorRange<AActor>(World))
			{
				AttachedCount += AutoAttachVisionComponents(Actor);
			}
			UE_LOG(LogApocalypseSurvival, Log, TEXT("Vision auto attach initial scan added %d component(s)."), AttachedCount);
		}
	}

	return true;
}

int32 AASFogOfWarManager::AutoAttachVisionComponents(AActor* Actor)
{
	if (!HasAuthority() || !VisionDefinition || !IsValid(Actor) || Actor->IsActorBeingDestroyed())
	{
		return 0;
	}

	const auto Matches = [Actor](const TArray<TSubclassOf<AActor>>& Classes)
	{
		return Classes.ContainsByPredicate([Actor](const TSubclassOf<AActor>& ActorClass)
		{
			return ActorClass && Actor->IsA(ActorClass.Get());
		});
	};

	int32 AttachedCount = 0;
	if (!Actor->FindComponentByClass<UASVisionSourceComponent>() && Matches(VisionDefinition->AutoVisionSourceClasses))
	{
		UASVisionSourceComponent* Component = NewObject<UASVisionSourceComponent>(Actor, TEXT("AutoVisionSource"));
		Actor->AddInstanceComponent(Component);
		// 이미 시작한 액터는 즉시, 지연 스폰 액터는 소유자와 함께 BeginPlay를 실행한다.
		Component->RegisterComponent();
		++AttachedCount;
	}

	if (!Actor->FindComponentByClass<UASVisibilityComponent>() && Matches(VisionDefinition->AutoVisibilityTargetClasses))
	{
		UASVisibilityComponent* Component = NewObject<UASVisibilityComponent>(Actor, TEXT("AutoVisibility"));
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		++AttachedCount;
	}

	return AttachedCount;
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
