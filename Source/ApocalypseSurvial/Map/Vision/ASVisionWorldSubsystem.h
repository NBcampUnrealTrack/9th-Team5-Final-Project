#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASVisionWorldSubsystem.generated.h"

class AASBushArea;
class AASFogOfWarManager;
class UASVisibilityComponent;
class UASVisionSourceComponent;

/** 월드별 시야 레지스트리. 모든 머신에서 공간 조회와 로컬 안개 조회가 동작한다. */
UCLASS()
class APOCALYPSESURVIAL_API UASVisionWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UASVisionWorldSubsystem* Get(const UObject* WorldContextObject);

	// 수풀
	void RegisterBush(AASBushArea* Bush);
	void UnregisterBush(AASBushArea* Bush);
	TArray<AASBushArea*> GetBushes() const;

	/** 해당 위치의 수풀 ID. 없으면 None. */
	FName FindBushIdAtLocation(const FVector& Location) const;

	// 시야
	void RegisterVisionSource(UASVisionSourceComponent* Source);
	void UnregisterVisionSource(UASVisionSourceComponent* Source);
	TArray<UASVisionSourceComponent*> GetVisionSources() const;

	void RegisterVisibilityTarget(UASVisibilityComponent* Target);
	void UnregisterVisibilityTarget(UASVisibilityComponent* Target);
	TArray<UASVisibilityComponent*> GetVisibilityTargets() const;

	void SetFogOfWarManager(AASFogOfWarManager* Manager);

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	AASFogOfWarManager* GetFogOfWarManager() const { return FogOfWarManager.Get(); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	template <typename T>
	static TArray<T*> ResolveWeak(const TArray<TWeakObjectPtr<T>>& Items)
	{
		TArray<T*> Result;
		Result.Reserve(Items.Num());
		for (const TWeakObjectPtr<T>& Item : Items)
		{
			if (T* Resolved = Item.Get())
			{
				Result.Add(Resolved);
			}
		}
		return Result;
	}

	TArray<TWeakObjectPtr<AASBushArea>> Bushes;
	TArray<TWeakObjectPtr<UASVisionSourceComponent>> VisionSources;
	TArray<TWeakObjectPtr<UASVisibilityComponent>> VisibilityTargets;
	TWeakObjectPtr<AASFogOfWarManager> FogOfWarManager;
};
