#include "Map/Vision/ASFogOfWarManager.h"

#include "Components/SceneComponent.h"
#include "Map/Core/ASMapDefinition.h"
#include "Map/Core/ASMapFunctionLibrary.h"
#include "Map/Core/ASMapStateComponent.h"
#include "Map/Vision/ASFogDisplayComponent.h"
#include "Map/Vision/ASVisionEvaluatorComponent.h"
#include "Map/Vision/ASVisionWorldSubsystem.h"
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

void AASFogOfWarManager::BeginPlay()
{
	Super::BeginPlay();

	if (UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this))
	{
		Subsystem->SetFogOfWarManager(this);
	}

	// 맵 정의가 복제될 때까지 낮은 빈도로 확인한 뒤 UpdateHz로 전환한다.
	ActiveInterval = 0.25f;
	GetWorldTimerManager().SetTimer(VisionTimerHandle, this, &AASFogOfWarManager::TickVision, ActiveInterval, true);
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
	if (!EnsureConfigured())
	{
		return;
	}

	if (const UASMapStateComponent* MapState = UASMapFunctionLibrary::GetMapStateComponent(this))
	{
		const int32 MatchId = MapState->GetSnapshot().MatchId;
		if (MatchId != LastMatchId)
		{
			LastMatchId = MatchId;
			ResetExplored();
		}
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

bool AASFogOfWarManager::EnsureConfigured()
{
	UASMapDefinition* Definition = UASMapFunctionLibrary::GetMapDefinition(this);
	if (!Definition)
	{
		return false;
	}

	MapDefinition = Definition;
	VisionEvaluatorComponent->Configure(Definition);
	FogDisplayComponent->Configure(Definition);

	const float WantedInterval = 1.0f / FMath::Clamp(MapDefinition->Vision.UpdateHz, 1.0f, 60.0f);
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
