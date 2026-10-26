#pragma once

#include "CoreMinimal.h"
#include "Mass/Monster/Targeting/ASMonsterTargetTypes.h"
#include "UObject/Interface.h"
#include "ASMonsterTargetable.generated.h"

/** Actor가 기본 Targetable Component 설정 대신 동적 타깃 정보를 제공할 때 구현한다. */
UINTERFACE(BlueprintType)
class APOCALYPSESURVIVAL_API UASMonsterTargetable : public UInterface
{
	GENERATED_BODY()
};

class APOCALYPSESURVIVAL_API IASMonsterTargetable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mass|Monster|Targeting")
	bool IsMonsterTargetable() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mass|Monster|Targeting")
	FVector GetMonsterTargetLocation() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mass|Monster|Targeting")
	float GetMonsterTargetRadius() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mass|Monster|Targeting")
	EASMonsterTargetCapability GetMonsterTargetCapabilities() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mass|Monster|Targeting")
	float GetMonsterTargetBaseScore() const;
};

