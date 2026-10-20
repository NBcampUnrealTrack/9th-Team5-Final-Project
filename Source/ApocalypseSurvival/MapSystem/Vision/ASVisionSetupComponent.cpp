#include "MapSystem/Vision/ASVisionSetupComponent.h"

#include "Core/Framework/ASGameState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MapSystem/Vision/ASFogOfWarManager.h"
#include "MapSystem/Vision/ASVisionDefinition.h"
#include "MapSystem/Zone/Core/ASMapFunctionLibrary.h"
#include "UObject/ConstructorHelpers.h"

UASVisionSetupComponent::UASVisionSetupComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	FogOfWarManagerClass = AASFogOfWarManager::StaticClass();
	static ConstructorHelpers::FObjectFinder<UASVisionDefinition> VisionDefinitionFinder(
		TEXT("/Game/ApocalypseSurvival/MapSystem/Data/DA_VisionDefinition.DA_VisionDefinition"));
	VisionDefinition = VisionDefinitionFinder.Object;
}

void UASVisionSetupComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World)
	{
		return;
	}

	if (!UASMapFunctionLibrary::LevelHasMapZones(this))
	{
		return;
	}

	// 시야 설정은 구역 매니저와 별도로 연결한다. 구역 액터가 없는 레벨에서는 위에서 이미 반환했다.
	TActorIterator<AASFogOfWarManager> ExistingFog(World);
	AASFogOfWarManager* FogManager = ExistingFog ? *ExistingFog : nullptr;
	if (!FogManager && FogOfWarManagerClass)
	{
		// BeginPlay 전에 정의를 지정해 스폰 중의 빈 설정 오류를 피한다.
		FogManager = World->SpawnActorDeferred<AASFogOfWarManager>(
			FogOfWarManagerClass, FTransform::Identity, Owner, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (FogManager)
		{
			FogManager->ServerSetVisionDefinition(VisionDefinition);
			FogManager->FinishSpawning(FTransform::Identity);
		}
	}
	else if (FogManager && !FogManager->GetVisionDefinition())
	{
		FogManager->ServerSetVisionDefinition(VisionDefinition);
	}

	FogOfWarManager = FogManager;
	if (FogManager)
	{
		GameState = World->GetGameState<AASGameState>();
		if (AASGameState* GS = GameState.Get())
		{
			GS->OnPhaseChanged.AddDynamic(this, &UASVisionSetupComponent::HandlePhaseChanged);
			// 시작 단계가 밤이면 낮에서 전환하지 않고 바로 밤 반경을 적용한다.
			ApplyPhase(GS->GetCurrentPhase(), true);
		}
	}
}

void UASVisionSetupComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AASGameState* GS = GameState.Get())
	{
		GS->OnPhaseChanged.RemoveDynamic(this, &UASVisionSetupComponent::HandlePhaseChanged);
	}
	GameState.Reset();
	FogOfWarManager.Reset();

	Super::EndPlay(EndPlayReason);
}

void UASVisionSetupComponent::HandlePhaseChanged(EGamePhase NewPhase)
{
	ApplyPhase(NewPhase, false);
}

void UASVisionSetupComponent::ApplyPhase(EGamePhase NewPhase, bool bImmediate)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (AASFogOfWarManager* Manager = FogOfWarManager.Get())
	{
		switch (NewPhase)
		{
		case EGamePhase::Night:
			Manager->SetNight(true, bImmediate);
			break;
		case EGamePhase::Day:
		case EGamePhase::Ready:
			Manager->SetNight(false, bImmediate);
			break;
		case EGamePhase::Victory:
		case EGamePhase::Defeat:
			break;
		}
	}
}

AASFogOfWarManager* UASVisionSetupComponent::GetFogOfWarManager() const
{
	return FogOfWarManager.Get();
}

void UASVisionSetupComponent::ResetExploredForAllPlayers()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (AASFogOfWarManager* Manager = FogOfWarManager.Get())
	{
		Manager->ResetExploredForAllPlayers();
	}
}
