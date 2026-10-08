#include "Characters/Player/ASPlayerCharacter.h"

#include "Characters/Player/Components/ASEquipmentComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/Framework/ASGameMode.h"
#include "Core/Framework/ASPlayerState.h"
#include "Core/Team/ASTeamTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectExtension.h"
#include "GAS/ASAbilitySet.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/ASAbilityTags.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "Items/ASItemInstance.h"

AASPlayerCharacter::AASPlayerCharacter()
{
	TeamId = ASTeam::Player();

	EquipmentComponent = CreateDefaultSubobject<UASEquipmentComponent>(TEXT("EquipmentComponent"));

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// 카메라가 회전하지 않으므로 캐릭터는 컨트롤러 방향이 아니라 걷는 방향을 본다.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->RotationRate = FRotator(0.0f, 1080.0f, 0.0f);
	MoveComp->bConstrainToPlane = true;
	MoveComp->bSnapToPlaneAtStart = true;

	// 클릭하면 바로 출발하고 바로 멈추게 한다. 롤 계열 캐릭터의 반응에 맞춘 값이다.
	MoveComp->MaxAcceleration = 8192.0f;
	MoveComp->BrakingDecelerationWalking = 8192.0f;

	// 길찾기 이동을 "가속(입력)"으로 굴리게 한다.
	// 기본값은 속도를 직접 넣는 방식인데, 클라이언트가 서버로 보내는 이동 기록에는
	// 그 속도가 들어가지 않는다. 그러면 서버는 캐릭터가 서 있다고 계산해서
	// 본인 클라이언트의 위치를 매 프레임 되돌려 놓는다.
	MoveComp->GetNavMovementProperties()->bUseAccelerationForPaths = true;
}

void AASPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	AASPlayerState* ASPlayerState = GetPlayerState<AASPlayerState>();

	// 리스폰이면 이전 캐릭터의 사망 상태가 ASC 에 남아 있다. 새 몸을 받았으니 걷어 낸다.
	// ASC 연결보다 먼저 해야 한다. 연결 직후 "이미 죽어 있나" 검사가 도는데,
	// 태그가 남아 있으면 새 몸에 사망 처리(이동·충돌 끄기)가 걸려 버린다.
	if (IsValid(ASPlayerState))
	{
		ASPlayerState->ResetForRespawn();
	}

	InitAbilitySystemFromPlayerState();

	if (IsValid(ASPlayerState))
	{
		ASPlayerState->GrantDefaultAbilitySetOnce(DefaultAbilitySet);
	}

	// 리스폰이면 PlayerState 에 장착 기록이 남아 있다. 옛 몸과 함께 사라진 무기를 새 몸에 다시 단다.
	// ASC 연결 뒤여야 공격 어빌리티를 부여할 수 있어서 맨 마지막에 한다.
	if (IsValid(ASPlayerState))
	{
		if (UASItemInstance* EquippedItem = ASPlayerState->GetEquippedItemRecord())
		{
			EquipmentComponent->EquipItem(EquippedItem);
		}
	}
}

void AASPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitAbilitySystemFromPlayerState();
}

bool AASPlayerCharacter::IsDead() const
{
	const UASAbilitySystemComponent* ASC = Cast<UASAbilitySystemComponent>(GetAbilitySystemComponent());
	return IsValid(ASC) && ASC->IsDead();
}

void AASPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ASC 는 PlayerState 에 남으므로 파괴되는 캐릭터의 구독을 반드시 풀어야 한다.
	UnbindDeathDelegates();

	Super::EndPlay(EndPlayReason);
}

void AASPlayerCharacter::OnAbilitySystemInitialized()
{
	Super::OnAbilitySystemInitialized();

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	// PlayerState 가 다시 복제되면 또 불릴 수 있다. 두 번 걸리지 않게 먼저 푼다.
	UnbindDeathDelegates();

	// 죽음 판정은 서버만 한다. 클라이언트는 아래 태그로 알게 된다.
	if (HasAuthority())
	{
		HealthChangedHandle = ASC
			->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
			.AddUObject(this, &AASPlayerCharacter::HandleHealthChanged);
	}

	DeadTagChangedHandle = ASC
		->RegisterGameplayTagEvent(ASGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &AASPlayerCharacter::HandleDeadTagChanged);

	// 늦게 접속한 클라이언트는 이미 죽어 있는 플레이어를 받을 수 있다. 태그 알림은 그때 오지 않는다.
	if (IsDead())
	{
		ApplyLocalDeath();
	}
}

void AASPlayerCharacter::InitAbilitySystemFromPlayerState()
{
	AASPlayerState* ASPlayerState = GetPlayerState<AASPlayerState>();
	if (!IsValid(ASPlayerState))
	{
		return;
	}

	// ASC 의 주인은 PlayerState, 아바타는 이 캐릭터다.
	InitAbilitySystem(ASPlayerState->GetASAbilitySystemComponent(), ASPlayerState);
}

void AASPlayerCharacter::UnbindDeathDelegates()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (HealthChangedHandle.IsValid())
		{
			ASC
				->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
				.Remove(HealthChangedHandle);
		}

		if (DeadTagChangedHandle.IsValid())
		{
			ASC
				->RegisterGameplayTagEvent(ASGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
				.Remove(DeadTagChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	DeadTagChangedHandle.Reset();
}

void AASPlayerCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (Data.NewValue > 0.0f || !HasAuthority() || !IsValid(ASC) || IsDead())
	{
		return;
	}

	// 누가 죽였는지를 이벤트에 싣는다. 효과로 깎인 경우에만 알 수 있고, 디버그 명령처럼 직접 바꾼 경우는 비어 있다.
	FGameplayEventData Payload;
	Payload.EventTag = ASGameplayTags::GameplayEvent_Death;
	Payload.Target = this;
	if (Data.GEModData != nullptr)
	{
		Payload.ContextHandle = Data.GEModData->EffectSpec.GetEffectContext();
		Payload.Instigator = Payload.ContextHandle.GetOriginalInstigator();
		Payload.OptionalObject = Data.GEModData->EffectSpec.Def;
	}

	// 사망 어빌리티가 이 이벤트로 켜진다. 돌려주는 값은 켜진 어빌리티 수다.
	const int32 ActivatedCount = ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
	if (ActivatedCount == 0)
	{
		// 어빌리티 세트에 사망 어빌리티가 없다. 연출 없이 바로 죽인다. 리스폰이 막히는 것보다 낫다.
		UE_LOG(LogTemp, Warning, TEXT("[Player] %s 사망 어빌리티가 없어 연출 없이 사망 처리한다. DA_AbilitySet_Player 에 GA_Death 를 넣어야 한다."), *GetName());
		ASC->CancelAllAbilities();
		StartDeath();
		FinishDeath();
	}
}

void AASPlayerCharacter::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		ApplyLocalDeath();
	}
	else
	{
		// 클라이언트에서는 새 캐릭터가 태그 제거보다 먼저 도착할 수 있다. 그러면 새 몸이
		// 연결 직후 검사에서 죽은 것으로 보여 사망 처리가 걸리고, 뒤늦게 태그가 떨어진다.
		// 그때 되돌려 주지 않으면 충돌이 꺼진 채 걷기 시작해 바닥을 뚫고 떨어진다.
		RestoreFromDeath();
	}
}

void AASPlayerCharacter::StartDeath()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !IsValid(ASC) || IsDead())
	{
		return;
	}

	// 모든 머신에 복제되는 태그다. Minimal 방식은 Mixed 모드에서 본인 클라이언트가 받지 못해 쓰지 않는다.
	// 이 줄에서 서버의 HandleDeadTagChanged 가 바로 불린다. 다른 어빌리티 취소는 사망 어빌리티가 맡는다.
	ASC->AddLooseGameplayTag(ASGameplayTags::State_Dead, 1, EGameplayTagReplicationState::TagAndCountToAll);
}

void AASPlayerCharacter::FinishDeath()
{
	if (!HasAuthority() || !IsDead() || bDeathFinished)
	{
		return;
	}
	bDeathFinished = true;

	// 데스카운트와 리스폰은 게임모드 규칙이다. 테스트 게임모드처럼 다른 클래스면 리스폰은 없다.
	if (AASGameMode* GameMode = GetWorld()->GetAuthGameMode<AASGameMode>())
	{
		GameMode->OnPlayerDied(GetController());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Player] %s 사망. 게임모드가 AASGameMode 가 아니라 리스폰하지 않는다."), *GetName());
	}
}

void AASPlayerCharacter::ApplyLocalDeath()
{
	// 진행 중인 길찾기를 끊는다. 명령을 내린 머신의 컨트롤러가 들고 있다.
	if (AController* OwnController = GetController())
	{
		OwnController->StopMovement();
	}

	// 이동을 완전히 끈다. 중력도 같이 꺼지므로 충돌을 꺼도 바닥으로 빠지지 않는다.
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->StopMovementImmediately();
	MoveComp->DisableMovement();

	// 시체가 길을 막거나 몬스터 공격 판정에 걸리지 않게 한다.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	OnDeathStarted();
}

void AASPlayerCharacter::RestoreFromDeath()
{
	// ApplyLocalDeath 가 끈 것을 같은 순서로 되돌린다. 충돌을 먼저 켜야 걷기 모드가 바닥을 찾는다.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetDefaultMovementMode();
}
