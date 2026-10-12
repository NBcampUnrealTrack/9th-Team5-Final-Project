#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ASMonsterTargetingComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FASMonsterTargetChanged, AActor*, PreviousTarget, AActor*, NewTarget);

/**
 * 몬스터의 현재 전투 타깃을 보관하고, 가장 가까운 살아 있는 플레이어를 찾아 준다.
 * 언제 찾고 언제 놓을지는 StateTree 가 정하고, 이 컴포넌트는 서버에서 그 요청을 실행한다.
 *
 * 지금은 플레이어만 타깃이 된다. 몬스터와 적대 관계인 팀의 플레이어 조종 폰이어야 한다.
 */
UCLASS(ClassGroup = (Enemy), BlueprintType, meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASMonsterTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASMonsterTargetingComponent();

	/** 탐지·추적 해제 반경을 정한다. 해제 반경은 탐지 반경보다 작아지지 않게 보정된다. */
	void Configure(float InDetectionRadius, float InLoseTargetRadius, float InAggroDecayPerSecond = 25.0f);

	UFUNCTION(BlueprintPure, Category = "Enemy|Targeting")
	float GetDetectionRadius() const { return DetectionRadius; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Targeting")
	float GetLoseTargetRadius() const { return LoseTargetRadius; }

	/** 서버 전용. MaxDistance 안에서 가장 가까운 살아 있는 플레이어 폰. 없거나 서버가 아니면 nullptr. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Targeting")
	APawn* FindNearestLivingPlayer(float MaxDistance) const;

	/**
	 * 서버 전용. 유효한 타깃이 이미 있으면 그대로 두고(바꾸지 않는다),
	 * 없으면 탐지 반경 안에서 새로 찾는다. 결과적으로 타깃이 있으면 true.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Targeting")
	bool AcquireTarget();

	/** 서버 전용. 거리 제한 없이 가장 가까운 살아 있는 플레이어를 찾는다. 밤 추격에 사용한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Targeting")
	bool AcquireTargetAnywhere();

	/**
	 * 서버 전용. 현재 타깃이 죽었거나 유효하지 않으면 비운다.
	 * 타깃이 계속 유효하면 true.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Targeting")
	bool ValidateTarget(bool bIgnoreLoseTargetRadius = false);

	/** 서버 전용. 유효한 플레이어 공격자에게 지정된 수치만큼 어그로를 부여한다. */
	bool ForceAggro(AActor* Aggressor, float AggroAmount);

	/** 서버 전용. 타깃이 추적 해제 거리 밖에 있으면 어그로를 감소시키고 0일 때 타깃을 비운다. */
	void UpdateAggro(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Enemy|Targeting")
	float GetCurrentAggro() const { return CurrentAggro; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Targeting")
	AActor* GetTargetActor() const { return TargetActor; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Targeting")
	bool HasValidTarget() const;

	/** 서버 전용. StateTree 또는 AI Controller가 새 타깃을 찾은 뒤 호출한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Targeting")
	void SetTargetActor(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enemy|Targeting")
	void ClearTarget();

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Targeting")
	FASMonsterTargetChanged OnTargetChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** 플레이어가 조종하고, 이 몬스터와 적대 팀이며, 체력이 남은 폰인지. */
	bool IsLivingPlayerTarget(const APawn* Pawn) const;

	UFUNCTION()
	void OnRep_TargetActor(AActor* PreviousTarget);

	UPROPERTY(EditDefaultsOnly, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float DetectionRadius = 1500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float LoseTargetRadius = 2200.0f;

	UPROPERTY(ReplicatedUsing = OnRep_TargetActor)
	TObjectPtr<AActor> TargetActor;

	float CurrentAggro = 0.0f;
	float AggroDecayPerSecond = 25.0f;
	static constexpr float MaxAggro = 100.0f;
};
