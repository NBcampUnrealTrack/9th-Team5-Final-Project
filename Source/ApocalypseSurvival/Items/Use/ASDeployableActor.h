#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "Mass/Monster/Targeting/ASMonsterTargetable.h"
#include "ASDeployableActor.generated.h"

class UAbilitySystemComponent;
class UASHealthAttributeSet;
class UASMonsterTargetableComponent;
struct FOnAttributeChangeData;

/**
 * 인벤토리 아이템이 월드에 배치한 Actor의 공통 기반 클래스다.
 * 배치 수명 주기, 체력/GAS, 복제와 Mass 몬스터 타깃 등록을 담당한다.
 * 구체적인 충돌 형태, 배치 가능 위치 검사와 표현은 자식 클래스가 담당한다.
 */
UCLASS(Abstract, Blueprintable)
class APOCALYPSESURVIVAL_API AASDeployableActor : public AActor,
	public IAbilitySystemInterface,
	public IASMonsterTargetable
{
	GENERATED_BODY()

public:
	AASDeployableActor();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버에서 체력과 선택적 수명을 설정한다. FinishPlacement 전 한 번만 성공한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item|Deployable")
	bool PreparePlacement(float MaxHealth, float Lifetime);

	/** 준비된 설치물을 활성화하고 배치 상태를 복제한다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item|Deployable")
	void FinishPlacement();

	UFUNCTION(BlueprintPure, Category = "Item|Deployable")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Item|Deployable")
	bool IsIntact() const;

	UFUNCTION(BlueprintPure, Category = "Item|Deployable")
	bool IsPlaced() const { return bPlaced; }

	virtual bool IsMonsterTargetable_Implementation() const override;
	virtual FVector GetMonsterTargetLocation_Implementation() const override;
	virtual float GetMonsterTargetRadius_Implementation() const override;
	virtual EASMonsterTargetCapability GetMonsterTargetCapabilities_Implementation() const override;
	virtual float GetMonsterTargetBaseScore_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 생성자에서 설치물 종류의 기본 타깃 프로필을 지정한다. */
	void SetDefaultMonsterTargetProfile(EASMonsterTargetCapability InCapabilities, float InBaseScore);

	/** 배치, 복제, 파괴 시 자식 클래스가 충돌과 표현 상태를 갱신한다. */
	virtual void ApplyPlacementState(bool bIsActive);

	/** 자식 클래스의 실제 수평 충돌 크기를 몬스터 공격 반경으로 제공한다. */
	virtual float CalculateMonsterTargetRadius() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UASHealthAttributeSet> HealthAttributes;

	/** 서버의 범용 몬스터 Target Registry에 이 설치물을 등록한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UASMonsterTargetableComponent> MonsterTargetableComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Deployable|Targeting",
		meta = (Bitmask, BitmaskEnum = "/Script/ApocalypseSurvival.EASMonsterTargetCapability"))
	int32 MonsterTargetCapabilityMask = static_cast<int32>(EASMonsterTargetCapability::None);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Deployable|Targeting")
	float MonsterTargetBaseScore = 0.0f;

private:
	UFUNCTION()
	void OnRep_Placed();

	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	UPROPERTY(ReplicatedUsing = OnRep_Placed)
	bool bPlaced = false;

	bool bPrepared = false;
	float PlacedLifetime = 0.0f;
};
