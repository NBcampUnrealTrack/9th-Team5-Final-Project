#include "Characters/Enemy/ASMonsterSlowFieldActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

TMap<TWeakObjectPtr<UAbilitySystemComponent>, AASMonsterSlowFieldActor::FSharedSlowState>
	AASMonsterSlowFieldActor::SharedSlowEffects;

AASMonsterSlowFieldActor::AASMonsterSlowFieldActor()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetupAttachment(SceneRoot);
	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AASMonsterSlowFieldActor::HandleBeginOverlap);
	TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &AASMonsterSlowFieldActor::HandleEndOverlap);
	ConfigureCollision();
}

void AASMonsterSlowFieldActor::BeginPlay()
{
	Super::BeginPlay();
	ConfigureCollision();
	BP_OnSlowFieldInitialized();

	if (!HasAuthority())
	{
		return;
	}

	TArray<AActor*> ExistingOverlaps;
	TriggerSphere->GetOverlappingActors(ExistingOverlaps, APawn::StaticClass());
	for (AActor* Actor : ExistingOverlaps)
	{
		ApplySlowToActor(Actor);
	}
}

void AASMonsterSlowFieldActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		ReleaseAllAffectedTargets();
	}

	Super::EndPlay(EndPlayReason);
}

void AASMonsterSlowFieldActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AASMonsterSlowFieldActor, Radius);
	DOREPLIFETIME_CONDITION(AASMonsterSlowFieldActor, Duration, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AASMonsterSlowFieldActor, StartServerTime, COND_InitialOnly);
}

void AASMonsterSlowFieldActor::InitializeField(const FGameplayEffectSpecHandle& InSlowEffectSpec,
	AActor* InInstigatorAvatar, const float InRadius, const float InDuration)
{
	if (!HasAuthority())
	{
		return;
	}

	SlowEffectSpec = InSlowEffectSpec;
	InstigatorAvatar = InInstigatorAvatar;
	Radius = FMath::Max(1.0f, InRadius);
	Duration = FMath::Max(0.05f, InDuration);
	StartServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	TriggerSphere->SetSphereRadius(Radius);
	SetLifeSpan(Duration);
	ForceNetUpdate();
}

float AASMonsterSlowFieldActor::GetProgress() const
{
	return Duration > 0.0f
		? FMath::Clamp((GetSynchronizedWorldTime() - StartServerTime) / Duration, 0.0f, 1.0f)
		: 1.0f;
}

void AASMonsterSlowFieldActor::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, const int32 OtherBodyIndex, const bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (HasAuthority())
	{
		ApplySlowToActor(OtherActor);
	}
}

void AASMonsterSlowFieldActor::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, const int32 OtherBodyIndex)
{
	if (HasAuthority())
	{
		RemoveSlowFromActor(OtherActor);
	}
}

void AASMonsterSlowFieldActor::ConfigureCollision()
{
	TriggerSphere->SetSphereRadius(FMath::Max(1.0f, Radius));
	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerSphere->SetGenerateOverlapEvents(true);
	TriggerSphere->SetCanEverAffectNavigation(false);
}

void AASMonsterSlowFieldActor::ApplySlowToActor(AActor* Actor)
{
	if (!IsEligibleTarget(Actor) || !SlowEffectSpec.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
	const TWeakObjectPtr<UAbilitySystemComponent> TargetKey(TargetASC);
	if (!IsValid(TargetASC) || AffectedAbilitySystems.Contains(TargetKey))
	{
		return;
	}

	FSharedSlowState& SharedState = SharedSlowEffects.FindOrAdd(TargetKey);
	if (SharedState.ReferenceCount == 0 || !SharedState.Handle.IsValid())
	{
		SharedState.Handle = TargetASC->ApplyGameplayEffectSpecToSelf(*SlowEffectSpec.Data.Get());
	}

	if (SharedState.Handle.IsValid())
	{
		++SharedState.ReferenceCount;
		AffectedAbilitySystems.Add(TargetKey);
	}
}

void AASMonsterSlowFieldActor::RemoveSlowFromActor(AActor* Actor)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
	const TWeakObjectPtr<UAbilitySystemComponent> TargetKey(TargetASC);
	if (!IsValid(TargetASC) || !AffectedAbilitySystems.Remove(TargetKey))
	{
		return;
	}

	FSharedSlowState* SharedState = SharedSlowEffects.Find(TargetKey);
	if (!SharedState)
	{
		return;
	}

	SharedState->ReferenceCount = FMath::Max(0, SharedState->ReferenceCount - 1);
	if (SharedState->ReferenceCount == 0)
	{
		if (SharedState->Handle.IsValid())
		{
			TargetASC->RemoveActiveGameplayEffect(SharedState->Handle);
		}
		SharedSlowEffects.Remove(TargetKey);
	}
}

void AASMonsterSlowFieldActor::ReleaseAllAffectedTargets()
{
	TArray<TWeakObjectPtr<UAbilitySystemComponent>> Targets;
	Targets.Reserve(AffectedAbilitySystems.Num());
	for (const TWeakObjectPtr<UAbilitySystemComponent>& TargetASC : AffectedAbilitySystems)
	{
		Targets.Add(TargetASC);
	}

	for (const TWeakObjectPtr<UAbilitySystemComponent>& TargetASC : Targets)
	{
		if (TargetASC.IsValid())
		{
			RemoveSlowFromActor(TargetASC->GetAvatarActor());
		}
	}
	AffectedAbilitySystems.Reset();
}

bool AASMonsterSlowFieldActor::IsEligibleTarget(const AActor* Actor) const
{
	return IsValid(Actor)
		&& Actor != InstigatorAvatar.Get()
		&& UASCombatTargetSubsystem::IsValidActorTarget(Actor, InstigatorAvatar.Get(), true);
}

float AASMonsterSlowFieldActor::GetSynchronizedWorldTime() const
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

void AASMonsterSlowFieldActor::OnRep_FieldConfig()
{
	ConfigureCollision();
	BP_OnSlowFieldInitialized();
}
