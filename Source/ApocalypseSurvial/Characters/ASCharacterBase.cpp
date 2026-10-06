#include "Characters/ASCharacterBase.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Attributes/ASAttributeSet.h"

AASCharacterBase::AASCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

UAbilitySystemComponent* AASCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AASCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 플레이어의 ASC 는 PlayerState 에 있어서 이 캐릭터보다 오래 산다.
	// 구독을 풀지 않으면 파괴된 캐릭터를 가리키는 구독이 ASC 에 남는다.
	UnbindAttributeDelegates();

	Super::EndPlay(EndPlayReason);
}

void AASCharacterBase::InitAbilitySystem(UASAbilitySystemComponent* InAbilitySystemComponent, UASAttributeSet* InAttributeSet, AActor* InOwnerActor)
{
	if (!IsValid(InAbilitySystemComponent) || !IsValid(InAttributeSet))
	{
		return;
	}

	// 다시 불렸을 때 구독이 두 번 걸리지 않도록 먼저 푼다.
	UnbindAttributeDelegates();

	AbilitySystemComponent = InAbilitySystemComponent;
	AttributeSet = InAttributeSet;
	AbilitySystemComponent->InitAbilityActorInfo(InOwnerActor, this);

	// MoveSpeed 는 모든 접속자에게 복제된다. 그래서 각 머신이 변경 알림을 받아
	// 자기 이동 컴포넌트에 옮기기만 하면 되고, 속도를 따로 복제할 필요가 없다.
	MoveSpeedChangedHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UASAttributeSet::GetMoveSpeedAttribute())
		.AddUObject(this, &AASCharacterBase::HandleMoveSpeedChanged);

	// 알림은 "바뀔 때"만 온다. 연결한 시점의 값은 직접 한 번 넣어 줘야 한다.
	GetCharacterMovement()->MaxWalkSpeed = AttributeSet->GetMoveSpeed();

	OnAbilitySystemInitialized();
}

void AASCharacterBase::OnAbilitySystemInitialized()
{
}

void AASCharacterBase::UnbindAttributeDelegates()
{
	if (IsValid(AbilitySystemComponent) && MoveSpeedChangedHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UASAttributeSet::GetMoveSpeedAttribute())
			.Remove(MoveSpeedChangedHandle);
	}

	MoveSpeedChangedHandle.Reset();
}

void AASCharacterBase::HandleMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	GetCharacterMovement()->MaxWalkSpeed = Data.NewValue;
}
