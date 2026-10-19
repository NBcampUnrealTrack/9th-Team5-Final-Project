#include "ASSkillAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Core/Combat/ASCombatTargetSubsystem.h"
#include "Core/Framework/ASPlayerController.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Navigation/PathFollowingComponent.h"
#include "Skills/ASSkillTags.h"
#include "Skills/Data/ASSkillData.h"
#include "AbilitySystemGlobals.h"
#include "GAS/ASAbilityTags.h"
#include "GAS/Attributes/ASPlayerAttributeSet.h"
#include "Skills/Indicator/ASSkillRangeIndicatorActor.h"
#include "Skills/Settings/ASSkillSettingsSubsystem.h"
#include "TimerManager.h"
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

	//DT 수치 반영 (SkillData가 없으면 BP 값 그대로)
	if (const FASSkillStatsRow* Stats = FindSkillStats())
	{
		ApplySkillStats(*Stats);
	}

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
	bIsWalkingToCast = false;
	StopTargetingInput();
	StopPendingWatch();
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

	//걸어가서 시전 모드면 커서가 사거리 밖이어도 효과 원이 커서를 따라감
	Settings.bClampToRange = !(TargetingType == EASSkillTargetingType::Location && ShouldWalkToCast());
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
		&& IsLocallyControlled()
		&& GetCastMode() != EASSkillCastMode::QuickCast;
}

EASSkillCastMode UASSkillAbility::GetCastMode() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const UASSkillSettingsSubsystem* SkillSettings = UASSkillSettingsSubsystem::Get(ActorInfo ? ActorInfo->PlayerController.Get() : nullptr);
	return SkillSettings ? SkillSettings->GetCastMode() : EASSkillCastMode::NormalCast;
}

void UASSkillAbility::BeginTargeting()
{
	bIsTargeting = true;
	bConfirmOnRelease = GetCastMode() != EASSkillCastMode::NormalCast;

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

	//일반 시전: 좌클릭을 조준 확정으로 받음 (ASC가 State.Targeting을 보고 기본 공격 대신 확정 신호를 보냄)
	if (!bConfirmOnRelease)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			//입력은 본인 화면에서만 처리하므로 복제하지 않는 로컬 태그
			ASC->AddLooseGameplayTag(ASGameplayTags::State_Targeting);
			bAddedTargetingTag = true;
			ASC->GenericLocalConfirmCallbacks.AddDynamic(this, &UASSkillAbility::OnLocalInputConfirm);
			bBoundLocalConfirm = true;
		}
	}

	//우클릭 이동·다른 스킬 사용 감지
	StartPendingWatch();

	//범위 표시 후 빠른 시전: 발동되는 사이에 이미 키를 뗐으면 바로 시전
	if (bConfirmOnRelease)
	{
		if (const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec(); Spec && !Spec->InputPressed)
		{
			ConfirmTargeting();
		}
	}
}

void UASSkillAbility::InputPressed(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	if (bIsTargeting && !bConfirmOnRelease)
	{
		CancelPendingCast();
	}
}

void UASSkillAbility::OnLocalInputConfirm()
{
	//LocalInputConfirm은 호출 후 구독을 비우므로 표시만 정리
	bBoundLocalConfirm = false;

	if (bIsTargeting && !bConfirmOnRelease)
	{
		ConfirmTargeting();
	}
}

void UASSkillAbility::StopTargetingInput()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (bAddedTargetingTag)
		{
			ASC->RemoveLooseGameplayTag(ASGameplayTags::State_Targeting);
		}

		if (bBoundLocalConfirm)
		{
			ASC->GenericLocalConfirmCallbacks.RemoveDynamic(this, &UASSkillAbility::OnLocalInputConfirm);
		}
	}

	bAddedTargetingTag = false;
	bBoundLocalConfirm = false;
}

void UASSkillAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	if (bIsTargeting && bConfirmOnRelease)
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
	StopTargetingInput();

	FVector AimDirection;
	FVector TargetLocation;
	ComputeAim(AimDirection, TargetLocation);

	//사거리 밖을 지정했고 걸어가서 시전 모드면 이동 후 시전
	FVector WalkTarget;
	if (GetOutOfRangeWalkTarget(WalkTarget))
	{
		BeginWalkToCast(WalkTarget);
		return;
	}

	StopPendingWatch();
	DestroyRangeIndicator();
	CastAtTarget(AimDirection, TargetLocation);
}

void UASSkillAbility::CastAtTarget(const FVector& AimDirection, const FVector& TargetLocation)
{
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

bool UASSkillAbility::ShouldWalkToCast() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const APlayerController* PlayerController = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;

	//설정은 로컬 플레이어에만 있음 (서버의 원격 플레이어·AI는 걸어가기 판단을 하지 않음)
	const UASSkillSettingsSubsystem* SkillSettings = UASSkillSettingsSubsystem::Get(PlayerController);
	return SkillSettings && SkillSettings->GetWalkToCastWhenOutOfRange();
}

bool UASSkillAbility::GetOutOfRangeWalkTarget(FVector& OutTargetLocation) const
{
	if (TargetingType != EASSkillTargetingType::Location || !ShouldWalkToCast())
	{
		return false;
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	FVector CursorLocation;
	if (!Avatar || !AASSkillRangeIndicatorActor::GetCursorWorldLocation(Cast<APawn>(Avatar), CursorLocation))
	{
		return false;
	}

	//사거리 제한 없이 바닥 높이만 맞춘 커서 위치
	OutTargetLocation = AASSkillRangeIndicatorActor::ResolveTargetLocation(Avatar, CursorLocation, 0.0f);
	return FVector::DistSquared2D(Avatar->GetActorLocation(), OutTargetLocation) > FMath::Square(MaxCastRange);
}

void UASSkillAbility::BeginWalkToCast(const FVector& InTargetLocation)
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	APlayerController* PlayerController = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;

	if (!World || !PlayerController)
	{
		CancelPendingCast();
		return;
	}

	bIsWalkingToCast = true;
	WalkTargetLocation = InTargetLocation;
	WalkStartTime = World->GetTimeSeconds();

	//걷는 동안은 범위 표시를 지움 (롤과 같음)
	DestroyRangeIndicator();

	//클릭 이동과 같은 경로찾기 이동
	if (AASPlayerController* ASPlayerController = Cast<AASPlayerController>(PlayerController))
	{
		ASPlayerController->RequestMoveTo(InTargetLocation);
	}
	else
	{
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(PlayerController, InTargetLocation);
	}

	//방금 보낸 이동 요청 번호를 기록 (이후 바뀌면 다른 이동 명령)
	StartPendingWatch();
	UpdatePendingCast();
}

void UASSkillAbility::StartPendingWatch()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	APlayerController* PlayerController = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;

	WatchedPathFollowing = PlayerController ? PlayerController->FindComponentByClass<UPathFollowingComponent>() : nullptr;
	WatchedMoveRequestId = WatchedPathFollowing.IsValid() ? WatchedPathFollowing->GetCurrentRequestId() : FAIRequestID::InvalidRequest;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && !OtherAbilityActivatedHandle.IsValid())
	{
		OtherAbilityActivatedHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &UASSkillAbility::OnOtherAbilityActivated);
		OtherAbilityFailedHandle = ASC->AbilityFailedCallbacks.AddUObject(this, &UASSkillAbility::OnOtherAbilityFailed);
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (UWorld* World = Avatar ? Avatar->GetWorld() : nullptr)
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (!TimerManager.IsTimerActive(PendingWatchTimerHandle))
		{
			TimerManager.SetTimer(PendingWatchTimerHandle,
				FTimerDelegate::CreateUObject(this, &UASSkillAbility::UpdatePendingCast), 0.05f, true);
		}
	}
}

void UASSkillAbility::StopPendingWatch()
{
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (UWorld* World = Avatar->GetWorld())
		{
			World->GetTimerManager().ClearTimer(PendingWatchTimerHandle);
		}
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AbilityActivatedCallbacks.Remove(OtherAbilityActivatedHandle);
		ASC->AbilityFailedCallbacks.Remove(OtherAbilityFailedHandle);
	}

	OtherAbilityActivatedHandle.Reset();
	OtherAbilityFailedHandle.Reset();
	WatchedPathFollowing.Reset();
}

void UASSkillAbility::UpdatePendingCast()
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		CancelPendingCast();
		return;
	}

	//새 이동 명령 = 유효한 요청 번호가 기록한 번호와 다름
	//이동이 끝나면 번호가 Invalid로 초기화되므로 그건 새 명령으로 보지 않음
	const UPathFollowingComponent* PathFollowing = WatchedPathFollowing.Get();
	const FAIRequestID CurrentMoveRequestId = PathFollowing ? PathFollowing->GetCurrentRequestId() : FAIRequestID::InvalidRequest;
	const bool bMoveReplaced = CurrentMoveRequestId.IsValid() && !(CurrentMoveRequestId == WatchedMoveRequestId);

	if (bIsTargeting)
	{
		//일반 시전 조준 중 우클릭 이동 → 조준 취소 (이동은 그대로 진행)
		//범위 표시 후 빠른 시전은 키를 누른 채 이동할 수 있으므로 취소하지 않음
		if (!bConfirmOnRelease && bMoveReplaced)
		{
			CancelPendingCast();
		}
		return;
	}

	if (!bIsWalkingToCast)
	{
		StopPendingWatch();
		return;
	}

	//사거리 안에 들어오면 멈추고 시전
	if (FVector::DistSquared2D(Avatar->GetActorLocation(), WalkTargetLocation) <= FMath::Square(MaxCastRange))
	{
		const FVector TargetLocation = WalkTargetLocation;
		bIsWalkingToCast = false;
		StopPendingWatch();

		if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
		{
			if (APlayerController* PlayerController = ActorInfo->PlayerController.Get())
			{
				PlayerController->StopMovement();
			}
		}

		FVector AimDirection = (TargetLocation - Avatar->GetActorLocation()).GetSafeNormal2D();
		if (AimDirection.IsNearlyZero())
		{
			AimDirection = Avatar->GetActorForwardVector().GetSafeNormal2D();
		}

		CastAtTarget(AimDirection, TargetLocation);
		return;
	}

	//시간 초과
	if (World->GetTimeSeconds() - WalkStartTime > WalkToCastTimeout)
	{
		CancelPendingCast();
		return;
	}

	//다른 이동 명령이 들어왔거나, 이동이 멈춤 (갈 수 없는 곳 등)
	if (bMoveReplaced || (PathFollowing && PathFollowing->GetStatus() == EPathFollowingStatus::Idle))
	{
		CancelPendingCast();
	}
}

void UASSkillAbility::CancelPendingCast()
{
	//조준·걸어가기 정리는 EndAbility에서 함
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UASSkillAbility::OnOtherAbilityActivated(UGameplayAbility* ActivatedAbility)
{
	if ((bIsTargeting || bIsWalkingToCast) && ActivatedAbility != this)
	{
		CancelPendingCast();
	}
}

void UASSkillAbility::OnOtherAbilityFailed(const UGameplayAbility* FailedAbility, const FGameplayTagContainer& FailureTags)
{
	if (!(bIsTargeting || bIsWalkingToCast) || !FailedAbility || FailedAbility == this || FailedAbility->GetClass() == GetClass())
	{
		return;
	}

	//조준·이동 중인 이 스킬 때문에 막혔을 수 있으므로, 취소한 뒤 다음 틱에 그 스킬을 다시 발동
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const FGameplayAbilitySpec* FailedSpec = ASC ? ASC->FindAbilitySpecFromClass(FailedAbility->GetClass()) : nullptr;
	const FGameplayAbilitySpecHandle RetryHandle = FailedSpec ? FailedSpec->Handle : FGameplayAbilitySpecHandle();

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;

	CancelPendingCast();

	if (ASC && World && RetryHandle.IsValid())
	{
		TWeakObjectPtr<UAbilitySystemComponent> WeakASC = ASC;
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(ASC, [WeakASC, RetryHandle]()
		{
			if (UAbilitySystemComponent* RetryASC = WeakASC.Get())
			{
				RetryASC->TryActivateAbility(RetryHandle);
			}
		}));
	}
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
	return UASCombatTargetSubsystem::IsValidActorTarget(Target, Instigator, bCanHitPlayers);
}

TArray<AActor*> UASSkillAbility::FindSkillTargets(const UWorld* World, const FVector& Center, float Radius, const AActor* Instigator, bool bCanHitPlayers)
{
	const UASCombatTargetSubsystem* CombatTargets = World
		? World->GetSubsystem<UASCombatTargetSubsystem>()
		: nullptr;
	return CombatTargets
		? CombatTargets->FindActorTargetsInRadius(Center, Radius, Instigator, bCanHitPlayers)
		: TArray<AActor*>();
}

void UASSkillAbility::ApplyDamageSpecToTarget(const FGameplayEffectSpecHandle& DamageSpec, AActor* Target)
{
	UASCombatTargetSubsystem::ApplyEffectSpecToActor(DamageSpec, Target);
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

const FASSkillStatsRow* UASSkillAbility::FindSkillStats() const
{
	return SkillData ? SkillData->FindStats() : nullptr;
}

void UASSkillAbility::ApplySkillStats(const FASSkillStatsRow& Stats)
{
	BaseDamage = Stats.BaseDamage;
	AttackPowerScale = Stats.AttackPowerScale;
	MaxCastRange = Stats.MaxCastRange;
}

void UASSkillAbility::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	const FASSkillStatsRow* Stats = FindSkillStats();
	const bool bOverrideDuration = Stats && Stats->Cooldown > 0.0f;
	const bool bAddTag = SkillData && SkillData->CooldownTag.IsValid();
	if (!CooldownGE || (!bOverrideDuration && !bAddTag))
	{
		//몬스터 등 SkillData 없음: GE에 적힌 시간·태그 그대로
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, CooldownGE->GetClass(), GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid())
	{
		return;
	}

	if (bOverrideDuration)
	{
		//true = 이후 재계산으로 덮어써지지 않게 고정
		SpecHandle.Data->SetDuration(Stats->Cooldown, true);
	}

	if (bAddTag)
	{
		//공용 GE라 태그가 없으므로 이 스킬의 쿨타임 태그를 붙임
		SpecHandle.Data->DynamicGrantedTags.AddTag(SkillData->CooldownTag);
	}

	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}

const FGameplayTagContainer* UASSkillAbility::GetCooldownTags() const
{
	if (!SkillData || !SkillData->CooldownTag.IsValid())
	{
		return Super::GetCooldownTags();
	}

	//DA 태그만 사용 (공용 GE의 태그는 모든 스킬이 같이 붙이므로 넣으면 스킬 하나가 쿨타임일 때 전부 막힘)
	CooldownTagsCache.Reset();
	CooldownTagsCache.AddTag(SkillData->CooldownTag);
	return &CooldownTagsCache;
}

bool UASSkillAbility::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const FASSkillStatsRow* Stats = FindSkillStats();
	if (!Stats || Stats->ManaCost <= 0.0f)
	{
		//SkillData 없음 또는 ManaCost 0: 코스트 GE에 적힌 값으로 확인
		return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
	}

	//SetByCaller 값은 기본 확인 로직이 몰라서 현재 마나와 직접 비교
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	bool bFound = false;
	const float CurrentMana = ASC
		? ASC->GetGameplayAttributeValue(UASPlayerAttributeSet::GetManaAttribute(), bFound)
		: 0.0f;

	if (!bFound || CurrentMana < Stats->ManaCost)
	{
		const FGameplayTag& CostFailTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;
		if (OptionalRelevantTags && CostFailTag.IsValid())
		{
			OptionalRelevantTags->AddTag(CostFailTag);
		}
		return false;
	}

	return true;
}

void UASSkillAbility::ApplyCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const UGameplayEffect* CostGE = GetCostGameplayEffect();
	const FASSkillStatsRow* Stats = FindSkillStats();
	if (!CostGE || !Stats || Stats->ManaCost <= 0.0f)
	{
		Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, CostGE->GetClass(), GetAbilityLevel(Handle, ActorInfo));
	if (SpecHandle.IsValid())
	{
		//GE_SkillCost는 Mana Add라서 음수로 넣음
		SpecHandle.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_ManaCost, -Stats->ManaCost);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
	}
}

UTexture2D* UASSkillAbility::GetSkillIcon() const
{
	if (SkillData && SkillData->Icon)
	{
		return SkillData->Icon;
	}
	return SkillIcon;
}
