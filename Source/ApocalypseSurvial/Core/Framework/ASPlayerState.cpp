#include "Core/Framework/ASPlayerState.h"

#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Attributes/ASAttributeSet.h"

AASPlayerState::AASPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UASAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// Mixed: the owning client receives full effect data for prediction and UI,
	// other clients only receive tags, cues and attribute values.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UASAttributeSet>(TEXT("AttributeSet"));

	// PlayerState replicates slowly by default, which delays attribute updates.
	SetNetUpdateFrequency(100.0f);
}

UAbilitySystemComponent* AASPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
