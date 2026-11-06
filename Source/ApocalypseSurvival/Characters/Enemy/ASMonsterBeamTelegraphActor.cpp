#include "Characters/Enemy/ASMonsterBeamTelegraphActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

AASMonsterBeamTelegraphActor::AASMonsterBeamTelegraphActor()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void AASMonsterBeamTelegraphActor::BeginPlay()
{
	Super::BeginPlay();
	NotifyBeamInitialized();
}

void AASMonsterBeamTelegraphActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bInitialized)
	{
		BP_OnBeamProgress(GetProgress());
	}
}

void AASMonsterBeamTelegraphActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AASMonsterBeamTelegraphActor, Length);
	DOREPLIFETIME(AASMonsterBeamTelegraphActor, Width);
	DOREPLIFETIME(AASMonsterBeamTelegraphActor, TotalDuration);
	DOREPLIFETIME(AASMonsterBeamTelegraphActor, TrackingDuration);
	DOREPLIFETIME(AASMonsterBeamTelegraphActor, StartServerTime);
	DOREPLIFETIME(AASMonsterBeamTelegraphActor, Phase);
}

void AASMonsterBeamTelegraphActor::InitializeBeam(const FVector& InStartLocation, const FVector& InDirection,
	const float InLength, const float InWidth, const float InTotalDuration, const float InTrackingDuration)
{
	if (!HasAuthority())
	{
		return;
	}

	Length = FMath::Max(1.0f, InLength);
	Width = FMath::Max(1.0f, InWidth);
	TotalDuration = FMath::Max(0.01f, InTotalDuration);
	TrackingDuration = FMath::Clamp(InTrackingDuration, 0.0f, TotalDuration);
	StartServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Phase = EASMonsterBeamTelegraphPhase::Tracking;
	SetActorLocationAndRotation(InStartLocation, InDirection.GetSafeNormal2D().Rotation());
	SetLifeSpan(TotalDuration + 0.3f);
	ForceNetUpdate();
}

void AASMonsterBeamTelegraphActor::UpdateTrackingTransform(const FVector& InStartLocation,
	const FVector& InDirection, const float InLength)
{
	if (!HasAuthority() || Phase != EASMonsterBeamTelegraphPhase::Tracking)
	{
		return;
	}

	Length = FMath::Max(1.0f, InLength);
	SetActorLocationAndRotation(InStartLocation, InDirection.GetSafeNormal2D().Rotation());
	ForceNetUpdate();
}

void AASMonsterBeamTelegraphActor::SetPhase(const EASMonsterBeamTelegraphPhase InPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	Phase = InPhase;
	ForceNetUpdate();
}

void AASMonsterBeamTelegraphActor::NotifyBeamFired()
{
	if (!HasAuthority() || bFiredFeedbackPlayed) return;
	MulticastBeamFired(GetActorLocation(), GetActorRotation());
}

void AASMonsterBeamTelegraphActor::MulticastBeamFired_Implementation(const FVector_NetQuantize Location, const FRotator Rotation)
{
	if (bFiredFeedbackPlayed) return;
	bFiredFeedbackPlayed = true;
	if (GetNetMode() == NM_DedicatedServer) return;
	// The final tracking movement may not have replicated before this event.
	SetActorLocationAndRotation(Location, Rotation);
	BP_OnBeamFired();
}

void AASMonsterBeamTelegraphActor::ReleaseAfterFeedback()
{
	if (!HasAuthority()) return;
	if (bFiredFeedbackPlayed)
	{
		SetActorTickEnabled(false);
		SetActorHiddenInGame(true);
		ForceNetUpdate();
		SetLifeSpan(0.2f);
	}
	else
	{
		Destroy();
	}
}

float AASMonsterBeamTelegraphActor::GetProgress() const
{
	return TotalDuration > 0.0f
		? FMath::Clamp((GetSynchronizedWorldTime() - StartServerTime) / TotalDuration, 0.0f, 1.0f)
		: 1.0f;
}

void AASMonsterBeamTelegraphActor::OnRep_BeamConfig()
{
	NotifyBeamInitialized();
}

void AASMonsterBeamTelegraphActor::NotifyBeamInitialized()
{
	if (TotalDuration <= 0.0f)
	{
		return;
	}

	bInitialized = true;
	BP_OnBeamInitialized();
	BP_OnBeamProgress(GetProgress());
}

float AASMonsterBeamTelegraphActor::GetSynchronizedWorldTime() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}

	return 0.0f;
}
