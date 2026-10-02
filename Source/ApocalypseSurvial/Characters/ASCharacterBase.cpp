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
	UnbindAttributeDelegates();

	Super::EndPlay(EndPlayReason);
}

void AASCharacterBase::InitAbilitySystem(UASAbilitySystemComponent* InAbilitySystemComponent, UASAttributeSet* InAttributeSet, AActor* InOwnerActor)
{
	if (!IsValid(InAbilitySystemComponent) || !IsValid(InAttributeSet))
	{
		return;
	}

	UnbindAttributeDelegates();

	AbilitySystemComponent = InAbilitySystemComponent;
	AttributeSet = InAttributeSet;
	AbilitySystemComponent->InitAbilityActorInfo(InOwnerActor, this);

	// MoveSpeed replicates to every connection, so each machine keeps its own
	// movement component in sync without a separate replicated property.
	MoveSpeedChangedHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UASAttributeSet::GetMoveSpeedAttribute())
		.AddUObject(this, &AASCharacterBase::HandleMoveSpeedChanged);
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
