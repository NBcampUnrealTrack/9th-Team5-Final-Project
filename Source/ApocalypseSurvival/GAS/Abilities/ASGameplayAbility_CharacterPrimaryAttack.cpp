#include "GAS/Abilities/ASGameplayAbility_CharacterPrimaryAttack.h"
#include "GAS/Abilities/ASGameplayAbilityTargetData.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Characters/Player/Components/ASEquipmentComponent.h"
#include "Characters/Player/Components/ASPlayerAttackMoveComponent.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Core/Framework/ASPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ASAbilityTags.h"
#include "Items/ASItemInstance.h"
#include "Items/Combat/ASWeaponAttackProfile.h"
#include "Items/Data/ASItemDefinition.h"
#include "Items/Inventory/ASInventoryComponent.h"
#include "MapSystem/Vision/ASVisibilityComponent.h"
#include "MapSystem/Vision/ASVisionSourceComponent.h"
#include "Skills/ASSkillTags.h"
#include "Skills/Indicator/ASSkillRangeIndicatorActor.h"

UASGameplayAbility_CharacterPrimaryAttack::UASGameplayAbility_CharacterPrimaryAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Dodging);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Casting);
}

float UASGameplayAbility_CharacterPrimaryAttack::GetCurrentAttackRange() const
{
	FASWeaponAttackExecutionContext Context;
	const UASWeaponAttackExecution* Execution = nullptr;
	return BuildExecutionContext(Context, Execution) && Context.Definition
		? FMath::Max(0.0f, Context.Definition->AttackRange)
		: 0.0f;
}

void UASGameplayAbility_CharacterPrimaryAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	bAttackStarted = false;
	bHitPerformed = false;
	bIsTargeting = false;
	bAddedTargetingTag = false;
	bBoundLocalConfirm = false;
	bBoundLocalCancel = false;

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || ASC->HasMatchingGameplayTag(ASGameplayTags::State_Dead))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsNetAuthority() && !IsLocallyControlled())
	{
		WaitForClientAim();
		return;
	}

	// A + 클릭 명령이 준비한 요청은 일반 조준 상태를 거치지 않고 즉시 한 번 공격한다.
	if (AASPlayerController* PlayerController = Cast<AASPlayerController>(ActorInfo->PlayerController.Get()))
	{
		AActor* RequestedTarget = nullptr;
		FVector RequestedTargetLocation = FVector::ZeroVector;
		uint32 RequestedMassTargetNetworkId = 0;
		if (PlayerController->ConsumeAttackMoveRequest(RequestedTarget, RequestedTargetLocation,
			RequestedMassTargetNetworkId))
		{
			FVector AimDirection = RequestedTarget
				? (RequestedTarget->GetActorLocation() - GetAvatarActorFromActorInfo()->GetActorLocation()).GetSafeNormal2D()
				: (RequestedTargetLocation - GetAvatarActorFromActorInfo()->GetActorLocation()).GetSafeNormal2D();
			if (AimDirection.IsNearlyZero())
			{
				AimDirection = GetAvatarActorFromActorInfo()->GetActorForwardVector().GetSafeNormal2D();
			}

			if (!ActorInfo->IsNetAuthority())
			{
				SendAimToServer(AimDirection, RequestedTargetLocation, RequestedTarget,
					RequestedMassTargetNetworkId);
			}
			StartAttack(AimDirection, RequestedTargetLocation, RequestedTarget,
				RequestedMassTargetNetworkId);
			return;
		}
	}

	BeginTargeting();
}

void UASGameplayAbility_CharacterPrimaryAttack::InputPressed(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	// 조준 중 A를 다시 누르면 확정하지 않고 취소한다.
	if (bIsTargeting)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UASGameplayAbility_CharacterPrimaryAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicatedEndAbility, const bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		const FPredictionKey PredictionKey = ActivationInfo.GetActivationPredictionKey();
		ASC->AbilityTargetDataSetDelegate(Handle, PredictionKey).RemoveAll(this);
		ASC->AbilityTargetDataCancelledDelegate(Handle, PredictionKey).RemoveAll(this);
	}

	bIsTargeting = false;
	StopTargetingInput(bWasCancelled);
	bAttackStarted = false;
	bHitPerformed = false;
	CachedExecutionContext = FASWeaponAttackExecutionContext();
	CachedExecution = nullptr;
	CachedRequestedTarget.Reset();
	ActiveMontage = nullptr;
	ActiveHitEventTag = FGameplayTag();
	if (AASPlayerController* PlayerController = Cast<AASPlayerController>(ActorInfo ? ActorInfo->PlayerController.Get() : nullptr))
	{
		PlayerController->GetAttackMoveComponent()->NotifyAttackFinished();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

void UASGameplayAbility_CharacterPrimaryAttack::BeginTargeting()
{
	// 일반 공격은 별도 쿨타임 GE 대신 이 인스턴스의 공격 간격을 사용한다.
	// A 입력이 쿨타임 중 조준 상태에 들어가면, 나중에 좌클릭해도 공격이 실패하고
	// 준비 상태만 남을 수 있으므로 시작 단계에서 무시한다.
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (World && World->GetTimeSeconds() < NextAttackTime)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	FASWeaponAttackExecutionContext Context;
	const UASWeaponAttackExecution* Execution = nullptr;
	if (!BuildExecutionContext(Context, Execution))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PrimaryAttack] %s 조준 시작 실패: 장착 무기/프로필 없음 (Authority=%d)"),
			*GetNameSafe(GetAvatarActorFromActorInfo()), CurrentActorInfo->IsNetAuthority());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	if (!Execution->CanPayCost(Context))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PrimaryAttack] %s 조준 시작 실패: 비용 부족 (스태미나/탄약, Authority=%d)"),
			*GetNameSafe(GetAvatarActorFromActorInfo()), CurrentActorInfo->IsNetAuthority());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	bIsTargeting = true;
	// 로컬 전용 태그다. 좌클릭이 Primary Attack 재발동 대신 조준 확정으로 처리된다.
	ASC->AddLooseGameplayTag(ASGameplayTags::State_Targeting);
	bAddedTargetingTag = true;
	ASC->GenericLocalConfirmCallbacks.AddDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnLocalInputConfirm);
	bBoundLocalConfirm = true;
	ASC->GenericLocalCancelCallbacks.AddDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnLocalInputCancel);
	bBoundLocalCancel = true;
	OnAttackTargetingStarted();
}

void UASGameplayAbility_CharacterPrimaryAttack::ConfirmTargeting()
{
	if (!bIsTargeting) return;

	bIsTargeting = false;
	StopTargetingInput(false);

	FVector AimDirection;
	FVector TargetLocation;
	ComputeLocalAim(AimDirection, TargetLocation);
	AActor* RequestedTarget = nullptr;
	uint32 RequestedMassTargetNetworkId = 0;
	if (AASPlayerController* PlayerController = Cast<AASPlayerController>(CurrentActorInfo->PlayerController.Get()))
	{
		if (PlayerController->ConsumeAttackMoveRequest(RequestedTarget, TargetLocation,
			RequestedMassTargetNetworkId))
		{
			TargetLocation = RequestedTarget ? RequestedTarget->GetActorLocation() : TargetLocation;
			AimDirection = (TargetLocation - GetAvatarActorFromActorInfo()->GetActorLocation()).GetSafeNormal2D();
			if (AimDirection.IsNearlyZero())
			{
				AimDirection = GetAvatarActorFromActorInfo()->GetActorForwardVector().GetSafeNormal2D();
			}
		}
	}
	if (!CurrentActorInfo->IsNetAuthority())
	{
		SendAimToServer(AimDirection, TargetLocation, RequestedTarget, RequestedMassTargetNetworkId);
	}
	StartAttack(AimDirection, TargetLocation, RequestedTarget, RequestedMassTargetNetworkId);
}

void UASGameplayAbility_CharacterPrimaryAttack::StopTargetingInput(const bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (bAddedTargetingTag)
		{
			ASC->RemoveLooseGameplayTag(ASGameplayTags::State_Targeting);
		}
		if (bBoundLocalConfirm)
		{
			ASC->GenericLocalConfirmCallbacks.RemoveDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnLocalInputConfirm);
		}
		if (bBoundLocalCancel)
		{
			ASC->GenericLocalCancelCallbacks.RemoveDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnLocalInputCancel);
		}
	}

	if (bAddedTargetingTag || bBoundLocalConfirm || bBoundLocalCancel)
	{
		OnAttackTargetingEnded(bWasCancelled);
	}
	bAddedTargetingTag = false;
	bBoundLocalConfirm = false;
	bBoundLocalCancel = false;
}

void UASGameplayAbility_CharacterPrimaryAttack::OnLocalInputConfirm()
{
	// LocalInputConfirm이 호출되면 ASC가 구독을 비우므로 상태도 함께 정리한다.
	bBoundLocalConfirm = false;
	ConfirmTargeting();
}

void UASGameplayAbility_CharacterPrimaryAttack::OnLocalInputCancel()
{
	if (bIsTargeting)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

bool UASGameplayAbility_CharacterPrimaryAttack::BuildExecutionContext(
	FASWeaponAttackExecutionContext& OutContext, const UASWeaponAttackExecution*& OutExecution) const
{
	OutContext = FASWeaponAttackExecutionContext();
	OutExecution = nullptr;
	APawn* Avatar = Cast<APawn>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UASEquipmentComponent* Equipment = Avatar ? Avatar->FindComponentByClass<UASEquipmentComponent>() : nullptr;
	UASItemInstance* Weapon = Equipment ? Equipment->GetEquippedItem() : nullptr;
	const UASItemDefinition* Definition = Weapon ? Weapon->GetItemDefinition() : nullptr;
	UASInventoryComponent* Inventory = Weapon ? Weapon->GetTypedOuter<UASInventoryComponent>() : nullptr;
	if (!Avatar || !ASC || !Weapon || !Definition || !Inventory || Definition->ItemType != EASItemType::Weapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PrimaryAttack] 컨텍스트 실패 (Authority=%d): Avatar=%d ASC=%d Equipment=%d Weapon=%d Definition=%d Inventory=%d IsWeapon=%d WeaponOuter=%s"),
			Avatar ? Avatar->HasAuthority() : -1,
			Avatar != nullptr, ASC != nullptr, Equipment != nullptr, Weapon != nullptr, Definition != nullptr, Inventory != nullptr,
			Definition ? Definition->ItemType == EASItemType::Weapon : -1,
			*GetNameSafe(Weapon ? Weapon->GetOuter() : nullptr));
		return false;
	}

	const UASWeaponAttackProfile* Profile = Definition->AttackProfile;
	UClass* ExecutionClass = Profile ? Profile->ExecutionClass.Get() : nullptr;
	OutExecution = ExecutionClass ? ExecutionClass->GetDefaultObject<UASWeaponAttackExecution>() : nullptr;
	if (!OutExecution)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PrimaryAttack] 컨텍스트 실패: Profile=%s ExecutionClass=%s"),
			*GetNameSafe(Profile), *GetNameSafe(ExecutionClass));
		return false;
	}

	OutContext.Avatar = Avatar;
	OutContext.AbilitySystem = ASC;
	OutContext.Inventory = Inventory;
	OutContext.Weapon = Weapon;
	OutContext.Definition = Definition;
	OutContext.Profile = Profile;
	return true;
}

void UASGameplayAbility_CharacterPrimaryAttack::ResolveAnimation(
	const FASWeaponAttackExecutionContext& Context, UAnimMontage*& OutMontage,
	FGameplayTag& OutHitEventTag, bool& bOutScaleToInterval) const
{
	OutMontage = nullptr;
	OutHitEventTag = ASGameplayTags::Event_Skill_Hit;
	bOutScaleToInterval = true;
	if (!Context.Profile) return;
	OutMontage = Context.Profile->AttackMontage;
	OutHitEventTag = Context.Profile->HitEventTag;
	bOutScaleToInterval = Context.Profile->bScaleMontageToAttackInterval;
}

void UASGameplayAbility_CharacterPrimaryAttack::ComputeLocalAim(FVector& OutAimDirection, FVector& OutTargetLocation) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const FVector Origin = Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector;
	OutAimDirection = Avatar ? Avatar->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
	OutTargetLocation = Origin;
	FVector CursorLocation;
	if (Avatar && AASSkillRangeIndicatorActor::GetCursorWorldLocation(Cast<APawn>(Avatar), CursorLocation))
	{
		const FVector ToCursor = (CursorLocation - Origin).GetSafeNormal2D();
		if (!ToCursor.IsNearlyZero()) OutAimDirection = ToCursor;
		OutTargetLocation = CursorLocation;
	}
}

void UASGameplayAbility_CharacterPrimaryAttack::SendAimToServer(
	const FVector& AimDirection, const FVector& TargetLocation, AActor* RequestedTarget,
	const uint32 RequestedMassTargetNetworkId)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;
	FGameplayAbilityTargetData_LocationInfo* AimData = new FGameplayAbilityTargetData_LocationInfo();
	AimData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	AimData->TargetLocation.LiteralTransform = FTransform(AimDirection.Rotation(), TargetLocation);
	FGameplayAbilityTargetDataHandle TargetData;
	TargetData.Add(AimData);
	if (IsValid(RequestedTarget))
	{
		FGameplayAbilityTargetData_ActorArray* ActorData = new FGameplayAbilityTargetData_ActorArray();
		ActorData->TargetActorArray.Add(TWeakObjectPtr<AActor>(RequestedTarget));
		TargetData.Add(ActorData);
	}
	if (RequestedMassTargetNetworkId != 0)
	{
		FASGameplayAbilityTargetData_MassTarget* MassData =
			new FASGameplayAbilityTargetData_MassTarget();
		MassData->MassNetworkId = RequestedMassTargetNetworkId;
		TargetData.Add(MassData);
	}
	ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(),
		TargetData, FGameplayTag(), ASC->ScopedPredictionKey);
}

void UASGameplayAbility_CharacterPrimaryAttack::WaitForClientAim()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	const FPredictionKey PredictionKey = CurrentActivationInfo.GetActivationPredictionKey();
	ASC->AbilityTargetDataSetDelegate(CurrentSpecHandle, PredictionKey).AddUObject(
		this, &UASGameplayAbility_CharacterPrimaryAttack::OnClientAimReceived);
	ASC->AbilityTargetDataCancelledDelegate(CurrentSpecHandle, PredictionKey).AddUObject(
		this, &UASGameplayAbility_CharacterPrimaryAttack::OnClientAimCancelled);
	ASC->CallReplicatedTargetDataDelegatesIfSet(CurrentSpecHandle, PredictionKey);
}

void UASGameplayAbility_CharacterPrimaryAttack::OnClientAimReceived(
	const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	if (bAttackStarted) return;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}
	FVector AimDirection;
	FVector TargetLocation;
	ComputeLocalAim(AimDirection, TargetLocation);
	AActor* RequestedTarget = nullptr;
	uint32 RequestedMassTargetNetworkId = 0;
	for (int32 DataIndex = 0; DataIndex < Data.Num(); ++DataIndex)
	{
		const FGameplayAbilityTargetData* AimData = Data.Get(DataIndex);
		if (!AimData)
		{
			continue;
		}
		if (AimData->GetScriptStruct() == FASGameplayAbilityTargetData_MassTarget::StaticStruct())
		{
			const FASGameplayAbilityTargetData_MassTarget* MassData =
				static_cast<const FASGameplayAbilityTargetData_MassTarget*>(AimData);
			RequestedMassTargetNetworkId = MassData->MassNetworkId;
			continue;
		}
		if (AimData->HasEndPoint())
		{
			AimDirection = AimData->GetEndPointTransform().GetRotation().Vector().GetSafeNormal2D();
			TargetLocation = AimData->GetEndPointTransform().GetLocation();
		}
		const TArray<TWeakObjectPtr<AActor>> TargetActors = AimData->GetActors();
		if (!TargetActors.IsEmpty())
		{
			RequestedTarget = TargetActors[0].Get();
		}
	}
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log,
		TEXT("[MassRangedTarget][T1Receive] Authority=%d Avatar=%s Target=(%.1f,%.1f,%.1f) Aim=(%.4f,%.4f,%.4f) RequestedActor=%s MassNetworkId=%u"),
		CurrentActorInfo && CurrentActorInfo->IsNetAuthority(), *GetNameSafe(GetAvatarActorFromActorInfo()),
		TargetLocation.X, TargetLocation.Y, TargetLocation.Z,
		AimDirection.X, AimDirection.Y, AimDirection.Z, *GetNameSafe(RequestedTarget), RequestedMassTargetNetworkId);
#endif
	StartAttack(AimDirection, TargetLocation, RequestedTarget, RequestedMassTargetNetworkId);
}

void UASGameplayAbility_CharacterPrimaryAttack::OnClientAimCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

bool UASGameplayAbility_CharacterPrimaryAttack::IsValidAttackRequest(
	const FASWeaponAttackExecutionContext& Context, AActor* RequestedTarget,
	const uint32 RequestedMassTargetNetworkId, const FVector& TargetLocation, FVector& OutAimDirection) const
{
	if (!Context.Avatar || !Context.Definition)
	{
		return false;
	}

	const UASRangedAttackProfile* RangedProfile = Cast<UASRangedAttackProfile>(Context.Profile);
	if (RangedProfile && !RangedProfile->bUseAttackMoveTargeting && IsValid(RequestedTarget))
	{
		// 방향성 투사체는 타깃 Actor를 받지 않는다. 클라이언트는 위치가 아니라 방향만 보낸다.
		return false;
	}

	if (RequestedMassTargetNetworkId != 0)
	{
		if (!RangedProfile || !RangedProfile->bUseAttackMoveTargeting || IsValid(RequestedTarget))
		{
			return false;
		}

		if (Context.Avatar->HasAuthority())
		{
			UASCombatTargetSubsystem* CombatTargets =
				Context.Avatar->GetWorld()->GetSubsystem<UASCombatTargetSubsystem>();
			FASCombatTargetRef ServerTarget;
			if (!CombatTargets || !CombatTargets->GetMassTargetByNetworkId(
				RequestedMassTargetNetworkId, ServerTarget))
			{
				return false;
			}

			const float AllowedRange = FMath::Max(0.0f, Context.Definition->AttackRange) + 15.0f
				+ ServerTarget.GetHitRadius();
			if (FVector::DistSquared2D(Context.Avatar->GetActorLocation(), ServerTarget.GetLocation())
				> FMath::Square(AllowedRange))
			{
				return false;
			}
		}

		OutAimDirection = (TargetLocation - Context.Avatar->GetActorLocation()).GetSafeNormal2D();
		if (OutAimDirection.IsNearlyZero())
		{
			OutAimDirection = Context.Avatar->GetActorForwardVector().GetSafeNormal2D();
		}
		return !OutAimDirection.IsNearlyZero();
	}

	if (!IsValid(RequestedTarget))
	{
		// 방향성 투사체와 기존 수동 조준은 대상 없이도 발사할 수 있다.
		OutAimDirection = (TargetLocation - Context.Avatar->GetActorLocation()).GetSafeNormal2D();
		if (OutAimDirection.IsNearlyZero())
		{
			OutAimDirection = Context.Avatar->GetActorForwardVector().GetSafeNormal2D();
		}
		return !OutAimDirection.IsNearlyZero();
	}

	if (!UASCombatTargetSubsystem::IsValidActorTarget(RequestedTarget, Context.Avatar, false))
	{
		return false;
	}

	// 서버도 공격자의 시야 팀에서 보이는 타깃인지 확인한다. 시야 컴포넌트가 없는 대상은
	// 기존 전투 규칙과 동일하게 보이는 대상으로 취급한다.
	if (Context.Avatar->HasAuthority())
	{
		if (const UASVisionSourceComponent* VisionSource = Context.Avatar->FindComponentByClass<UASVisionSourceComponent>())
		{
			if (const UASVisibilityComponent* Visibility = RequestedTarget->FindComponentByClass<UASVisibilityComponent>();
				Visibility && !Visibility->IsVisibleToTeam(VisionSource->GetTeamId()))
			{
				return false;
			}
		}
	}

	const FVector ToTarget = RequestedTarget->GetActorLocation() - Context.Avatar->GetActorLocation();
	const float AllowedRange = FMath::Max(0.0f, Context.Definition->AttackRange) + 15.0f;
	if (ToTarget.SizeSquared2D() > FMath::Square(AllowedRange))
	{
		return false;
	}

	OutAimDirection = ToTarget.GetSafeNormal2D();
	if (OutAimDirection.IsNearlyZero())
	{
		OutAimDirection = Context.Avatar->GetActorForwardVector().GetSafeNormal2D();
	}
	return !OutAimDirection.IsNearlyZero();
}

void UASGameplayAbility_CharacterPrimaryAttack::StartAttack(
	const FVector& AimDirection, const FVector& TargetLocation, AActor* RequestedTarget,
	const uint32 RequestedMassTargetNetworkId)
{
	if (bAttackStarted || !CurrentActorInfo) return;
	const UASWeaponAttackExecution* Execution = nullptr;
	FASWeaponAttackExecutionContext Context;
	if (!BuildExecutionContext(Context, Execution) || !Execution->CanPayCost(Context))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PrimaryAttack] 공격 시작 실패: 무기/비용 (Authority=%d)"), CurrentActorInfo->IsNetAuthority());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	FVector ValidAimDirection = AimDirection;
	if (!IsValidAttackRequest(Context, RequestedTarget, RequestedMassTargetNetworkId, TargetLocation, ValidAimDirection))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PrimaryAttack] 공격 시작 실패: 요청 검증 (Target=%s, Authority=%d)"),
			*GetNameSafe(RequestedTarget), CurrentActorInfo->IsNetAuthority());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	UWorld* World = Context.Avatar->GetWorld();
	if (!World || World->GetTimeSeconds() < NextAttackTime)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (CurrentActorInfo->IsNetAuthority() && !Execution->CommitCost(Context))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PrimaryAttack] 공격 시작 실패: 비용 지불 (Authority=%d)"), CurrentActorInfo->IsNetAuthority());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	bAttackStarted = true;
	bHitPerformed = false;
	NextAttackTime = World->GetTimeSeconds() + FMath::Max(0.01f, Context.Definition->AttackInterval);
	CachedAimDirection = ValidAimDirection.GetSafeNormal2D();
	if (CachedAimDirection.IsNearlyZero()) CachedAimDirection = Context.Avatar->GetActorForwardVector().GetSafeNormal2D();
	Context.AimDirection = CachedAimDirection;
	Context.RequestedTarget = RequestedTarget;
	Context.RequestedMassTargetNetworkId = RequestedMassTargetNetworkId;
	Context.TargetLocation = TargetLocation;
	CachedExecutionContext = Context;
	CachedExecution = Execution;
	CachedRequestedTarget = RequestedTarget;
#if !UE_BUILD_SHIPPING
	if (CurrentActorInfo->IsNetAuthority())
	{
		UE_LOG(LogTemp, Log,
			TEXT("[MassRangedTarget][T1Start] Avatar=%s Target=(%.1f,%.1f,%.1f) Aim=(%.4f,%.4f,%.4f) RequestedActor=%s MassNetworkId=%u"),
			*GetNameSafe(Context.Avatar), TargetLocation.X, TargetLocation.Y, TargetLocation.Z,
			CachedAimDirection.X, CachedAimDirection.Y, CachedAimDirection.Z, *GetNameSafe(RequestedTarget),
			RequestedMassTargetNetworkId);
	}
#endif
	// 공격이 시작되는 순간 걷기를 멈추고 대상 방향으로 몸을 돌린다.
	// (어택땅 중 사거리에 적이 들어와 공격할 때, 남은 이동 때문에 몸이 이동 방향으로 다시 돌아가는 것을 막는다)
	if (ACharacter* AttackingCharacter = Cast<ACharacter>(Context.Avatar))
	{
		if (AController* AttackingController = AttackingCharacter->GetController())
		{
			AttackingController->StopMovement();
		}
		AttackingCharacter->GetCharacterMovement()->StopMovementImmediately();
	}
	Context.Avatar->SetActorRotation(FRotator(0.0f, CachedAimDirection.Rotation().Yaw, 0.0f));

	bool bScaleToInterval = true;
	UAnimMontage* ResolvedMontage = nullptr;
	ResolveAnimation(Context, ResolvedMontage, ActiveHitEventTag, bScaleToInterval);
	ActiveMontage = ResolvedMontage;
	if (ActiveMontage)
	{
		if (ActiveHitEventTag.IsValid())
		{
			UAbilityTask_WaitGameplayEvent* WaitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
				this, ActiveHitEventTag, nullptr, true, true);
			WaitTask->EventReceived.AddDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnHitEvent);
			WaitTask->ReadyForActivation();
		}
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, ActiveMontage, GetMontagePlayRate(ActiveMontage, Context.Definition, bScaleToInterval),
			NAME_None, true);
		MontageTask->OnCompleted.AddDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnMontageFinished);
		MontageTask->OnBlendOut.AddDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnMontageFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnMontageCancelled);
		MontageTask->OnCancelled.AddDynamic(this, &UASGameplayAbility_CharacterPrimaryAttack::OnMontageCancelled);
		MontageTask->ReadyForActivation();
		if (!ActiveHitEventTag.IsValid() && CurrentActorInfo->IsNetAuthority()) PerformHit();
		return;
	}

	if (CurrentActorInfo->IsNetAuthority())
	{
		PerformHit();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	// A predicting client with no montage waits for the server's replicated end. Ending here can
	// race the TargetData RPC and cancel the authoritative activation before it receives the aim.
}

void UASGameplayAbility_CharacterPrimaryAttack::PerformHit()
{
	if (bHitPerformed || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return;
	bHitPerformed = true;

	// The equipped weapon is authoritative. Do not finish an old weapon's attack after a swap.
	FASWeaponAttackExecutionContext CurrentContext;
	const UASWeaponAttackExecution* CurrentExecution = nullptr;
	if (!BuildExecutionContext(CurrentContext, CurrentExecution)
		|| CurrentContext.Weapon != CachedExecutionContext.Weapon
		|| CurrentExecution != CachedExecution
		|| CurrentContext.AbilitySystem->HasMatchingGameplayTag(ASGameplayTags::State_Dead))
	{
		return;
	}

	if (const UASWeaponAttackExecution* Execution = CachedExecution)
	{
		Execution->ExecuteHit(CachedExecutionContext);
	}
}

float UASGameplayAbility_CharacterPrimaryAttack::GetMontagePlayRate(
	const UAnimMontage* Montage, const UASItemDefinition* Definition, const bool bScaleToInterval) const
{
	if (!bScaleToInterval || !Montage || !Definition || Definition->AttackInterval <= 0.0f) return 1.0f;
	return FMath::Max(1.0f, Montage->GetPlayLength() / Definition->AttackInterval);
}

void UASGameplayAbility_CharacterPrimaryAttack::OnHitEvent(FGameplayEventData Payload)
{
	PerformHit();
}

void UASGameplayAbility_CharacterPrimaryAttack::OnMontageFinished()
{
	if (!IsActive() || !CurrentActorInfo)
	{
		return;
	}

	// LocalPredicted 일반 공격은 클라이언트와 서버에 각각 Ability 인스턴스가 있다.
	// 예측 클라이언트가 여기서 EndAbility를 복제하면, 서버가 AnimNotify/Fallback으로
	// PerformHit를 호출하기 전에 authoritative Ability가 끝날 수 있다. 그 결과 클라이언트에는
	// 공격 몽타주만 보이고 서버 피해 판정과 디버그 콘은 실행되지 않는 레이스가 생긴다.
	// 타격이 시작된 뒤의 종료 권한은 서버에만 둔다. 클라이언트는 서버의 종료 복제를 기다린다.
	if (!CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	if (!bHitPerformed)
	{
		PerformHit();
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UASGameplayAbility_CharacterPrimaryAttack::OnMontageCancelled()
{
	if (!IsActive() || !CurrentActorInfo)
	{
		return;
	}

	// 로컬 몽타주의 중단은 표현 계층의 이벤트일 수 있으므로 서버 공격을 취소하지 않는다.
	// 서버가 실제 취소 여부와 Ability 종료를 권위적으로 결정해 클라이언트에 복제한다.
	if (!CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
