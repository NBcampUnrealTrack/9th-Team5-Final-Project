#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ASMonsterDamageable.generated.h"

class AActor;

/** GAS를 사용하지 않는 공격 가능 Actor에게 전달하는 Mass 몬스터 피해 정보다. */
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASMonsterDamageContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster|Damage")
	float Damage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster|Damage")
	FVector DamageOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster|Damage")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Mass|Monster|Damage")
	TObjectPtr<AActor> DamageCauser = nullptr;
};

/** GAS 없이도 Mass 몬스터의 공격 대상이 될 수 있는 Actor 계약이다. */
UINTERFACE(BlueprintType)
class APOCALYPSESURVIVAL_API UASMonsterDamageable : public UInterface
{
	GENERATED_BODY()
};

class APOCALYPSESURVIVAL_API IASMonsterDamageable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mass|Monster|Damage")
	bool CanReceiveMonsterDamage() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mass|Monster|Damage")
	bool IsAliveForMonsterTargeting() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mass|Monster|Damage")
	void ApplyMonsterDamage(const FASMonsterDamageContext& Context);
};
