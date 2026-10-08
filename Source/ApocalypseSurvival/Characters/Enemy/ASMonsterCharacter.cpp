#include "Characters/Enemy/ASMonsterCharacter.h"

#include "Characters/Enemy/AI/ASMonsterAIController.h"
#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Core/Team/ASTeamTypes.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Attributes/ASCombatAttributeSet.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "Skills/ASSkillTags.h"

AASMonsterCharacter::AASMonsterCharacter()
{
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(10.0f);
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AASMonsterAIController::StaticClass();
	TeamId = ASTeam::Monster();

	OwnedAbilitySystemComponent = CreateDefaultSubobject<UASAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	// 일반 몬스터는 소유 클라이언트가 없으므로 전체 Effect 목록 대신 태그/큐/Attribute만 동기화한다.
	OwnedAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	OwnedHealthAttributeSet = CreateDefaultSubobject<UASHealthAttributeSet>(TEXT("HealthAttributeSet"));
	OwnedCombatAttributeSet = CreateDefaultSubobject<UASCombatAttributeSet>(TEXT("CombatAttributeSet"));

	TargetingComponent = CreateDefaultSubobject<UASMonsterTargetingComponent>(TEXT("TargetingComponent"));
	CombatComponent = CreateDefaultSubobject<UASMonsterCombatComponent>(TEXT("CombatComponent"));

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->bUseRVOAvoidance = true;
	MovementComponent->AvoidanceConsiderationRadius = 200.0f;
}

void AASMonsterCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 적은 ASC와 Avatar가 모두 자기 자신이다.
	InitAbilitySystem(OwnedAbilitySystemComponent, this);

	if (HasAuthority())
	{
		ApplyMonsterDefinition();

		// 초기 능력치와 Ability 가 바뀐 직후라 다음 복제 주기를 기다리지 않고 바로 내보낸다.
		ForceNetUpdate();

		// AI 는 이 신호를 받은 뒤에 시작한다. Definition 과 GAS 가 모두 준비된 시점이다.
		bMonsterInitialized = true;
		OnMonsterInitialized.Broadcast();
	}
}

void AASMonsterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindHealthDelegate();
	UnbindDamageEffectDelegate();

	Super::EndPlay(EndPlayReason);
}

void AASMonsterCharacter::OnAbilitySystemInitialized()
{
	Super::OnAbilitySystemInitialized();

	// 다시 불려도 구독이 겹치지 않게 먼저 푼다.
	UnbindHealthDelegate();
	HealthChangedHandle = OwnedAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AASMonsterCharacter::HandleHealthChanged);

	UnbindDamageEffectDelegate();
	DamageEffectAppliedHandle = OwnedAbilitySystemComponent->OnGameplayEffectAppliedDelegateToSelf
		.AddUObject(this, &AASMonsterCharacter::HandleGameplayEffectApplied);
}

bool AASMonsterCharacter::IsDead() const
{
	return IsValid(OwnedAbilitySystemComponent) && OwnedAbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Monster_Dead);
}

bool AASMonsterCharacter::IsHitReacting() const
{
	return IsValid(OwnedAbilitySystemComponent) && OwnedAbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Monster_HitReact);
}

void AASMonsterCharacter::SetFaceControllerFocus(bool bFaceControllerFocus)
{
	if (!HasAuthority())
	{
		return;
	}

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->bOrientRotationToMovement = !bFaceControllerFocus;
	MovementComponent->bUseControllerDesiredRotation = bFaceControllerFocus;
}

void AASMonsterCharacter::NotifyDamagedBy(AActor* InstigatorActor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AASMonsterAIController* AIController = Cast<AASMonsterAIController>(GetController()))
	{
		AIController->NotifyMonsterDamaged(InstigatorActor);
	}
}

void AASMonsterCharacter::ApplyMonsterDefinition()
{
	if (!MonsterDefinition)
	{
		return;
	}

	const FASMonsterInitialAttributes& InitialAttributes = MonsterDefinition->InitialAttributes;
	OwnedHealthAttributeSet->InitMaxHealth(InitialAttributes.MaxHealth);
	OwnedHealthAttributeSet->InitHealth(InitialAttributes.MaxHealth);
	OwnedCombatAttributeSet->InitMoveSpeed(InitialAttributes.MoveSpeed);
	OwnedCombatAttributeSet->InitAttackPower(InitialAttributes.AttackPower);
	GetCharacterMovement()->MaxWalkSpeed = InitialAttributes.MoveSpeed;

	CombatComponent->Configure(MonsterDefinition->AttackRange, MonsterDefinition->AttackAbilityTag);
	TargetingComponent->Configure(MonsterDefinition->DetectionRadius, MonsterDefinition->LoseTargetRadius);
	if (MonsterDefinition->GrantedAbilities.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Monster] %s has no GrantedAbilities. Configure a monster attack Ability in its Definition."),
			*GetNameSafe(MonsterDefinition));
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : MonsterDefinition->GrantedAbilities)
	{
		if (AbilityClass)
		{
			OwnedAbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
		}
	}
}

void AASMonsterCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	// 죽음 판정은 서버만 한다. 클라이언트는 복제된 Dead 태그로 알게 된다.
	if (HasAuthority() && Data.NewValue <= 0.0f)
	{
		HandleDeath();
	}
}

void AASMonsterCharacter::HandleDeath()
{
	if (IsDead())
	{
		return;
	}

	// Minimal 복제 모드에서는 GameplayEffect가 주는 태그처럼 TagOnly 상태로 복제해야 한다.
	// 사망 GameplayEffect를 만들기 전까지는 이 API로 Dead 태그를 모든 클라이언트에 전달한다.
	OwnedAbilitySystemComponent->AddMinimalReplicationGameplayTag(ASGameplayTags::State_Monster_Dead);

	// 사망 직전의 공격 몽타주/타격 이벤트가 계속되는 것을 막는다.
	FGameplayTagContainer AttackAbilityTags;
	AttackAbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack);
	OwnedAbilitySystemComponent->CancelAbilities(&AttackAbilityTags, nullptr, nullptr);

	StopMovementAndAI();
	SetCanBeDamaged(false);
	// CharacterMovement는 Capsule 충돌을 기준으로 이동한다. Actor 플래그뿐 아니라 Capsule을
	// 직접 끄고 NavMesh 장애물에서도 빼야 사망 대기 중 길막하지 않는다.
	SetActorEnableCollision(false);
	if (UCapsuleComponent* CollisionComponent = GetCapsuleComponent())
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionComponent->SetCanEverAffectNavigation(false);
	}
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetCanEverAffectNavigation(false);
	}

	const float DespawnDelay = MonsterDefinition ? MonsterDefinition->DeathDespawnDelay : 1.5f;
	if (DespawnDelay <= 0.0f)
	{
		Destroy();
	}
	else
	{
		SetLifeSpan(DespawnDelay);
	}
}

void AASMonsterCharacter::StopMovementAndAI()
{
	if (AASMonsterAIController* AIController = Cast<AASMonsterAIController>(GetController()))
	{
		AIController->StopAI(TEXT("Dead"));
	}

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->StopMovementImmediately();
	MovementComponent->DisableMovement();
}

void AASMonsterCharacter::ApplyHitReact()
{
	if (!HasAuthority() || IsDead() || !MonsterDefinition || !MonsterDefinition->HitReactEffect)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = OwnedAbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	const FGameplayEffectSpecHandle HitReactSpec = OwnedAbilitySystemComponent->MakeOutgoingSpec(
		MonsterDefinition->HitReactEffect, 1.0f, EffectContext);
	if (!HitReactSpec.IsValid())
	{
		return;
	}

	OwnedAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*HitReactSpec.Data.Get());

	// 새 공격은 HitReact 태그로 막고, 이미 진행 중인 공격은 즉시 끝낸다.
	FGameplayTagContainer AttackAbilityTags;
	AttackAbilityTags.AddTag(ASGameplayTags::Ability_Monster_Attack);
	OwnedAbilitySystemComponent->CancelAbilities(&AttackAbilityTags, nullptr, nullptr);

	if (AASMonsterAIController* AIController = Cast<AASMonsterAIController>(GetController()))
	{
		AIController->NotifyMonsterHitReact();
	}
}

void AASMonsterCharacter::UnbindHealthDelegate()
{
	if (IsValid(OwnedAbilitySystemComponent) && HealthChangedHandle.IsValid())
	{
		OwnedAbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedHandle);
	}

	HealthChangedHandle.Reset();
}

void AASMonsterCharacter::UnbindDamageEffectDelegate()
{
	if (IsValid(OwnedAbilitySystemComponent) && DamageEffectAppliedHandle.IsValid())
	{
		OwnedAbilitySystemComponent->OnGameplayEffectAppliedDelegateToSelf.Remove(DamageEffectAppliedHandle);
	}

	DamageEffectAppliedHandle.Reset();
}

void AASMonsterCharacter::HandleGameplayEffectApplied(UAbilitySystemComponent* TargetAbilitySystemComponent,
	const FGameplayEffectSpec& AppliedEffect, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	if (!HasAuthority())
	{
		return;
	}

	// 프로젝트의 피해 Effect는 Data.Damage를 음수로 넣어 Health에 더한다.
	const float DamageMagnitude = AppliedEffect.GetSetByCallerMagnitude(ASGameplayTags::Data_Damage, false, 0.0f);
	if (DamageMagnitude >= 0.0f)
	{
		return;
	}

	// 치명타는 체력 변경 콜백에서 이미 Dead 상태가 됐으므로 경직과 어그로를 만들지 않는다.
	if (IsDead())
	{
		return;
	}

	const FGameplayEffectContextHandle& EffectContext = AppliedEffect.GetContext();
	AActor* InstigatorActor = EffectContext.GetOriginalInstigator();
	if (!IsValid(InstigatorActor))
	{
		InstigatorActor = EffectContext.GetInstigator();
	}

	// 투사체가 이미 파괴됐더라도 OriginalInstigator(발사한 플레이어)를 우선 사용한다.
	NotifyDamagedBy(InstigatorActor);
	ApplyHitReact();
}
