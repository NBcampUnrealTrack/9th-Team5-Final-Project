#include "GAS/Abilities/ASGameplayAbility_Dodge.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Core/Framework/ASPlayerController.h"
#include "GameFramework/Character.h"
#include "GAS/ASAbilityTags.h"

UASGameplayAbility_Dodge::UASGameplayAbility_Dodge()
{
	// 액터마다 인스턴스 하나. 상태(태스크, 델리게이트)를 들고 있어야 해서 CDO 로는 안 된다.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 클라이언트가 먼저 실행하고 서버가 확인한다. 누르자마자 움직여야 하는 입력이라 예측이 필요하다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 실행 중에는 State.Dodging 을 갖고, 그 태그가 있으면 다시 발동하지 못한다(연타 방지).
	ActivationOwnedTags.AddTag(ASGameplayTags::State_Dodging);
	ActivationBlockedTags.AddTag(ASGameplayTags::State_Dodging);
}

void UASGameplayAbility_Dodge::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get();
	const FPredictionKey PredictionKey = ActivationInfo.GetActivationPredictionKey();

	if (IsLocallyControlled())
	{
		// 방향을 먼저 구한다. 비용을 낸 뒤에 방향이 없어서 취소하면 스태미나만 날린다.
		FVector Direction;
		if (!TryGetCursorDirection(Direction))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		// 비용과 쿨타임을 한 번에 확인하고 적용한다. 스태미나가 모자라거나 쿨타임이면 여기서 끝난다.
		if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		// 클라이언트면 방향을 서버로 보낸다. 호스트는 자기가 서버라 보낼 필요가 없다.
		if (!ActorInfo->IsNetAuthority())
		{
			FGameplayAbilityTargetData_LocationInfo* LocationData = new FGameplayAbilityTargetData_LocationInfo();
			LocationData->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
			LocationData->TargetLocation.LiteralTransform = FTransform(Direction);

			FGameplayAbilityTargetDataHandle TargetData;
			TargetData.Add(LocationData);

			AbilitySystemComponent->CallServerSetReplicatedTargetData(Handle, PredictionKey, TargetData, FGameplayTag(), AbilitySystemComponent->ScopedPredictionKey);
		}

		PerformDodge(Direction);
		return;
	}

	// 서버에서 돌아가는 원격 클라이언트의 어빌리티. 방향은 클라이언트가 보내 준다.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 데이터가 아직 안 왔으면 기다리고, 이미 와 있으면(순서가 바뀐 경우) 바로 처리한다.
	AbilitySystemComponent->AbilityTargetDataSetDelegate(Handle, PredictionKey).AddUObject(this, &UASGameplayAbility_Dodge::OnServerTargetDataReceived);
	AbilitySystemComponent->CallReplicatedTargetDataDelegatesIfSet(Handle, PredictionKey);
}

void UASGameplayAbility_Dodge::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicatedEndAbility,
	bool bWasCancelled)
{
	if (UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		AbilitySystemComponent->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).RemoveAll(this);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicatedEndAbility, bWasCancelled);
}

bool UASGameplayAbility_Dodge::TryGetCursorDirection(FVector& OutDirection) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const AASPlayerController* PlayerController = Cast<AASPlayerController>(GetCurrentActorInfo()->PlayerController.Get());
	if (!IsValid(Avatar) || !IsValid(PlayerController))
	{
		return false;
	}

	FVector CursorLocation;
	if (!PlayerController->GetCursorWorldLocation(CursorLocation))
	{
		return false;
	}

	// 바닥 위 방향만 쓴다. 커서가 캐릭터 바로 위면 방향이 안 나오므로 바라보는 쪽으로 간다.
	OutDirection = (CursorLocation - Avatar->GetActorLocation()).GetSafeNormal2D();
	if (OutDirection.IsNearlyZero())
	{
		OutDirection = Avatar->GetActorForwardVector().GetSafeNormal2D();
	}

	return !OutDirection.IsNearlyZero();
}

void UASGameplayAbility_Dodge::OnServerTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* AbilitySystemComponent = GetCurrentActorInfo()->AbilitySystemComponent.Get();

	// 받은 데이터는 바로 비운다. 남겨 두면 다음 발동에 섞인다.
	AbilitySystemComponent->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

	const FVector Direction = Data.IsValid(0) ? Data.Get(0)->GetEndPoint().GetSafeNormal2D() : FVector::ZeroVector;
	if (Direction.IsNearlyZero())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	PerformDodge(Direction);
}

void UASGameplayAbility_Dodge::PerformDodge(const FVector& Direction)
{
	// 회피 방향을 보게 돌린다. 이동 컴포넌트가 이동 방향으로 몸을 돌리기도 하지만 첫 프레임부터 맞추려는 것이다.
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->SetActorRotation(Direction.Rotation());
	}

	// 루트 모션 태스크는 클라이언트 예측과 복제를 이동 컴포넌트가 처리한다.
	// 끝나면 속도를 0 으로 두어 미끄러지지 않게 한다.
	UAbilityTask_ApplyRootMotionConstantForce* DodgeTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this,
		NAME_None,
		Direction,
		DodgeDistance / DodgeDuration,
		DodgeDuration,
		/*bIsAdditive*/ false,
		/*StrengthOverTime*/ nullptr,
		ERootMotionFinishVelocityMode::SetVelocity,
		FVector::ZeroVector,
		/*ClampVelocityOnFinish*/ 0.0f,
		/*bEnableGravity*/ false);

	DodgeTask->OnFinish.AddDynamic(this, &UASGameplayAbility_Dodge::OnDodgeFinished);
	DodgeTask->ReadyForActivation();
}

void UASGameplayAbility_Dodge::OnDodgeFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
