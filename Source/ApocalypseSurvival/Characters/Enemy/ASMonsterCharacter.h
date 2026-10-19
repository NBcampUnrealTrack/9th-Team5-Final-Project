#pragma once

#include "CoreMinimal.h"
#include "Characters/ASCharacterBase.h"
#include "ASMonsterCharacter.generated.h"

class UASAbilitySystemComponent;
class AASPlayerCameraManager;
class UASCombatAttributeSet;
class UASHealthAttributeSet;
class UASMonsterCombatComponent;
class UASMonsterDefinition;
class UASMonsterHealthBarWidget;
class UASMonsterTargetingComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UWidgetComponent;
struct FOnAttributeChangeData;
struct FGameplayEffectSpec;
struct FActiveGameplayEffectHandle;
struct FGameplayTag;
class UAbilitySystemComponent;

/** 스폰 출처에 따른 낮 시간 탐지 정책. AI 판단은 서버에서만 사용한다. */
UENUM()
enum class EASMonsterDayBehavior : uint8
{
	NightOnly,
	ProximityAggro
};

/**
 * 일반 몬스터 공통 Character.
 * 공격 종류는 자식 클래스가 아니라 MonsterDefinition, StateTree, GAS Ability 조합으로 결정한다.
 *
 * 두뇌(StateTree)는 이 클래스가 아니라 AASMonsterAIController 가 소유한다.
 * 이 클래스는 Definition 적용과 GAS 초기화를 끝낸 뒤 OnMonsterInitialized 로 알리기만 한다.
 */
UCLASS(Abstract)
class APOCALYPSESURVIVAL_API AASMonsterCharacter : public AASCharacterBase
{
	GENERATED_BODY()

public:
	AASMonsterCharacter();

	/**
	 * 몬스터는 Actor 자신이 ASC를 소유한다. AttributeSet의 초기 복제가 BeginPlay보다 먼저
	 * 도착할 수 있으므로, 베이스 클래스가 InitAbilitySystem에서 캐시한 참조를 기다리지 않는다.
	 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 서버에서 Definition 적용과 GAS 초기화가 끝난 직후 한 번 불린다. AI 컨트롤러가 이걸 기다린다. */
	FSimpleMulticastDelegate OnMonsterInitialized;

	/** 서버 전용. 클라이언트에서는 항상 false 다. */
	bool IsMonsterInitialized() const { return bMonsterInitialized; }

	/** State.Monster.Dead 태그로 판정한다. 태그가 복제되므로 모든 머신에서 맞다. */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDead() const;

	/** State.Monster.HitReact 태그로 판정한다. 경직 중에는 AI 이동과 공격을 멈춘다. */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsHitReacting() const;

	/** SpawnSubsystem이 FinishSpawning 전에 설정하는 낮 시간 행동 정책. */
	void SetDayBehavior(EASMonsterDayBehavior InDayBehavior) { DayBehavior = InDayBehavior; }

	bool CanAggroDuringDay() const { return DayBehavior == EASMonsterDayBehavior::ProximityAggro; }

	/** SpawnSubsystem이 필드 몬스터의 원래 스폰 위치와 Leash 설정을 기록한다. */
	void ConfigureSpawnHome(const FVector& InLocation, float InLeashRadius, float InReturnAcceptanceRadius,
		float InReturnHealthPercentPerSecond, float InDayDetectionRadius, float InDayLoseTargetRadius,
		float InReturnMoveSpeedMultiplier);

	bool HasSpawnHome() const { return bHasSpawnHome; }
	bool IsOutsideSpawnLeash() const;
	const FVector& GetSpawnHomeLocation() const { return SpawnHomeLocation; }
	float GetReturnAcceptanceRadius() const { return ReturnAcceptanceRadius; }
	float GetReturnHealthPercentPerSecond() const { return ReturnHealthPercentPerSecond; }
	float GetDayDetectionRadius() const { return DayDetectionRadius; }
	float GetDayLoseTargetRadius() const { return DayLoseTargetRadius; }
	float GetReturnMoveSpeedMultiplier() const { return ReturnMoveSpeedMultiplier; }

	/**
	 * 서버 전용. true 면 몸이 이동 방향이 아니라 컨트롤러 시선(AIController 의 Focus)을 따라 돈다.
	 * 원거리 몬스터가 타깃을 보면서 움직일 때 쓴다.
	 */
	void SetFaceControllerFocus(bool bFaceControllerFocus);

	/** 서버 전용. 피해를 준 플레이어에게 즉시 어그로를 건다. */
	void NotifyDamagedBy(AActor* InstigatorActor);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UASMonsterDefinition* GetMonsterDefinition() const { return MonsterDefinition; }

	UFUNCTION(BlueprintPure, Category = "Enemy|GAS")
	UASAbilitySystemComponent* GetASAbilitySystemComponent() const { return OwnedAbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Components")
	UASMonsterTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Components")
	UASMonsterCombatComponent* GetCombatComponent() const { return CombatComponent; }

	/**
	 * 월드 체력바 기능 자체를 켜거나 끈다.
	 * 끄면 Widget과 WidgetComponent를 제거하므로 숨겨진 UI의 유지 비용도 남기지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|UI")
	void SetHealthBarEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Enemy|UI")
	bool IsHealthBarEnabled() const { return bHealthBarEnabled; }

	/**
	 * 활성화된 체력바의 런타임 표시 여부를 바꾼다.
	 * 숨길 때는 렌더링, Component Tick, GAS Attribute 구독을 모두 중지한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|UI")
	void SetHealthBarVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Enemy|UI")
	bool IsHealthBarVisible() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 자기 체력 변화를 구독한다. 베이스가 ASC 연결을 마친 뒤 모든 머신에서 불린다. */
	virtual void OnAbilitySystemInitialized() override;

	/** Definition 값을 Health/Combat AttributeSet과 컴포넌트에 적용하고 기본 Ability를 부여한다. */
	void ApplyMonsterDefinition();

	/**
	 * 서버 전용. 체력이 0 이하가 되면 한 번만 불린다.
	 * 죽음 태그만 붙이고 이동과 AI 정지는 StopMovementAndAI 로 넘긴다.
	 * 애니메이션, 충돌 해제, 소멸, 보상은 아직 없다.
	 */
	virtual void HandleDeath();

	/**
	 * 죽음 처리에서 이동과 AI 를 멈추는 진입점. 자식에서 늘릴 수 있다.
	 *
	 * 죽음은 StateTree 가 아니라 Character/GAS 생명주기가 처리한다. 체력이 0 이 되면 여기서
	 * AIController 의 StopLogic 으로 트리를 끝낸다. 그러므로 초기 StateTree 에는 Dead 상태를
	 * 만들지 않는다. 만들어도 트리가 먼저 멈추기 때문에 실행되지 않는다.
	 */
	virtual void StopMovementAndAI();

	/** 서버 전용. Definition의 HitReactEffect를 자신에게 적용하고 진행 중인 공격을 취소한다. */
	void ApplyHitReact();

	/** 사망을 GameMode에 보고한다. 기본 몬스터는 처치 수를 올리고, 엘리트 BP는 보스 처치로 오버라이드한다. */
	UFUNCTION(BlueprintNativeEvent, Category = "Enemy|Death")
	void ReportDeathToGameMode();
	virtual void ReportDeathToGameMode_Implementation();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UASMonsterDefinition> MonsterDefinition;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASAbilitySystemComponent> OwnedAbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASHealthAttributeSet> OwnedHealthAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASCombatAttributeSet> OwnedCombatAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASMonsterTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASMonsterCombatComponent> CombatComponent;

private:
	void ApplyHealthBarState();
	void CreateHealthBarIfNeeded();
	void ReleaseHealthBar();
	void BindHealthBarToCameraZoom();
	void UnbindHealthBarFromCameraZoom();
	void HandleCameraZoomChanged(float ZoomAlpha);
	void UnbindTargetedAttackTagDelegate();
	void HandleTargetedAttackTagChanged(const FGameplayTag Tag, int32 NewCount);
	void SetTargetedAttackMaterialTint(bool bActive);
	/** 인접 몬스터의 피격 알림. 전파가 다시 전파되지 않도록 자기 자신만 어그로 처리한다. */
	void NotifyAllyDamagedBy(AActor* InstigatorActor, float AggroAmount);
	void UnbindHealthDelegate();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void UnbindDamageEffectDelegate();
	void HandleGameplayEffectApplied(UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FGameplayEffectSpec& AppliedEffect, FActiveGameplayEffectHandle ActiveEffectHandle);

	bool bMonsterInitialized = false;
	/** Subsystem 밖에서 생성한 기존 몬스터의 낮 탐지 동작은 유지한다. 밤 웨이브만 NightOnly로 명시 설정한다. */
	EASMonsterDayBehavior DayBehavior = EASMonsterDayBehavior::ProximityAggro;
	bool bHasSpawnHome = false;
	FVector SpawnHomeLocation = FVector::ZeroVector;
	float SpawnLeashRadius = 0.0f;
	float ReturnAcceptanceRadius = 100.0f;
	float ReturnHealthPercentPerSecond = 0.0f;
	float DayDetectionRadius = 0.0f;
	float DayLoseTargetRadius = 0.0f;
	float ReturnMoveSpeedMultiplier = 1.0f;
	bool bHealthBarEnabled = false;
	bool bHealthBarVisibleRequested = false;

	/** bHealthBarEnabled && bHealthBarVisibleRequested일 때만 지연 생성한다. */
	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> HealthBarComponent;

	UPROPERTY(Transient)
	TObjectPtr<UASMonsterHealthBarWidget> HealthBarWidget;

	/** M_Mannequin 계열이 이미 제공하는 Vector Parameter. 다른 메시라면 BP에서 이름만 바꾼다. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Visual|Targeted Attack")
	FName TargetedAttackTintParameter = FName(TEXT("Paint Tint"));

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Visual|Targeted Attack")
	FLinearColor TargetedAttackTintColor = FLinearColor(1.0f, 0.03f, 0.03f, 1.0f);

	bool bTargetedAttackTintActive = false;

	/** 색상 전환 전에 메시가 사용하던 원본 Material을 슬롯별로 보관한다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> TargetedAttackOriginalMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TargetedAttackMaterialInstances;

	TWeakObjectPtr<AASPlayerCameraManager> BoundCameraManager;
	FDelegateHandle CameraZoomChangedHandle;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle TargetedAttackTagChangedHandle;
	FDelegateHandle DamageEffectAppliedHandle;
};
