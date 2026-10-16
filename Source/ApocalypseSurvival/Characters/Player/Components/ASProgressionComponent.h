#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASProgressionComponent.generated.h"

class UAbilitySystemComponent;
class UASCharacterDefinition;
class UASProgressionData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASOnLevelChanged, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FASOnExpChanged, int32, CurrentExp, int32, ExpToNextLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FASOnSkillPointsChanged, int32, SkillPoints);

/*
 * 레벨, 경험치, 스킬포인트. PlayerState에 붙어서 리스폰해도 유지된다.
 * 값은 서버만 바꾼다. 클라이언트는 복제로 받고 이벤트로 UI를 갱신한다.
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
	
	/* 서버 전용. 스킬포인트를 쓴다. 모자라면 false. (스킬 강화 쪽에서 호출) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Progression")
	bool SpendSkillPoints(int32 Amount);
	
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetLevel() const { return Level; }
	
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetCurrentExp() const { return CurrentExp; }
	
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetExpToNextLevel() const;
	
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetSkillPoints() const { return SkillPoints; }
	
	UFUNCTION(BlueprintPure, Category = "Progression")
	bool IsMaxLevel() const;
	
	// UI용 이벤트 (서버는 값 변경 시, 클라이언트는 복제 도착 시 호출)
	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FASOnLevelChanged OnLevelChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FASOnExpChanged OnExpChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FASOnSkillPointsChanged OnSkillPointsChanged;

protected:
	void LevelUp();
	
	// 캐릭터 정의의 레벨당 증가량만큼 기본값을 올린다
	void ApplyLevelGrowth(const UASCharacterDefinition* Definition) const;
	
	const UASCharacterDefinition* GetCharacterDefinition() const;
	const UASProgressionData* GetProgressionData() const;
	UAbilitySystemComponent* GetAbilitySystemComponent() const;
	
	UFUNCTION()
	void OnRep_Level();
	
	UFUNCTION()
	void OnRep_CurrentExp();
	
	UFUNCTION()
	void OnRep_SkillPoints();
	
	UPROPERTY(ReplicatedUsing = OnRep_Level)
	int32 Level = 1;
	
	// 현재 레벨에서 모은 경험치 (렙업하면 필요량만큼 빠진다)
	UPROPERTY(ReplicatedUsing = OnRep_CurrentExp)
	int32 CurrentExp = 0;
	
	UPROPERTY(ReplicatedUsing = OnRep_SkillPoints)
	int32 SkillPoints = 0;
};
