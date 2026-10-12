#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AITypes.h"
#include "AttributeSet.h"
#include "Engine/TimerHandle.h"
#include "Skills/Indicator/ASSkillIndicatorTypes.h"
#include "ASSkillAbility.generated.h"

class AASSkillRangeIndicatorActor;
enum class EASSkillCastMode : uint8;
class ACharacter;
class UPathFollowingComponent;
class UAnimMontage;
class UGameplayEffect;
class UTexture2D;

/*
 * Base for Skills
 */
UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASSkillAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UASSkillAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicatedEndAbility,
		bool bWasCancelled) override;

	//일반 시전 조준 중 같은 키를 다시 누르면 취소
	virtual void InputPressed(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	//범위 표시 후 빠른 시전: 꾹 누르던 키를 떼면 조준 확정
	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	//공격 대상이 될 수 있는지 (자신·아군 제외, ASC 보유)
	static bool IsValidSkillTarget(const AActor* Target, const AActor* Instigator, bool bCanHitPlayers);

	//원 범위 안의 공격 대상
	static TArray<AActor*> FindSkillTargets(const UWorld* World, const FVector& Center, float Radius, const AActor* Instigator, bool bCanHitPlayers);

	//데미지 스펙을 대상에게 적용 (서버에서 호출)
	static void ApplyDamageSpecToTarget(const FGameplayEffectSpecHandle& DamageSpec, AActor* Target);
	
	//스킬 아이콘 반환
	UTexture2D* GetSkillIcon() const;
	
protected:
	//스킬 효과. 타격 타이밍마다 호출
	UFUNCTION(BlueprintNativeEvent, Category = "AS|Skill")
	void OnSkillEffect();

	//BaseDamage + 공격력 × AttackPowerScale
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "AS|Skill|Damage")
	float CalculateDamage() const;

	//조준 방향 (기본: 캐릭터 정면)
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "AS|Skill")
	FVector GetAimDirection() const;

	//위치형: 시전 순간 확정된 목표 지점 (그 외: 시전자 위치)
	UFUNCTION(BlueprintPure, Category = "AS|Skill")
	FVector GetTargetLocation() const;

	//인디케이터에 넘길 범위 정보 (자식 클래스가 모양·크기 채움)
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "AS|Skill|Indicator")
	FASSkillIndicatorSettings GetIndicatorSettings() const;

	//스킬을 쓴 캐릭터 (Avatar)
	UFUNCTION(BlueprintPure, Category = "AS|Skill")
	ACharacter* GetOwnerCharacter() const;

	//범위 안의 공격 대상
	UFUNCTION(BlueprintCallable, Category = "AS|Skill|Target")
	TArray<AActor*> FindTargetsInRadius(const FVector& Center, float Radius) const;

	//데미지 스펙 생성 (투사체·범위 액터에 넘길 때 사용)
	UFUNCTION(BlueprintCallable, Category = "AS|Skill|Damage")
	FGameplayEffectSpecHandle MakeDamageSpec() const;

	//데미지 적용 (서버 전용)
	UFUNCTION(BlueprintCallable, Category = "AS|Skill|Damage")
	void ApplySkillDamage(AActor* Target);

	//서버인지
	bool HasSkillAuthority() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Animation")
	TObjectPtr<UAnimMontage> AnimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	//타격 타이밍 이벤트 (비우면 시작 즉시 발동)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Animation", meta = (Categories = "Event"))
	FGameplayTag HitEventTag;

	//Health Add, SetByCaller(Data.Damage)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Damage", meta = (ClampMin = "0.0"))
	float AttackPowerScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Damage")
	FGameplayAttribute AttackPowerAttribute;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Target")
	bool bCanHitPlayers = false;

	//마우스 커서로 조준 (클라이언트가 조준을 서버로 전송)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Aim")
	bool bAimAtCursor = true;

	//조준 방식 (방향 / 위치 / 시전자 중심)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Aim")
	EASSkillTargetingType TargetingType = EASSkillTargetingType::Direction;

	//위치형 최대 사거리 (커서가 밖이면 사거리 끝에 시전)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Aim", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxCastRange = 800.0f;

	//범위 표시 사용 (끄면 시전 방식 설정과 상관없이 누르는 즉시 시전)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Indicator")
	bool bShowRangeIndicator = true;

	//걸어가서 시전할 때 이 시간 안에 사거리에 못 들어가면 취소
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Aim", meta = (ClampMin = "0.5", Units = "s"))
	float WalkToCastTimeout = 5.0f;

	//범위 표시 액터 (머티리얼을 지정한 BP 자식)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Indicator", meta = (EditCondition = "bShowRangeIndicator"))
	TSubclassOf<AASSkillRangeIndicatorActor> RangeIndicatorClass;

	//시전 시 캐릭터를 조준 방향으로 회전
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Aim")
	bool bFaceAimDirection = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Debug")
	bool bDrawDebug = false;

	//스킬 아이콘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Icon")
	TObjectPtr<UTexture2D> SkillIcon;
	
	UFUNCTION()
	virtual void OnMontageCompleted();

	UFUNCTION()
	virtual void OnMontageInterrupted();

private:
	UFUNCTION()
	void HandleHitEvent(FGameplayEventData Payload);

	void FinishSkill(bool bWasCancelled);

	//시전 순간의 조준 방향 계산 (커서는 본인 클라이언트·호스트에서만 읽힘)
	FVector ComputeAimDirection() const;

	//조준 방향과 목표 지점 계산 (위치형은 사거리 안으로 제한)
	void ComputeAim(FVector& OutAimDirection, FVector& OutTargetLocation) const;

	//받은 목표 지점을 조준 방식에 맞게 보정 (서버 검증용)
	FVector ResolveAimLocation(const FVector& DesiredLocation) const;

	//조준이 정해진 뒤 실제 스킬 진행 (회전, 몽타주, 효과)
	void StartSkill(const FVector& AimDirection, const FVector& TargetLocation);

	//범위 표시 조준을 쓰는 상황인지 (본인 화면의 플레이어, 빠른 시전 모드가 아님)
	bool ShouldUseRangeIndicator() const;

	//현재 시전 방식 (설정이 없으면 일반 시전)
	EASSkillCastMode GetCastMode() const;

	//조준 시작: 인디케이터 생성 후 시전 방식에 따라 키 떼기 또는 좌클릭 대기
	void BeginTargeting();

	//조준 확정 (키 떼기 / 좌클릭): 사거리 밖이면 걸어가기, 아니면 바로 시전
	void ConfirmTargeting();

	//일반 시전 좌클릭 확정 신호 (ASC의 LocalInputConfirm, 다이나믹 델리게이트라 UFUNCTION)
	UFUNCTION()
	void OnLocalInputConfirm();

	//조준 입력 정리 (조준 태그·확정 대기 해제)
	void StopTargetingInput();

	//비용·쿨타임 차감, 서버 전송, 스킬 시작
	void CastAtTarget(const FVector& AimDirection, const FVector& TargetLocation);

	//걸어가서 시전 설정이 켜져 있는지 (본인 화면 기준)
	bool ShouldWalkToCast() const;

	//위치형에서 커서가 사거리 밖이고 걸어가기 모드면 목표 지점 반환
	bool GetOutOfRangeWalkTarget(FVector& OutTargetLocation) const;

	//목표 지점으로 이동 시작, 사거리에 들어오면 시전
	void BeginWalkToCast(const FVector& InTargetLocation);

	//조준·걸어가기 중 감시 시작 (이동 명령 번호 기록, 다른 스킬 감지, 타이머)
	void StartPendingWatch();

	//감시 정리 (타이머·델리게이트 해제)
	void StopPendingWatch();

	//조준 중 우클릭 이동, 걷는 중 도착·취소 조건 확인 (타이머)
	void UpdatePendingCast();

	//조준·걸어가기 취소 후 스킬 종료
	void CancelPendingCast();

	//조준·걸어가기 중 다른 어빌리티가 발동되면 취소
	void OnOtherAbilityActivated(UGameplayAbility* ActivatedAbility);

	//조준·걸어가기 중 다른 스킬이 막혀서 실패하면 취소 후 그 스킬을 다시 발동
	void OnOtherAbilityFailed(const UGameplayAbility* FailedAbility, const FGameplayTagContainer& FailureTags);

	void DestroyRangeIndicator();

	//서버에서 도는 원격 플레이어의 커서 조준 스킬인지 (클라이언트가 보낸 조준을 기다려야 함)
	bool ShouldWaitForClientAim() const;

	//클라이언트: 조준 방향·목표 지점을 서버로 전송
	void SendAimToServer(const FVector& AimDirection, const FVector& TargetLocation);

	//서버: 클라이언트가 보낸 조준을 기다림
	void WaitForClientAim();

	void OnClientAimReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void OnClientAimCancelled();

	//시전 순간에 저장한 조준 (타격 때 사용)
	FVector CachedAimDirection = FVector::ForwardVector;
	FVector CachedTargetLocation = FVector::ZeroVector;
	bool bHasCachedAim = false;

	//조준 중 (범위 표시)
	bool bIsTargeting = false;

	//true = 키를 떼면 확정, false = 좌클릭으로 확정 (일반 시전)
	bool bConfirmOnRelease = true;

	//일반 시전 조준 태그를 붙였는지
	bool bAddedTargetingTag = false;

	//좌클릭 확정 신호를 구독 중인지
	bool bBoundLocalConfirm = false;

	//사거리 밖을 지정해서 걸어가는 중
	bool bIsWalkingToCast = false;
	FVector WalkTargetLocation = FVector::ZeroVector;
	float WalkStartTime = 0.0f;

	//조준·걸어가기 감시 (이동 요청 번호가 바뀌면 다른 이동 명령이 들어온 것)
	FTimerHandle PendingWatchTimerHandle;
	TWeakObjectPtr<UPathFollowingComponent> WatchedPathFollowing;
	FAIRequestID WatchedMoveRequestId;

	FDelegateHandle OtherAbilityActivatedHandle;
	FDelegateHandle OtherAbilityFailedHandle;

	UPROPERTY(Transient)
	TObjectPtr<AASSkillRangeIndicatorActor> RangeIndicator;

	//AnimNotify 누락 경고용
	bool bWaitingForHitEvent = false;
	bool bHitEventReceived = false;
};
