#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Mass/Monster/Targeting/ASMonsterTargetTypes.h"
#include "ASMonsterTargetableComponent.generated.h"

class UASMonsterTargetSubsystem;

/**
 * Actor를 서버의 범용 몬스터 Target Registry에 등록한다.
 * Owner가 IASMonsterTargetable을 구현하면 동적 값이 이 컴포넌트의 기본값보다 우선한다.
 */
UCLASS(ClassGroup = (Mass), BlueprintType, meta = (BlueprintSpawnableComponent))
class APOCALYPSESURVIVAL_API UASMonsterTargetableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UASMonsterTargetableComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Mass|Monster|Targeting")
	FASTargetHandle GetTargetHandle() const { return TargetHandle; }

	UFUNCTION(BlueprintPure, Category = "Mass|Monster|Targeting")
	bool IsRegistered() const { return TargetHandle.IsValid(); }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Mass|Monster|Targeting")
	void SetTargetingEnabled(bool bInTargetingEnabled);

	void ConfigureTargetProfile(EASMonsterTargetCapability InCapabilities, float InBaseScore, float InTargetRadius);

	bool IsTargetable() const;
	FVector GetTargetLocation() const;
	float GetTargetRadius() const;
	EASMonsterTargetCapability GetTargetCapabilities() const;
	float GetTargetBaseScore() const;

private:
	void RegisterWithTargetSubsystem();
	void UnregisterFromTargetSubsystem();
	UASMonsterTargetSubsystem* GetTargetSubsystem() const;

	UPROPERTY(EditAnywhere, Category = "Mass|Monster|Targeting")
	bool bTargetingEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Mass|Monster|Targeting", meta = (Bitmask, BitmaskEnum = "/Script/ApocalypseSurvival.EASMonsterTargetCapability"))
	int32 TargetCapabilityMask = static_cast<int32>(EASMonsterTargetCapability::PrimaryTarget);

	UPROPERTY(EditAnywhere, Category = "Mass|Monster|Targeting")
	float BaseTargetScore = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Mass|Monster|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float TargetRadius = 0.0f;

	FASTargetHandle TargetHandle;
};

