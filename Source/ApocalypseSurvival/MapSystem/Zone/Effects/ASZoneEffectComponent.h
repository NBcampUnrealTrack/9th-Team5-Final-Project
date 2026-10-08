#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "MapSystem/Zone/Core/ASMapTypes.h"
#include "ASZoneEffectComponent.generated.h"

class APawn;
class UAbilitySystemComponent;
class UASMapStateComponent;
class UASZoneEffectDefinition;
class UGameplayEffect;
struct FASZoneEffectMagnitude;

/** GameMode에 추가하는 서버 전용 구역 효과 컴포넌트. */
UCLASS(ClassGroup = (AS), meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASZoneEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASZoneEffectComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone Effects")
	TObjectPtr<UASZoneEffectDefinition> ZoneEffectDefinition;

	/** 이 컴포넌트가 Actor의 ASC에 적용한 위험도. 효과가 없으면 0이다. */
	UFUNCTION(BlueprintPure, Category = "AS|Map|Zone Effects")
	int32 GetAppliedDangerLevel(const AActor* Actor) const;

	/** 활성 구역 추가 효과의 ZoneId. 효과가 없으면 NAME_None이다. */
	UFUNCTION(BlueprintPure, Category = "AS|Map|Zone Effects")
	FName GetAppliedZoneEffectZone(const AActor* Actor) const;

	/** 서버/호스트 콘솔에서 추적 중인 효과를 출력한다. */
	void DumpState() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 각 효과의 대상과 핸들을 독립적으로 보관한다. */
	struct FEffectSlot
	{
		// 선택된 행의 키. 기본 효과는 ZoneId가 NAME_None이다.
		FName ZoneId;
		uint8 TeamId = 255;
		int32 DangerLevel = 0;
		TSubclassOf<UGameplayEffect> Effect;
		FActiveGameplayEffectHandle Handle;
	};

	struct FEntry
	{
		TWeakObjectPtr<APawn> Pawn;
		FEffectSlot DangerEffect;
		FEffectSlot ZoneEffect;
	};

	void UpdateEffects();
	void BindMapState(UASMapStateComponent* NewMapState);
	void WarnUnknownZoneRows();
	FActiveGameplayEffectHandle ApplyEffect(UAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> Effect,
		const TArray<FASZoneEffectMagnitude>& Magnitudes, int32 DangerLevel);
	void RemoveSlot(UAbilitySystemComponent* ASC, FEffectSlot& Slot);
	void RemoveEntry(UAbilitySystemComponent* ASC);
	void RemoveAllEffects();

	UFUNCTION()
	void HandleZoneStateChanged(FName ZoneId, const FASZoneRuntimeState& State);

	UFUNCTION()
	void HandleMapStateUpdated();

	UFUNCTION()
	void HandleMatchReset();

	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FEntry> Entries;
	TWeakObjectPtr<UASMapStateComponent> MapState;
	/** 런타임 편집으로 잘못된 GE가 들어와도 클래스별 경고는 한 번만 출력한다. */
	TSet<TWeakObjectPtr<UClass>> WarnedNonInfiniteEffects;
	FTimerHandle UpdateTimer;
	bool bRunning = false;
	bool bUpdating = false;
	bool bCheckedZoneRows = false;
};
