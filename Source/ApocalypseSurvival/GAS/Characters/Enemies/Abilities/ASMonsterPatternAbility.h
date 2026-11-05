#pragma once

#include "CoreMinimal.h"
#include "Skills/ASSkillAbility.h"
#include "ASMonsterPatternAbility.generated.h"

struct FGameplayCueParameters;
class UAnimMontage;

/** 몬스터 패턴 단계에서 재생할 Montage와 시작 Section을 Blueprint가 지정하는 공통 설정이다. */
USTRUCT(BlueprintType)
struct FASMonsterPresentationMontage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FName StartSection = NAME_None;

	/** Montage 전체 재생 배율. Segment별 배율은 Montage 자산에서 설정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;
};

/**
 * 엘리트·보스의 선딜 패턴 전용 Ability 기반 클래스.
 * 공용 UASSkillAbility의 동작은 건드리지 않고, 패턴의 선딜/실행/정리 생명주기만 별도로 제공한다.
 */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterPatternAbility : public UASSkillAbility
{
	GENERATED_BODY()

public:
	UASMonsterPatternAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicatedEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Monster|Pattern", meta = (ClampMin = "0.0", Units = "s"))
	float WindupDuration = 0.0f;

	/** 선딜 시작 시 인디케이터 생성·방향 고정 등을 처리한다. */
	virtual void BeginPatternWindup();

	/** true면 실행 직후 Ability를 종료한다. 돌진처럼 지속 실행이면 false를 반환한다. */
	virtual bool ExecutePatternEffect();

	/** 취소·사망을 포함해 패턴이 끝날 때 인디케이터 등을 정리한다. */
	virtual void EndPattern(bool bWasCancelled);

	/** 패턴별 선딜 연출 훅. 필요한 상속 클래스만 재정의한다. */
	virtual void StartWindupPresentation();

	/** 패턴별 실행 연출 훅. 필요한 상속 클래스만 재정의한다. */
	virtual void StartActionPresentation();

	/** 패턴별 후딜 연출 훅. 필요한 상속 클래스만 재정의한다. */
	virtual void StartRecoveryPresentation();

	/** 상속받은 AnimMontage를 서버 ASC로 재생해 원격 클라이언트에 복제한다. */
	bool PlayPresentationMontage(FName StartSectionName = NAME_None);

	/** 명시한 단계별 Montage를 서버 ASC로 재생한다. Blend 설정은 Montage 자산 값을 사용한다. */
	bool PlayPresentationMontage(const FASMonsterPresentationMontage& PresentationMontage);

	/** 명시한 Montage를 서버 ASC로 재생한다. */
	bool PlayPresentationMontage(UAnimMontage* Montage, float PlayRate = 1.0f,
		FName StartSectionName = NAME_None);

	/** 이 Monster Ability가 재생 중인 Presentation Montage의 Section을 전환한다. */
	bool JumpPresentationMontageSection(FName SectionName);

	/** 이 Monster Ability가 시작한 Presentation Montage만 중지한다. */
	void StopPresentationMontage(float OverrideBlendOutTime = -1.0f);

	/** 서버 ASC를 통해 일회성 GameplayCue를 실행한다. */
	void ExecutePresentationCue(const FGameplayTag& CueTag, const FGameplayCueParameters& CueParameters);

	/** 서버 ASC에 지속 GameplayCue를 추가하고 종료 시 정리할 수 있도록 추적한다. */
	void AddPresentationCue(const FGameplayTag& CueTag, const FGameplayCueParameters& CueParameters);

	/** 이 Monster Ability가 추가한 지속 GameplayCue 하나를 제거한다. */
	void RemovePresentationCue(const FGameplayTag& CueTag);

	/** 이 Monster Ability가 시작한 Montage와 지속 Cue를 모두 정리한다. */
	void CleanupPresentation();

	void FinishPattern(bool bWasCancelled);

private:
	void BeginPatternEffect();
	void InitializePresentationCueParameters(FGameplayCueParameters& CueParameters) const;

	FTimerHandle WindupTimerHandle;
	TSet<FGameplayTag> ActivePresentationCueTags;
	TWeakObjectPtr<UAnimMontage> ActivePresentationMontage;
	bool bPresentationMontageStarted = false;
};
