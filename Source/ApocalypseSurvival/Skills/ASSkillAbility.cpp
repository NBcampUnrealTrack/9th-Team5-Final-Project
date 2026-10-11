#include "ASSkillAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "GameplayEffect.h"
#include "Skills/ASSkillTags.h"
#include "Skills/Indicator/ASSkillRangeIndicatorActor.h"
#include "Engine/Texture2D.h"

UASSkillAbility::UASSkillAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	HitEventTag = ASGameplayTags::Event_Skill_Hit;
}

void UASSkillAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	//꾹 누르는 동안 범위 표시, 떼면 시전 (발동 가능 여부에서 비용·쿨타임은 이미 확인됨. 차감은 뗄 때)
	if (ShouldUseRangeIndicator())
	{
		BeginTargeting();
		return;
	}

	//서버에서 도는 원격 플레이어 스킬은 커서를 모르므로 클라이언트가 보낸 조준을 기다림
	if (ShouldWaitForClientAim())
	{
		WaitForClientAim();
		return;
	}

	//비용, 쿨다운
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	//조준은 시전 순간에 한 번 확정
	FVector AimDirection;
	FVector TargetLocation;
	ComputeAim(AimDirection, TargetLocation);

	//클라이언트는 같은 조준을 서버로 보냄 (호스트는 자기가 서버라 필요 없음)
	if (bAimAtCursor && !ActorInfo->IsNetAuthority())
	{
		SendAimToServer(AimDirection, TargetLocation);
	}

	StartSkill(AimDirection, TargetLocation);
}

void UASSkillAbility::StartSkill(const FVector& AimDirection, const FVector& TargetLocation)
{
	const bool bHasMontage = AnimMontage != nullptr;
	const bool bWaitForHitEvent = bHasMontage && HitEventTag.IsValid();

	bWaitingForHitEvent = bWaitForHitEvent;
	bHitEventReceived = false;

	CachedAimDirection = AimDirection;
	CachedTargetLocation = TargetLocation;
	bHasCachedAim = true;

	if (bFaceAimDirection)
	{
		if (AActor* Avatar = GetAvatarActorFromActorInfo())
		{
			Avatar->SetActorRotation(FRotator(0.0f, CachedAimDirection.Rotation().Yaw, 0.0f));
		}
	}

	//몽타주보다 먼저 대기 시작
	if (bWaitForHitEvent)
	{
		UAbilityTask_WaitGameplayEvent* WaitHitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, HitEventTag, nullptr, /*OnlyTriggerOnce*/ false, /*OnlyMatchExact*/ true);

		if (WaitHitTask)
		{
			WaitHitTask->EventReceived.AddDynamic(this, &UASSkillAbility::HandleHitEvent);
			WaitHitTask->ReadyForActivation();
		}
	}

	if (bHasMontage)
	{
		UAbilityTask_PlayMontageAndWait* PlayMontageTask =
			UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this, NAME_None, AnimMontage, MontagePlayRate, NAME_None, /*bStopWhenAbilityEnds*/ true);

		if (PlayMontageTask)
		{
			PlayMontageTask->OnCompleted.AddDynamic(this, &UASSkillAbility::OnMontageCompleted);
			PlayMontageTask->OnBlendOut.AddDynamic(this, &UASSkillAbility::OnMontageCompleted);
			PlayMontageTask->OnInterrupted.AddDynamic(this, &UASSkillAbility::OnMontageInterrupted);
			PlayMontageTask->OnCancelled.AddDynamic(this, &UASSkillAbility::OnMontageInterrupted);
			PlayMontageTask->ReadyForActivation();

			if (!bWaitForHitEvent)
			{
				OnSkillEffect();
			}

			return;
		}
	}

	//몽타주 없음: 즉시 발동 후 종료
	OnSkillEffect();

	if (IsActive())
	{
		FinishSkill(false);
	}
}

void UASSkillAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility,
	bool bWasCancelled)
{
	bHasCachedAim = false;
	bIsTargeting = false;
	DestroyRangeIndicator();

	//서버에서 조준을 기다리던 중이면 델리게이트 해제
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		const FPredictionKey PredictionKey = ActivationInfo.GetActivationPredictionKey();
		ASC->AbilityTargetDataSetDelegate(Handle, PredictionKey).RemoveAll(this);
		ASC->AbilityTargetDataCancelledDelegate(Handle, PredictionKey).RemoveAll(this);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASSkillAbility::OnSkillEffect_Implementation()
{
	//자식 클래스에서 구현
}

float UASSkillAbility::CalculateDamage_Implementation() const
{
	float AttackPower = 0.0f;

	if (AttackPowerAttribute.IsValid())
	{
		if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			bool bFound = false;
			AttackPower = ASC->GetGameplayAttributeValue(AttackPowerAttribute, bFound);
		}
	}

	return FMath::Max(0.0f, BaseDamage + AttackPower * AttackPowerScale);
}

FVector UASSkillAbility::GetAimDirection_Implementation() const
{
	return bHasCachedAim ? CachedAimDirection : ComputeAimDirection();
}

FVector UASSkillAbility::ComputeAimDirection() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return FVector::ForwardVector;
	}

	//커서는 본인 화면에서만 읽힘. 서버의 원격 플레이어는 ShouldWaitForClientAim으로 따로 처리
	FVector CursorLocation;
	if (bAimAtCursor && AASSkillRangeIndicatorActor::GetCursorWorldLocation(Cast<APawn>(Avatar), CursorLocation))
	{
		const FVector ToCursor = (CursorLocation - Avatar->GetActorLocation()).GetSafeNormal2D();
		if (!ToCursor.IsNearlyZero())
		{
			return ToCursor;
		}
	}

	return Avatar->GetActorForwardVector().GetSafeNormal2D();
}

void UASSkillAbility::ComputeAim(FVector& OutAimDirection, FVector& OutTargetLocation) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();

	OutAimDirection = ComputeAimDirection();
	OutTargetLocation = Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector;

	if (!Avatar || TargetingType != EASSkillTargetingType::Location)
	{
		return;
	}

	//커서가 없으면(AI 등) 조준 방향으로 사거리 끝
	FVector Desired;
	if (!bAimAtCursor || !AASSkillRangeIndicatorActor::GetCursorWorldLocation(Cast<APawn>(Avatar), Desired))
	{
		Desired = Avatar->GetActorLocation() + OutAimDirection * MaxCastRange;
	}

	OutTargetLocation = ResolveAimLocation(Desired);

	//위치형은 목표 지점을 바라보게
	const FVector ToTarget = (OutTargetLocation - Avatar->GetActorLocation()).GetSafeNormal2D();
	if (!ToTarget.IsNearlyZero())
	{
		OutAimDirection = ToTarget;
	}
}

FVector UASSkillAbility::ResolveAimLocation(const FVector& DesiredLocation) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return DesiredLocation;
	}

	if (TargetingType == EASSkillTargetingType::Location)
	{
		return AASSkillRangeIndicatorActor::ResolveTargetLocation(Avatar, DesiredLocation, MaxCastRange);
	}

	return Avatar->GetActorLocation();
}

FVector UASSkillAbility::GetTargetLocation() const
{
	if (bHasCachedAim)
	{
		return CachedTargetLocation;
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	return Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector;
}

FASSkillIndicatorSettings UASSkillAbility::GetIndicatorSettings_Implementation() const
{
	FASSkillIndicatorSettings Settings;
	Settings.TargetingType = TargetingType;
	Settings.MaxCastRange = MaxCastRange;
	Settings.RangeRingRadius = TargetingType == EASSkillTargetingType::Location ? MaxCastRange : 0.0f;
	return Settings;
}

bool UASSkillAbility::ShouldUseRangeIndicator() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	//AI는 PlayerController가 없으므로 제외
	return bShowRangeIndicator
		&& bAimAtCursor
		&& ActorInfo
		&& ActorInfo->PlayerController.IsValid()
		&& IsLocallyControlled();
}

void UASSkillAbility::BeginTargeting()
{
	bIsTargeting = true;

	APawn* Caster = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (UWorld* World = Caster ? Caster->GetWorld() : nullptr)
	{
		UClass* IndicatorClass = RangeIndicatorClass ? RangeIndicatorClass.Get() : AASSkillRangeIndicatorActor::StaticClass();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Caster;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		RangeIndicator = World->SpawnActor<AASSkillRangeIndicatorActor>(IndicatorClass, Caster->GetActorTransform(), SpawnParams);
		if (RangeIndicator)
		{
			RangeIndicator->InitIndicator(Caster, GetIndicatorSettings());
		}
	}

	//발동되는 사이에 이미 키를 뗐으면 바로 시전
	if (const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec(); Spec && !Spec->InputPressed)
	{
		ConfirmTargeting();
	}
}

void UASSkillAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	if (bIsTargeting)
	{
		ConfirmTargeting();
	}
}

void UASSkillAbility::ConfirmTargeting()
{
	if (!bIsTargeting)
	{
		return;
	}

	bIsTargeting = false;

	FVector AimDirection;
	FVector TargetLocation;
	ComputeAim(AimDirection, TargetLocation);

	DestroyRangeIndicator();

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	//발동 순간이 지났으므로 예측 창을 새로 열어야 클라이언트의 비용·쿨타임 차감이 서버와 맞음
	FScopedPredictionWindow ScopedPrediction(ASC, !HasSkillAuthority());

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (!HasSkillAuthority())
	{
		SendAimToServer(AimDirection, TargetLocation);
	}

	StartSkill(AimDirection, TargetLocation);
}

void UASSkillAbility::DestroyRangeIndicator()
{
	if (IsValid(RangeIndicator))
	{
		RangeIndicator->Destroy();
	}

	RangeIndicator = nullptr;
}

bool UASSkillAbility::ShouldWaitForClientAim() const
{
	return bAimAtCursor && HasSkillAuthority() && !IsLocallyControlled();
}

void UASSkillAbility::SendAimToServer(const FVector& AimDirection, const FVector& TargetLocation)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	//회전 = 조준 방향, 위치 = 목표 지점
	FGameplayAbilityTargetData_LocationInfo* AimData = new FGameplayAbilityTargetData_LocationInfo();
	AimData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	AimData->TargetLocation.LiteralTransform = FTransform(AimDirection.Rotation(), TargetLocation);

	FGameplayAbilityTargetDataHandle TargetData;
	TargetData.Add(AimData);

	ASC->CallServerSetReplicatedTargetData(
		CurrentSpecHandle,
		CurrentActivationInfo.GetActivationPredictionKey(),
		TargetData,
		FGameplayTag(),
		ASC->ScopedPredictionKey);
}

void UASSkillAbility::WaitForClientAim()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const FPredictionKey PredictionKey = CurrentActivationInfo.GetActivationPredictionKey();
	ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, PredictionKey).AddUObject(this, &UASSkillAbility::OnClientAimReceived);
	ASC->AbilityTargetDataCancelledDelegate(CurrentSpecHandle, PredictionKey).AddUObject(this, &UASSkillAbility::OnClientAimCancelled);

	//데이터가 발동보다 먼저 도착했으면 바로 처리
	ASC->CallReplicatedTargetDataDelegatesIfSet(CurrentSpecHandle, PredictionKey);
}

void UASSkillAbility::OnClientAimReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	//이미 시작했으면 무시 (중복 수신 방지)
	if (bHasCachedAim)
	{
		return;
	}

	//받은 데이터는 바로 비움. 남겨 두면 다음 발동에 섞임
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();

	FVector AimDirection = FVector::ZeroVector;
	FVector TargetLocation = Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector;

	const FGameplayAbilityTargetData* AimData = Data.Get(0);
	if (AimData && AimData->HasEndPoint())
	{
		const FTransform AimTransform = AimData->GetEndPointTransform();
		AimDirection = AimTransform.GetRotation().Vector().GetSafeNormal2D();
		TargetLocation = AimTransform.GetLocation();
	}

	if (AimDirection.IsNearlyZero())
	{
		AimDirection = ComputeAimDirection();
	}

	//서버에서 한 번 더 사거리 제한 (잘못된 값 방지)
	TargetLocation = ResolveAimLocation(TargetLocation);

	//예측 창 안이므로 클라이언트의 예측 차감과 맞춰짐
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	StartSkill(AimDirection, TargetLocation);
}

void UASSkillAbility::OnClientAimCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

ACharacter* UASSkillAbility::GetOwnerCharacter() const
{
	//Owner는 PlayerState일 수 있음
	return Cast<ACharacter>(GetAvatarActorFromActorInfo());
}

bool UASSkillAbility::HasSkillAuthority() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	return ActorInfo && ActorInfo->IsNetAuthority();
}

bool UASSkillAbility::IsValidSkillTarget(const AActor* Target, const AActor* Instigator, bool bCanHitPlayers)
{
	if (!IsValid(Target) || Target == Instigator)
	{
		return false;
	}

	if (!bCanHitPlayers)
	{
		if (const APawn* Pawn = Cast<APawn>(Target); Pawn && Pawn->IsPlayerControlled())
		{
			return false;
		}
	}

	// 투사체와 범위 공격 모두 이 함수를 거치므로, 여기서 같은 팀/중립 팀을 한 번에 제외한다.
	// 공격자는 적대 팀에만 피해를 준다. 따라서 몬스터 투사체는 플레이어만, 플레이어 공격은
	// 몬스터만 유효 타깃으로 판단한다.
	if (const IGenericTeamAgentInterface* InstigatorTeam = Cast<IGenericTeamAgentInterface>(Instigator))
	{
		if (InstigatorTeam->GetTeamAttitudeTowards(*Target) != ETeamAttitude::Hostile)
		{
			return false;
		}
	}

	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Target)) != nullptr;
}

TArray<AActor*> UASSkillAbility::FindSkillTargets(const UWorld* World, const FVector& Center, float Radius, const AActor* Instigator, bool bCanHitPlayers)
{
	TArray<AActor*> Targets;

	if (!World || Radius <= 0.0f)
	{
		return Targets;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ASSkillFindTargets), false, Instigator);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Center,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius),
		Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (IsValidSkillTarget(Actor, Instigator, bCanHitPlayers))
		{
			Targets.AddUnique(Actor);
		}
	}

	return Targets;
}

void UASSkillAbility::ApplyDamageSpecToTarget(const FGameplayEffectSpecHandle& DamageSpec, AActor* Target)
{
	if (!DamageSpec.IsValid() || !Target)
	{
		return;
	}

	if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
	{
		TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpec.Data.Get());
	}
}

TArray<AActor*> UASSkillAbility::FindTargetsInRadius(const FVector& Center, float Radius) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;

	TArray<AActor*> Targets = FindSkillTargets(World, Center, Radius, Avatar, bCanHitPlayers);

	if (bDrawDebug && World)
	{
		DrawDebugSphere(World, Center, Radius, 16, Targets.Num() > 0 ? FColor::Red : FColor::Green, false, 1.0f);
	}

	return Targets;
}

FGameplayEffectSpecHandle UASSkillAbility::MakeDamageSpec() const
{
	if (!DamageEffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
	if (SpecHandle.IsValid())
	{
		//Health에 더하므로 음수
		SpecHandle.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, -CalculateDamage());
	}

	return SpecHandle;
}

void UASSkillAbility::ApplySkillDamage(AActor* Target)
{
	if (!HasSkillAuthority())
	{
		return;
	}

	ApplyDamageSpecToTarget(MakeDamageSpec(), Target);
}

void UASSkillAbility::HandleHitEvent(FGameplayEventData Payload)
{
	bHitEventReceived = true;
	OnSkillEffect();
}

void UASSkillAbility::OnMontageCompleted()
{
	FinishSkill(false);
}

void UASSkillAbility::OnMontageInterrupted()
{
	FinishSkill(true);
}

void UASSkillAbility::FinishSkill(bool bWasCancelled)
{
#if !UE_BUILD_SHIPPING
	if (bWaitingForHitEvent && !bHitEventReceived && !bWasCancelled)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Skill] %s: %s에 '%s' AnimNotify가 없습니다."),
			*GetName(), *GetNameSafe(AnimMontage), *HitEventTag.ToString());
	}
#endif

	bWaitingForHitEvent = false;

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}

UTexture2D* UASSkillAbility::GetSkillIcon() const
{
	return SkillIcon;
}