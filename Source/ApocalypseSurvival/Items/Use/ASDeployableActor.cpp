#include "Items/Use/ASDeployableActor.h"

#include "AbilitySystemComponent.h"
#include "Engine/DamageEvents.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "GameFramework/Controller.h"
#include "Items/GAS/ASItemEffects.h"
#include "Mass/Monster/Targeting/ASMonsterTargetableComponent.h"
#include "Net/UnrealNetwork.h"
#include "Skills/ASSkillTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASDeployableActor)

AASDeployableActor::AASDeployableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	HealthAttributes = CreateDefaultSubobject<UASHealthAttributeSet>(TEXT("HealthAttributes"));
	MonsterTargetableComponent = CreateDefaultSubobject<UASMonsterTargetableComponent>(TEXT("MonsterTargetableComponent"));
	MonsterTargetableComponent->ConfigureTargetProfile(
		EASMonsterTargetCapability::None, 0.0f, 0.0f);

	SetCanBeDamaged(false);
}

UAbilitySystemComponent* AASDeployableActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AASDeployableActor::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystemComponent->AddAttributeSetSubobject(HealthAttributes.Get());
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthChanged);
	ApplyPlacementState(bPlaced);
}

void AASDeployableActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
		.RemoveAll(this);
	ApplyPlacementState(false);
	Super::EndPlay(EndPlayReason);
}

bool AASDeployableActor::PreparePlacement(const float MaxHealth, const float Lifetime)
{
	if (!HasAuthority() || bPrepared || bPlaced || !FMath::IsFinite(MaxHealth) || MaxHealth <= 0.0f
		|| !FMath::IsFinite(Lifetime) || Lifetime < 0.0f)
	{
		return false;
	}

	AbilitySystemComponent->SetNumericAttributeBase(UASHealthAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UASHealthAttributeSet::GetHealthAttribute(), MaxHealth);
	PlacedLifetime = Lifetime;
	bPrepared = true;
	return true;
}

void AASDeployableActor::FinishPlacement()
{
	if (!HasAuthority() || !bPrepared || bPlaced)
	{
		return;
	}

	bPlaced = true;
	ApplyPlacementState(true);
	if (PlacedLifetime > 0.0f)
	{
		SetLifeSpan(PlacedLifetime);
	}
	ForceNetUpdate();
}

float AASDeployableActor::GetHealth() const
{
	return HealthAttributes->GetHealth();
}

bool AASDeployableActor::IsIntact() const
{
	return bPlaced && !IsActorBeingDestroyed() && GetHealth() > 0.0f;
}

bool AASDeployableActor::IsMonsterTargetable_Implementation() const
{
	return IsIntact();
}

FVector AASDeployableActor::GetMonsterTargetLocation_Implementation() const
{
	return GetActorLocation();
}

float AASDeployableActor::GetMonsterTargetRadius_Implementation() const
{
	return FMath::Max(0.0f, CalculateMonsterTargetRadius());
}

EASMonsterTargetCapability AASDeployableActor::GetMonsterTargetCapabilities_Implementation() const
{
	return static_cast<EASMonsterTargetCapability>(MonsterTargetCapabilityMask);
}

float AASDeployableActor::GetMonsterTargetBaseScore_Implementation() const
{
	return MonsterTargetBaseScore;
}

void AASDeployableActor::SetDefaultMonsterTargetProfile(
	const EASMonsterTargetCapability InCapabilities,
	const float InBaseScore)
{
	MonsterTargetCapabilityMask = static_cast<int32>(InCapabilities);
	MonsterTargetBaseScore = InBaseScore;
	MonsterTargetableComponent->ConfigureTargetProfile(InCapabilities, InBaseScore, 0.0f);
}

void AASDeployableActor::ApplyPlacementState(const bool bIsActive)
{
	SetCanBeDamaged(bIsActive);
	if (HasAuthority())
	{
		MonsterTargetableComponent->SetTargetingEnabled(bIsActive);
	}
}

float AASDeployableActor::CalculateMonsterTargetRadius() const
{
	return 0.0f;
}

float AASDeployableActor::TakeDamage(const float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || !IsIntact() || !CanBeDamaged()
		|| !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!IsIntact() || !FMath::IsFinite(ActualDamage) || ActualDamage <= 0.0f)
	{
		return 0.0f;
	}

	const float AppliedDamage = FMath::Min(GetHealth(), ActualDamage);
	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	Context.AddInstigator(EventInstigator ? EventInstigator->GetPawn() : DamageCauser, DamageCauser);
	const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(
		UASItemDamageEffect::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		return 0.0f;
	}

	Spec.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, -AppliedDamage);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	return AppliedDamage;
}

void AASDeployableActor::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (HasAuthority() && bPlaced && Data.NewValue <= 0.0f)
	{
		ApplyPlacementState(false);
		Destroy();
	}
}

void AASDeployableActor::OnRep_Placed()
{
	ApplyPlacementState(bPlaced);
}

void AASDeployableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AASDeployableActor, bPlaced);
}
