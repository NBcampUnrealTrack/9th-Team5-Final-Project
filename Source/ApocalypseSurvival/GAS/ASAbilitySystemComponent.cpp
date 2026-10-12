#include "GAS/ASAbilitySystemComponent.h"

#include "Core/Input/ASInputTags.h"
#include "GAS/ASAbilityTags.h"
#include "Skills/ASSkillTags.h"

UASAbilitySystemComponent::UASAbilitySystemComponent()
{
	// 복제를 켜야 클라이언트가 어트리뷰트와 태그를 받는다.
	SetIsReplicatedByDefault(true);
}

void UASAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	// 죽으면 어떤 어빌리티도 입력으로 발동하지 않는다.
	// 어빌리티마다 차단 태그를 적지 않아도 되도록 입력 입구에서 한 번에 막는다.
	if (!InputTag.IsValid() || IsDead())
	{
		return;
	}

	// 스킬 일반 시전 조준 중(State.Targeting)에는 좌클릭을 기본 공격 대신 조준 확정으로 쓴다.
	// 확정 신호는 GAS 표준 LocalInputConfirm 으로 보내고, 조준 중인 스킬이 받아서 시전한다.
	if (InputTag.MatchesTagExact(ASGameplayTags::InputTag_Attack_Primary)
		&& HasMatchingGameplayTag(ASGameplayTags::State_Targeting))
	{
		LocalInputConfirm();
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

bool UASAbilitySystemComponent::IsDead() const
{
	return HasMatchingGameplayTag(ASGameplayTags::State_Dead);
}

bool UASAbilitySystemComponent::DoesSpecMatchInputTag(const FGameplayAbilitySpec& Spec, const FGameplayTag& InputTag)
{
	if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
	{
		return true;
	}

	return Spec.Ability != nullptr && Spec.Ability->GetAssetTags().HasTagExact(InputTag);
}
