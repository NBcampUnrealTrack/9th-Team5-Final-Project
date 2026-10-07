#include "MapSystem/Vision/ASVisionSetupComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "MapSystem/Vision/ASFogOfWarManager.h"
#include "MapSystem/Vision/ASVisionDefinition.h"

UASVisionSetupComponent::UASVisionSetupComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	FogOfWarManagerClass = AASFogOfWarManager::StaticClass();
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

	// 시야 설정은 구역 시스템과 독립적으로 연결한다.
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
