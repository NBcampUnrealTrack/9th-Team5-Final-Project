#include "Characters/Enemy/ASMonsterCharacter.h"

#include "Characters/Enemy/AI/ASMonsterAIController.h"
#include "Characters/Enemy/Components/ASMonsterCombatComponent.h"
#include "Characters/Enemy/Components/ASMonsterTargetingComponent.h"
#include "Characters/Enemy/Data/ASMonsterDefinition.h"
#include "Characters/Enemy/Data/ASMonsterTags.h"
#include "Core/Team/ASTeamTypes.h"
#include "GameplayAbilitySpec.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Attributes/ASAttributeSet.h"

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
	OwnedAttributeSet = CreateDefaultSubobject<UASAttributeSet>(TEXT("AttributeSet"));

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
	InitAbilitySystem(OwnedAbilitySystemComponent, OwnedAttributeSet, this);

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

	Super::EndPlay(EndPlayReason);
}

void AASMonsterCharacter::OnAbilitySystemInitialized()
{
	Super::OnAbilitySystemInitialized();

	// 다시 불려도 구독이 겹치지 않게 먼저 푼다.
	UnbindHealthDelegate();
	HealthChangedHandle = OwnedAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UASAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AASMonsterCharacter::HandleHealthChanged);
}

bool AASMonsterCharacter::IsDead() const
{
	return IsValid(OwnedAbilitySystemComponent) && OwnedAbilitySystemComponent->HasMatchingGameplayTag(ASGameplayTags::State_Monster_Dead);
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

void AASMonsterCharacter::ApplyMonsterDefinition()
{
	if (!MonsterDefinition)
	{
		return;
	}

	const FASMonsterInitialAttributes& InitialAttributes = MonsterDefinition->InitialAttributes;
	OwnedAttributeSet->InitMaxHealth(InitialAttributes.MaxHealth);
	OwnedAttributeSet->InitHealth(InitialAttributes.MaxHealth);
	OwnedAttributeSet->InitMoveSpeed(InitialAttributes.MoveSpeed);
	OwnedAttributeSet->InitAttackPower(InitialAttributes.AttackPower);
	GetCharacterMovement()->MaxWalkSpeed = InitialAttributes.MoveSpeed;

	CombatComponent->Configure(MonsterDefinition->AttackRange, MonsterDefinition->AttackAbilityTag);
	TargetingComponent->Configure(MonsterDefinition->DetectionRadius, MonsterDefinition->LoseTargetRadius);

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

	// Minimal 복제 모드에서도 태그는 복제된다. 일반 Loose 태그는 서버에만 남으므로 복제 버전을 쓴다.
	OwnedAbilitySystemComponent->AddReplicatedLooseGameplayTag(ASGameplayTags::State_Monster_Dead);

	StopMovementAndAI();
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

void AASMonsterCharacter::UnbindHealthDelegate()
{
	if (IsValid(OwnedAbilitySystemComponent) && HealthChangedHandle.IsValid())
	{
		OwnedAbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UASAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedHandle);
	}

	HealthChangedHandle.Reset();
}
