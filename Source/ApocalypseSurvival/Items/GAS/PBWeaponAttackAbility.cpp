#include "Items/GAS/PBWeaponAttackAbility.h"

#include "AbilitySystemComponent.h"
#include "Characters/Player/ASPlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/GAS/ASItemAbilityLibrary.h"
#include "Skills/ASSkillAbility.h"
#include "Skills/Indicator/ASSkillRangeIndicatorActor.h"

UPBWeaponAttackAbility::UPBWeaponAttackAbility()
{
	// 스킬과 같은 방식으로 클라이언트가 조준을 계산하고 서버에 TargetData로 보낸다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UPBWeaponAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	bAttackStarted = false;

	if (!ActorInfo)
	{
		return;
	}

	// 서버의 원격 플레이어 컨트롤러에는 클라이언트의 마우스가 없으므로 기다린다.
	if (ActorInfo->IsNetAuthority() && !IsLocallyControlled())
	{
		WaitForClientAim();
		return;
	}

	FVector AimDirection;
	FVector TargetLocation;
	ComputeLocalAim(AimDirection, TargetLocation);

	if (!ActorInfo->IsNetAuthority())
	{
		SendAimToServer(AimDirection, TargetLocation);
		return;
	}

	// 리슨 서버 호스트는 조준과 공격을 서버에서 바로 처리한다.
	ExecuteAttack(AimDirection);
}

void UPBWeaponAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicatedEndAbility,
	const bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		const FPredictionKey PredictionKey = ActivationInfo.GetActivationPredictionKey();
		ASC->AbilityTargetDataSetDelegate(Handle, PredictionKey).RemoveAll(this);
		ASC->AbilityTargetDataCancelledDelegate(Handle, PredictionKey).RemoveAll(this);
	}

	bAttackStarted = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UPBWeaponAttackAbility::ComputeLocalAim(
	FVector& OutAimDirection,
	FVector& OutTargetLocation) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const FVector Origin = Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector;
	OutAimDirection = Avatar
		? Avatar->GetActorForwardVector().GetSafeNormal2D()
		: FVector::ForwardVector;
	OutTargetLocation = Origin;

	FVector CursorLocation;
	if (Avatar
		&& AASSkillRangeIndicatorActor::GetCursorWorldLocation(Cast<APawn>(Avatar), CursorLocation))
	{
		const FVector ToCursor = (CursorLocation - Origin).GetSafeNormal2D();
		if (!ToCursor.IsNearlyZero())
		{
			OutAimDirection = ToCursor;
		}

		OutTargetLocation = CursorLocation;
	}
}

void UPBWeaponAttackAbility::SendAimToServer(
	const FVector& AimDirection,
	const FVector& TargetLocation)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	// 스킬과 동일하게 회전에 방향, 위치에 커서 월드 좌표를 담는다.
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

void UPBWeaponAttackAbility::WaitForClientAim()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const FPredictionKey PredictionKey = CurrentActivationInfo.GetActivationPredictionKey();
	ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, PredictionKey).AddUObject(
		this, &UPBWeaponAttackAbility::OnClientAimReceived);
	ASC->AbilityTargetDataCancelledDelegate(CurrentSpecHandle, PredictionKey).AddUObject(
		this, &UPBWeaponAttackAbility::OnClientAimCancelled);
	ASC->CallReplicatedTargetDataDelegatesIfSet(CurrentSpecHandle, PredictionKey);
}

void UPBWeaponAttackAbility::OnClientAimReceived(
	const FGameplayAbilityTargetDataHandle& Data,
	const FGameplayTag ApplicationTag)
{
	if (bAttackStarted)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->ConsumeClientReplicatedTargetData(
			CurrentSpecHandle,
			CurrentActivationInfo.GetActivationPredictionKey());
	}

	FVector AimDirection;
	FVector TargetLocation;
	ComputeLocalAim(AimDirection, TargetLocation);

	const FGameplayAbilityTargetData* AimData = Data.Get(0);
	if (AimData && AimData->HasEndPoint())
	{
		const FTransform AimTransform = AimData->GetEndPointTransform();
		AimDirection = AimTransform.GetRotation().Vector().GetSafeNormal2D();
	}

	ExecuteAttack(AimDirection);
}

void UPBWeaponAttackAbility::OnClientAimCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UPBWeaponAttackAbility::ExecuteAttack(const FVector& InAimDirection)
{
	if (bAttackStarted || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	APawn* Avatar = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
	UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
	const UASItemDefinition* Definition = UASItemAbilityLibrary::GetWeaponDefinitionFromSourceObject(
		GetCurrentSourceObject());
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;

	if (!Avatar || !SourceASC || !Definition || !World
		|| (Definition->WeaponType != EASWeaponType::Melee
			&& Definition->WeaponType != EASWeaponType::Ranged))
	{
			UE_LOG(LogTemp, Warning, TEXT("[WeaponAttack] Attack source is invalid."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[WeaponAttack] CommitAbility failed for %s."),
			*GetNameSafe(Definition));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	bAttackStarted = true;
	const FVector NormalizedAimDirection = InAimDirection.GetSafeNormal2D();
	const FVector AimDirection = NormalizedAimDirection.IsNearlyZero()
		? Avatar->GetActorForwardVector().GetSafeNormal2D()
		: NormalizedAimDirection;
	const float AttackRange = FMath::Max(1.0f, Definition->AttackRange);
	const float TraceRange = Definition->WeaponType == EASWeaponType::Ranged
		? AttackRange * 10.0f
		: AttackRange;
	const FVector Start = Avatar->GetActorLocation();
	TArray<AActor*> Targets;

	if (Definition->WeaponType == EASWeaponType::Melee)
	{
		// 구의 크기는 유지하고, 구가 생성되는 중심 위치만 기존보다 2배 멀리 둔다.
		const float Radius = FMath::Max(50.0f, AttackRange * 0.5f);
		const float HitboxOffset = Radius * 2.0f;
		const FVector Center = Start + AimDirection * HitboxOffset;
		Targets = UASSkillAbility::FindSkillTargets(World, Center, Radius, Avatar, false);

		if (AASPlayerCharacter* DebugPlayerCharacter = Cast<AASPlayerCharacter>(Avatar))
		{
				DebugPlayerCharacter->MulticastDrawWeaponAttackDebug(
				Start,
				Center,
				Radius,
				true,
				Targets.Num() > 0);
		}
	}
	else
	{
		// 프로토타입 원거리 공격은 50cm 폭의 비관통 판정으로 처리한다.
		const float TraceRadius = 50.0f;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponAttack), false, Avatar);
		TArray<FHitResult> Hits;
		const FVector End = Start + AimDirection * TraceRange;

		World->SweepMultiByObjectType(
			Hits,
			Start,
			End,
			FQuat::Identity,
			FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeSphere(TraceRadius),
			QueryParams);

		AActor* FirstTarget = nullptr;
		float FirstHitTime = 1.0f;
		for (const FHitResult& Hit : Hits)
		{
			AActor* Target = Hit.GetActor();
			if (UASSkillAbility::IsValidSkillTarget(Target, Avatar, false)
				&& (!FirstTarget || Hit.Time < FirstHitTime))
			{
				FirstTarget = Target;
				FirstHitTime = Hit.Time;
			}
		}

		if (FirstTarget)
		{
			Targets.Add(FirstTarget);
		}

		if (AASPlayerCharacter* DebugPlayerCharacter = Cast<AASPlayerCharacter>(Avatar))
		{
			const FVector DebugEnd = FirstTarget
				? Start + (End - Start) * FirstHitTime
				: End;
			DebugPlayerCharacter->MulticastDrawWeaponAttackDebug(
				Start,
				DebugEnd,
				TraceRadius,
				false,
				FirstTarget != nullptr);
		}
	}

	const TArray<FGameplayEffectSpecHandle> DamageSpecs =
		UASItemAbilityLibrary::MakeWeaponHitEffectSpecs(GetCurrentSourceObject(), SourceASC);

	for (AActor* Target : Targets)
	{
		for (const FGameplayEffectSpecHandle& DamageSpec : DamageSpecs)
		{
			UASSkillAbility::ApplyDamageSpecToTarget(DamageSpec, Target);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WeaponAttack] Weapon=%s Targets=%d DamageSpecs=%d"),
		*GetNameSafe(Definition), Targets.Num(), DamageSpecs.Num());

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
