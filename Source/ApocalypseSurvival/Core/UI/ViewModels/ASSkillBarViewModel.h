#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "ASSkillBarViewModel.generated.h"

class AActor;
class UAbilitySystemComponent;
class UASSkillSlotViewModel;
class UWorld;

//슬롯 하나와 ASC의 스킬을 잇는 내부 연결 정보
struct FASSkillSlotBinding
{
	//이 슬롯이 찾을 입력 태그 (InputTag.Skill.Q 등)
	FGameplayTag InputTag;

	//입력 태그에 맞는 스킬을 찾았는가
	bool bBound = false;

	//스킬의 쿨타임 GE가 붙이는 태그
	FGameplayTagContainer CooldownTags;

	//쿨타임 태그 이벤트 핸들 (해제용)
	TArray<TPair<FGameplayTag, FDelegateHandle>> CooldownTagHandles;

	//쿨타임 진행 중인가
	bool bOnCooldown = false;
};

/** HUD에 고정 배치되는 Q, W, E, R 스킬 슬롯 네 개를 소유한다. */
UCLASS(BlueprintType)
class APOCALYPSESURVIVAL_API UASSkillBarViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** QWER 슬롯 ViewModel을 만들고 각 슬롯의 단축키를 초기화한다. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Bar")
	void InitializeSlots();

	//ASC의 스킬을 QWER 슬롯에 연결하고 아이콘과 쿨타임을 구독
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Bar")
	void Initialize(UAbilitySystemComponent* InAbilitySystemComponent);

	//AbilitySystemInterface를 구현한 액터(Pawn 또는 PlayerState)로 초기화
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Bar")
	void InitializeFromActor(AActor* InAbilitySystemOwner);

	//구독한 태그 이벤트와 타이머를 해제하고 슬롯을 비움
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Bar")
	void Deinitialize();

	//아직 스킬을 못 찾은 슬롯을 다시 찾음
	UFUNCTION(BlueprintCallable, Category = "HUD|Skill Bar")
	void RefreshSkillSlots();

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Bar")
	UASSkillSlotViewModel* GetQSlot() const { return QSlot; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Bar")
	UASSkillSlotViewModel* GetWSlot() const { return WSlot; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Bar")
	UASSkillSlotViewModel* GetESlot() const { return ESlot; }

	UFUNCTION(BlueprintPure, Category = "HUD|Skill Bar")
	UASSkillSlotViewModel* GetRSlot() const { return RSlot; }

private:
	UASSkillSlotViewModel* GetSlotByIndex(int32 SlotIndex) const;

	//입력 태그에 맞는 스펙을 찾아 아이콘과 쿨타임 태그 이벤트를 연결
	void BindSlot(int32 SlotIndex);

	//태그 이벤트를 해제하고 슬롯 표시를 비움
	void UnbindSlot(int32 SlotIndex);

	//쿨타임 태그가 붙거나 떨어졌을 때 호출
	void HandleCooldownTagChanged(const FGameplayTag CooldownTag, int32 NewCount, int32 SlotIndex);

	//쿨타임 중인 슬롯의 남은 비율을 갱신
	void UpdateCooldowns();

	//쿨타임 남은 비율 (1 = 방금 사용, 0 = 끝)
	float GetCooldownRemainingPercent(const FGameplayTagContainer& CooldownTags) const;

	void StartCooldownTimer();
	void StopCooldownTimer();

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Bar", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASSkillSlotViewModel> QSlot = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Bar", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASSkillSlotViewModel> WSlot = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Bar", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASSkillSlotViewModel> ESlot = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Skill Bar", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASSkillSlotViewModel> RSlot = nullptr;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	//ViewModel은 월드가 없어서 ASC의 월드를 빌려 타이머를 돌림
	TWeakObjectPtr<UWorld> CachedWorld;

	//Q, W, E, R 순서
	TArray<FASSkillSlotBinding> SlotBindings;

	FTimerHandle CooldownTimerHandle;
	FTimerHandle RebindTimerHandle;
};
