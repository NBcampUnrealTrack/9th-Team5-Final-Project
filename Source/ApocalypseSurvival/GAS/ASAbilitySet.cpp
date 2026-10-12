#include "GAS/ASAbilitySet.h"

#include "GAS/ASAbilitySystemComponent.h"
#include "GameplayEffect.h"

void FASAbilitySet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FASAbilitySet_GrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

void FASAbilitySet_GrantedHandles::TakeFromAbilitySystem(UASAbilitySystemComponent* AbilitySystemComponent)
{
	if (!IsValid(AbilitySystemComponent) || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->ClearAbility(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
		}
	}

	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
}

void UASAbilitySet::GiveToAbilitySystem(UASAbilitySystemComponent* AbilitySystemComponent, FASAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const
{
	// 어빌리티 부여는 서버만 한다. 클라이언트에는 복제로 전달된다.
	if (!IsValid(AbilitySystemComponent) || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const FASAbilitySet_GameplayAbility& Entry : GrantedGameplayAbilities)
	{
		if (!Entry.Ability)
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(Entry.Ability, Entry.AbilityLevel, INDEX_NONE, SourceObject);

		// 입력 태그는 어빌리티 클래스가 아니라 이 스펙에 붙인다.
		// ASC 는 입력이 오면 스펙의 이 태그를 보고 발동할 어빌리티를 고른다.
		if (Entry.InputTag.IsValid())
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
		}

		const FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GiveAbility(AbilitySpec);
		if (OutGrantedHandles != nullptr)
		{
			OutGrantedHandles->AddAbilitySpecHandle(Handle);
		}
	}

	for (const FASAbilitySet_GameplayEffect& Entry : GrantedGameplayEffects)
	{
		if (!Entry.GameplayEffect)
		{
			continue;
		}

		const UGameplayEffect* GameplayEffect = Entry.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle EffectHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(
			GameplayEffect,
			Entry.EffectLevel,
			AbilitySystemComponent->MakeEffectContext());
		if (OutGrantedHandles != nullptr)
		{
			OutGrantedHandles->AddGameplayEffectHandle(EffectHandle);
		}
	}
}
