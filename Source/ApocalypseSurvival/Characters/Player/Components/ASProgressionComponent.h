#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Characters/Player/Data/ASProgressionData.h"
#include "ASProgressionComponent.generated.h"

class UAbilitySystemComponent;
class UASCharacterDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASOnLevelChanged, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FASOnExpChanged, int32, CurrentExp, int32, ExpToNextLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FASOnSkillUpgradeUnlocked, int32, TierIndex, FGameplayTag, SkillSlotTag, int32, ChoiceCount);

/*
 * 레벨, 경험치, 스킬 강화 단계. PlayerState에 붙어서 리스폰해도 유지된다.
 * 값은 서버만 바꾼다. 클라이언트는 복제로 받고 이벤트로 UI를 갱신한다.
 * 스킬 강화의 선택지 내용, 선택 기록, 효과 적용은 스킬 담당 (OnSkillUpgradeUnlocked 를 받아서 처리)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASProgressionComponent();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	/* 서버 전용. 경험치를 더하고, 넘치면 레벨업을 반복한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Progression")
	void AddExperience(int32 Amount);
	
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetLevel() const { return Level; }
	
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetCurrentExp() const { return CurrentExp; }
	
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetExpToNextLevel() const;
	
	UFUNCTION(BlueprintPure, Category = "Progression")
	bool IsMaxLevel() const;
	
	// 지금까지 열린 강화 단계 수 (0 ~ 개수-1 번 단계가 열린 것)
	UFUNCTION(BlueprintPure, Category = "Progression|SkillUpgrade")
	int32 GetUnlockedSkillUpgradeTierCount() const { return UnlockedSkillUpgradeTierCount; }
	
	// 단계 정보 조회 (번호가 잘못되면 false)
	UFUNCTION(BlueprintPure, Category = "Progression|SkillUpgrade")
	bool GetSkillUpgradeTier(int32 TierIndex, FASSkillUpgradeTier& OutTier) const;
	
	// UI용 이벤트 (서버는 값 변경 시, 클라이언트는 복제 도착 시 호출)
	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FASOnLevelChanged OnLevelChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FASOnExpChanged OnExpChanged;
	
	// 강화 단계가 열릴 때 단계마다 한 번씩 호출 (스킬 담당이 구독)
	UPROPERTY(BlueprintAssignable, Category = "Progression|SkillUpgrade")
	FASOnSkillUpgradeUnlocked OnSkillUpgradeUnlocked;

protected:
	void LevelUp();
	
	// 캐릭터 정의의 레벨당 증가량만큼 기본값을 올린다
	void ApplyLevelGrowth(const UASCharacterDefinition* Definition) const;
	
	// [FromCount, ToCount) 번 단계를 차례로 알린다
	void BroadcastSkillUpgradeUnlocked(int32 FromCount, int32 ToCount);
	
	const UASCharacterDefinition* GetCharacterDefinition() const;
	const UASProgressionData* GetProgressionData() const;
	UAbilitySystemComponent* GetAbilitySystemComponent() const;
	
	UFUNCTION()
	void OnRep_Level();
	
	UFUNCTION()
	void OnRep_CurrentExp();
	
	UFUNCTION()
	void OnRep_UnlockedSkillUpgradeTierCount(int32 OldCount);
	
	UPROPERTY(ReplicatedUsing = OnRep_Level)
	int32 Level = 1;
	
	// 현재 레벨에서 모은 경험치 (렙업하면 필요량만큼 빠진다)
	UPROPERTY(ReplicatedUsing = OnRep_CurrentExp)
	int32 CurrentExp = 0;
	
	UPROPERTY(ReplicatedUsing = OnRep_UnlockedSkillUpgradeTierCount)
	int32 UnlockedSkillUpgradeTierCount = 0;
};