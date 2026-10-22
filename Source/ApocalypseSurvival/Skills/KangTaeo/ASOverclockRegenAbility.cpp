#include "Skills/KangTaeo/ASOverclockRegenAbility.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Skills/ASSkillTags.h"

UASOverclockRegenAbility::UASOverclockRegenAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	//피해 판정과 스태미나 변경은 서버에서만
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	//적중 이벤트를 받으면 자동 발동
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = ASGameplayTags::Event_Hit_Dealt;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);

	//오버클록 중일 때만
	ActivationRequiredTags.AddTag(ASGameplayTags::State_Taeo_Overclock);

	RequiredAttackTag = ASGameplayTags::Attack_Basic;
}

void UASOverclockRegenAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	bool bGained = false;

	if (World && TriggerEventData && StaminaGainEffect)
	{
		//기본 공격만
		const bool bAttackMatches = !RequiredAttackTag.IsValid()
			|| TriggerEventData->InstigatorTags.HasTag(RequiredAttackTag);

		//0.5초에 1회
		const double Now = World->GetTimeSeconds();
		const bool bIntervalReady = Now - LastGainTime >= GainInterval;

		//5m 이내
		FVector HitLocation;
		const bool bInRange = GetHitLocation(*TriggerEventData, HitLocation)
			&& FVector::DistSquared2D(Avatar->GetActorLocation(), HitLocation) <= FMath::Square(MaxHitDistance);

		if (bAttackMatches && bIntervalReady && bInRange)
		{
			ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo,
				StaminaGainEffect->GetDefaultObject<UGameplayEffect>(), GetAbilityLevel(Handle, ActorInfo));
			LastGainTime = Now;
			bGained = true;
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bGained);
}

bool UASOverclockRegenAbility::GetHitLocation(const FGameplayEventData& EventData, FVector& OutLocation)
{
	if (const AActor* Target = EventData.Target.Get())
	{
		OutLocation = Target->GetActorLocation();
		return true;
	}

	//Mass 대상: TargetData의 위치
	for (int32 Index = 0; Index < EventData.TargetData.Num(); ++Index)
	{
		const FGameplayAbilityTargetData* Data = EventData.TargetData.Get(Index);
		if (!Data)
		{
			continue;
		}

		if (Data->HasHitResult())
		{
			OutLocation = Data->GetHitResult()->Location;
			return true;
		}

		if (Data->HasEndPoint())
		{
			OutLocation = Data->GetEndPoint();
			return true;
		}
	}

	return false;
}
