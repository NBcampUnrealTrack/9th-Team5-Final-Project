#include "Skills/KangTaeo/ASDeadAngleAbility.h"

#include "AbilitySystemComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Skills/Data/ASSkillData.h"
#include "TimerManager.h"

UASDeadAngleAbility::UASDeadAngleAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	//적 수 판정과 GE 적용은 서버에서만 (태그는 복제됨)
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UASDeadAngleAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	//패시브 - 서버에서 아직 안 켜져 있으면 켜기
	if (ActorInfo && ActorInfo->IsNetAuthority() && ActorInfo->AbilitySystemComponent.IsValid() && !Spec.IsActive())
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
	}
}

void UASDeadAngleAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	//부모(UASSkillAbility)의 조준·시전 흐름은 건너뛰고 기본 GA 처리만
	UGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World || !ReductionEffect)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	//EndAbility 안 부름 - 계속 켜 두고 주기적으로 검사
	World->GetTimerManager().SetTimer(CheckTimerHandle, this, &UASDeadAngleAbility::UpdateDeadAngle, CheckInterval, true, 0.0f);
}

void UASDeadAngleAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		Avatar->GetWorldTimerManager().ClearTimer(CheckTimerHandle);
	}
	RemoveReductionEffect();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UASDeadAngleAbility::UpdateDeadAngle()
{
	const float SearchRadius = GetSearchRadius();
	const int32 EnemyCount = CountNearbyEnemies(SearchRadius);
	const bool bShouldBeActive = EnemyCount >= RequiredEnemyCount;
	const bool bHasReduction = ReductionEffectHandle.IsValid();

	if (bShouldBeActive && !bHasReduction)
	{
		//N명 이상 - 피해 감소 켜기
		ReductionEffectHandle = ApplyGameplayEffectToOwner(
			GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
			ReductionEffect->GetDefaultObject<UGameplayEffect>(), GetAbilityLevel());
	}
	else if (!bShouldBeActive && bHasReduction)
	{
		//N명 미만 - 끄기
		RemoveReductionEffect();
	}

	if (ShouldDrawDebug())
	{
		if (const AActor* Avatar = GetAvatarActorFromActorInfo())
		{
			const FColor Color = bShouldBeActive ? FColor::Green : FColor::White;
			DrawDebugCircle(Avatar->GetWorld(), Avatar->GetActorLocation(), SearchRadius, 32, Color, false, CheckInterval,
				0, 2.0f, FVector::ForwardVector, FVector::RightVector, false);
			DrawDebugString(Avatar->GetWorld(), Avatar->GetActorLocation() + FVector(0, 0, 120),
				FString::Printf(TEXT("DeadAngle %d/%d"), EnemyCount, RequiredEnemyCount), nullptr, Color, CheckInterval);
		}
	}
}

int32 UASDeadAngleAbility::CountNearbyEnemies(const float SearchRadius) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	const UASCombatTargetSubsystem* CombatTargets = World ? World->GetSubsystem<UASCombatTargetSubsystem>() : nullptr;
	if (!CombatTargets)
	{
		return 0;
	}

	FASCombatTargetQuery Query;
	Query.SearchOrigin = Avatar->GetActorLocation();
	Query.SearchRadius = SearchRadius;
	Query.Instigator = Avatar;
	Query.Shape = EASCombatTargetShape::Radius;
	//1차: Actor만 (Mass는 셀 함수가 아직 없음)
	Query.Domain = EASCombatTargetDomain::ActorOnly;

	return CombatTargets->FindActorTargets(Query).Num();
}

float UASDeadAngleAbility::GetSearchRadius() const
{
	//부모의 SkillData → DT 행
	if (const FASSkillStatsRow* Stats = FindSkillStats())
	{
		return Stats->AreaRadius;
	}
	return Radius;
}

void UASDeadAngleAbility::RemoveReductionEffect()
{
	if (!ReductionEffectHandle.IsValid())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveActiveGameplayEffect(ReductionEffectHandle);
	}
	ReductionEffectHandle.Invalidate();
}
