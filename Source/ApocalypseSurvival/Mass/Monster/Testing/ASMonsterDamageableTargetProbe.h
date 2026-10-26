#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mass/Monster/Combat/ASMonsterDamageable.h"
#include "ASMonsterDamageableTargetProbe.generated.h"

class UASMonsterTargetableComponent;
class UStaticMeshComponent;

/** 비-GAS 파괴 가능 타깃과 BlockingTarget 동작을 검증하기 위한 테스트 Actor 기반 클래스다. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASMonsterDamageableTargetProbe : public AActor, public IASMonsterDamageable
{
	GENERATED_BODY()

public:
	AASMonsterDamageableTargetProbe();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanReceiveMonsterDamage_Implementation() const override;
	virtual bool IsAliveForMonsterTargeting_Implementation() const override;
	virtual void ApplyMonsterDamage_Implementation(const FASMonsterDamageContext& Context) override;

	UFUNCTION(BlueprintPure, Category = "Mass|Monster|Testing")
	float GetRemainingHealth() const { return CurrentHealth; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_DestroyedByMonster();

	void ApplyDestroyedState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass|Monster|Testing")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass|Monster|Testing")
	TObjectPtr<UASMonsterTargetableComponent> MonsterTargetableComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mass|Monster|Testing", meta = (ClampMin = "1.0"))
	float MaxHealth = 200.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Mass|Monster|Testing")
	float CurrentHealth = 200.0f;

	UPROPERTY(ReplicatedUsing = OnRep_DestroyedByMonster, VisibleInstanceOnly, BlueprintReadOnly, Category = "Mass|Monster|Testing")
	bool bDestroyedByMonster = false;
};
