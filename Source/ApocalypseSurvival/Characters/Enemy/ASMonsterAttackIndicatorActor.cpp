#include "Characters/Enemy/ASMonsterAttackIndicatorActor.h"

#include "Components/SceneComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

AASMonsterAttackIndicatorActor::AASMonsterAttackIndicatorActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void AASMonsterAttackIndicatorActor::BeginPlay()
{
	Super::BeginPlay();

	// 서버는 FinishSpawning 이후, 클라이언트는 복제값이 이미 도착한 경우에만 초기화한다.
	NotifyIndicatorInitialized();
}

void AASMonsterAttackIndicatorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIndicatorInitialized)
	{
		BP_OnIndicatorProgress(GetProgress());
	}
}

void AASMonsterAttackIndicatorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AASMonsterAttackIndicatorActor, Shape);
	DOREPLIFETIME(AASMonsterAttackIndicatorActor, Radius);
	DOREPLIFETIME(AASMonsterAttackIndicatorActor, Length);
	DOREPLIFETIME(AASMonsterAttackIndicatorActor, Width);
	DOREPLIFETIME(AASMonsterAttackIndicatorActor, Duration);
	DOREPLIFETIME(AASMonsterAttackIndicatorActor, StartServerTime);
}

void AASMonsterAttackIndicatorActor::InitializeIndicator(EASMonsterAttackIndicatorShape InShape, float InRadius,
	float InLength, float InWidth, float InDuration)
{
	if (!HasAuthority())
	{
		return;
	}

	Shape = InShape;
	Radius = FMath::Max(0.0f, InRadius);
	Length = FMath::Max(0.0f, InLength);
	Width = FMath::Max(0.0f, InWidth);
	Duration = FMath::Max(0.01f, InDuration);
	StartServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	SetLifeSpan(Duration + 0.2f);
	ForceNetUpdate();
}

float AASMonsterAttackIndicatorActor::GetProgress() const
{
	if (Duration <= 0.0f)
	{
		return 1.0f;
	}

	return FMath::Clamp((GetSynchronizedWorldTime() - StartServerTime) / Duration, 0.0f, 1.0f);
}

void AASMonsterAttackIndicatorActor::OnRep_IndicatorConfig()
{
	NotifyIndicatorInitialized();
}

void AASMonsterAttackIndicatorActor::NotifyIndicatorInitialized()
{
	if (Duration <= 0.0f)
	{
		return;
	}

	bIndicatorInitialized = true;
	BP_OnIndicatorInitialized();
	BP_OnIndicatorProgress(GetProgress());
}

float AASMonsterAttackIndicatorActor::GetSynchronizedWorldTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return World->GetTimeSeconds();
}
