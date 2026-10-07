#include "GAS/ASAbilitySystemComponent.h"

UASAbilitySystemComponent::UASAbilitySystemComponent()
{
	// 복제를 켜야 클라이언트가 어트리뷰트와 태그를 받는다.
	SetIsReplicatedByDefault(true);
}

void UASAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	// 발동은 핸들로 한다. TryActivateAbility 가 목록을 바꿀 수 있어서
	// 순회 중에 Spec 참조를 계속 들고 있으면 안 된다.
	TArray<FGameplayAbilitySpecHandle> HandlesToActivate;

	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!DoesSpecMatchInputTag(Spec, InputTag))
		{
			continue;
		}

		Spec.InputPressed = true;

		if (Spec.IsActive())
		{
			// 이미 실행 중이면 다시 발동하지 않고 "눌림"만 알린다.
			AbilitySpecInputPressed(Spec);
		}
		else
		{
			HandlesToActivate.Add(Spec.Handle);
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : HandlesToActivate)
	{
		TryActivateAbility(Handle);
	}
}

void UASAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!DoesSpecMatchInputTag(Spec, InputTag))
		{
			continue;
		}

		Spec.InputPressed = false;

		if (Spec.IsActive())
		{
			AbilitySpecInputReleased(Spec);
		}
	}
}

bool UASAbilitySystemComponent::DoesSpecMatchInputTag(const FGameplayAbilitySpec& Spec, const FGameplayTag& InputTag)
{
	if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
	{
		return true;
	}

	return Spec.Ability != nullptr && Spec.Ability->GetAssetTags().HasTagExact(InputTag);
}
