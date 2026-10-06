#include "Map/Manager/ASMapGameFlowComponent.h"

#include "ApocalypseSurvival.h"
#include "Core/Framework/ASGameState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Map/Core/ASMapDefinition.h"
#include "Map/Core/ASMapFunctionLibrary.h"
#include "Map/Manager/ASMapSystemManager.h"
#include "Map/Vision/ASFogOfWarManager.h"
#include "Map/Vision/ASVisionDefinition.h"
#include "Map/Vision/ASVisionWorldSubsystem.h"

UASMapGameFlowComponent::UASMapGameFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	MapManagerClass = AASMapSystemManager::StaticClass();
	FogOfWarManagerClass = AASFogOfWarManager::StaticClass();
}

void UASMapGameFlowComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World)
	{
		return;
	}

	// 시야 설정은 맵 상태나 구역 검증의 성공 여부와 관계없이 먼저 연결한다.
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

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	GameState = World->GetGameState<AASGameState>();
	AASGameState* GS = GameState.Get();
	UASMapStateComponent* MapState = UASMapFunctionLibrary::GetMapStateComponent(this);
	if (!GS || !MapState)
	{
		UE_LOG(LogApocalypseSurvival, Error, TEXT("Map flow requires AASGameState with a map state component."));
		return;
	}

	// OnMapReady를 동기적으로 브로드캐스트하는 초기화 전에 구독한다.
	GS->OnPhaseChanged.AddDynamic(this, &UASMapGameFlowComponent::HandlePhaseChanged);
	HandlePhaseChanged(GS->GetCurrentPhase());

	MapManager = World->SpawnActor<AASMapSystemManager>(
		MapManagerClass ? MapManagerClass.Get() : AASMapSystemManager::StaticClass(),
		FTransform::Identity,
		SpawnParams);
	if (!MapManager)
	{
		UE_LOG(LogApocalypseSurvival, Error, TEXT("Map flow could not spawn the map manager."));
		return;
	}

	MapManager->OnMapReady.AddDynamic(this, &UASMapGameFlowComponent::HandleMapReady);
	if (!MapManager->InitializeMap(MapDefinition, MapState))
	{
		UE_LOG(LogApocalypseSurvival, Error, TEXT("Map flow could not initialize the map. Fix the errors above."));
	}
}

void UASMapGameFlowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AASGameState* GS = GameState.Get())
	{
		GS->OnPhaseChanged.RemoveDynamic(this, &UASMapGameFlowComponent::HandlePhaseChanged);
	}
	if (IsValid(MapManager))
	{
		MapManager->OnMapReady.RemoveDynamic(this, &UASMapGameFlowComponent::HandleMapReady);
		MapManager->StopMap();
	}
	GameState.Reset();
	MapManager = nullptr;
	bHasPendingPhase = false;

	Super::EndPlay(EndPlayReason);
}

void UASMapGameFlowComponent::HandleMapReady(AASMapSystemManager* Manager)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Manager != MapManager || !IsValid(Manager))
	{
		return;
	}

	const int32 Seed = MatchSeed != 0 ? MatchSeed : FMath::RandRange(1, MAX_int32 - 1);
	if (Manager->StartMap(Seed))
	{
		ApplyCurrentPhase();
	}
}

void UASMapGameFlowComponent::RestartMap(int32 NewSeed)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(MapManager) || !MapManager->IsMapReady())
	{
		return;
	}

	MapManager->StopMap();
	const int32 Seed = NewSeed != 0 ? NewSeed : FMath::RandRange(1, MAX_int32 - 1);
	if (MapManager->StartMap(Seed))
	{
		if (const UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this))
		{
			if (AASFogOfWarManager* FogManager = Subsystem->GetFogOfWarManager())
			{
				FogManager->ResetExploredForAllPlayers();
			}
		}
		ApplyCurrentPhase();
	}
}

void UASMapGameFlowComponent::HandlePhaseChanged(EGamePhase NewPhase)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	switch (NewPhase)
	{
	case EGamePhase::Day:
	case EGamePhase::Night:
		if (const AASGameState* GS = GameState.Get())
		{
			FASMapPhaseContext Context;
			Context.Day = GS->GetCurrentDay();
			Context.DayPhase = NewPhase == EGamePhase::Day ? EASDayPhase::Day : EASDayPhase::Night;

			if (IsValid(MapManager) && MapManager->IsMapRunning())
			{
				bHasPendingPhase = false;
				MapManager->HandlePhaseChanged(Context);
			}
			else
			{
				PendingPhase = Context;
				bHasPendingPhase = true;
			}
		}
		break;
	case EGamePhase::Victory:
	case EGamePhase::Defeat:
		bHasPendingPhase = false;
		if (IsValid(MapManager))
		{
			MapManager->StopMap();
		}
		break;
	case EGamePhase::Ready:
		break;
	}
}

void UASMapGameFlowComponent::ApplyCurrentPhase()
{
	// 시작과 재시작에 최신 단계를 사용하도록 게임 시계에서 갱신한다.
	if (const AASGameState* GS = GameState.Get())
	{
		HandlePhaseChanged(GS->GetCurrentPhase());
	}
	if (bHasPendingPhase && IsValid(MapManager) && MapManager->IsMapRunning())
	{
		const FASMapPhaseContext Context = PendingPhase;
		bHasPendingPhase = false;
		MapManager->HandlePhaseChanged(Context);
	}
}
