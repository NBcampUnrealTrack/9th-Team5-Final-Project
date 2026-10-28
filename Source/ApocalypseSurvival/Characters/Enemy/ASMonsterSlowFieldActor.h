#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/Actor.h"
#include "ASMonsterSlowFieldActor.generated.h"

class UAbilitySystemComponent;
class UPrimitiveComponent;
class USceneComponent;
class USphereComponent;

/**
 * 고정형 둔화 장판. Actor별 진입/이탈 Handle을 직접 관리한다.
 * 서로 겹친 장판은 대상 하나에 50% 둔화를 한 번만 적용하고, 마지막 장판에서 나갈 때만 해제한다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASMonsterSlowFieldActor : public AActor
{
	GENERATED_BODY()

public:
	AASMonsterSlowFieldActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Deferred Spawn의 FinishSpawning 전에 서버가 호출한다. */
	void InitializeField(const FGameplayEffectSpecHandle& InSlowEffectSpec, AActor* InInstigatorAvatar,
		float InRadius, float InDuration);

	UFUNCTION(BlueprintPure, Category = "Enemy|SlowField")
	float GetRadius() const { return Radius; }

	UFUNCTION(BlueprintPure, Category = "Enemy|SlowField")
	float GetDuration() const { return Duration; }

	UFUNCTION(BlueprintPure, Category = "Enemy|SlowField")
	float GetProgress() const;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|SlowField")
	void BP_OnSlowFieldInitialized();

private:
	struct FSharedSlowState
	{
		FActiveGameplayEffectHandle Handle;
		int32 ReferenceCount = 0;
	};

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	void ConfigureCollision();
	void ApplySlowToActor(AActor* Actor);
	void RemoveSlowFromActor(AActor* Actor);
	void ReleaseAllAffectedTargets();
	bool IsEligibleTarget(const AActor* Actor) const;
	float GetSynchronizedWorldTime() const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> TriggerSphere;

	UPROPERTY(ReplicatedUsing = OnRep_FieldConfig)
	float Radius = 280.0f;

	UPROPERTY(ReplicatedUsing = OnRep_FieldConfig)
	float Duration = 5.0f;

	UPROPERTY(ReplicatedUsing = OnRep_FieldConfig)
	float StartServerTime = 0.0f;

	FGameplayEffectSpecHandle SlowEffectSpec;
	TWeakObjectPtr<AActor> InstigatorAvatar;
	TSet<TWeakObjectPtr<UAbilitySystemComponent>> AffectedAbilitySystems;

	static TMap<TWeakObjectPtr<UAbilitySystemComponent>, FSharedSlowState> SharedSlowEffects;

	UFUNCTION()
	void OnRep_FieldConfig();
};
