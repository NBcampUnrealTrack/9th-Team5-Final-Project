#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASMapWorldSubsystem.generated.h"

class AASMapZone;

DECLARE_MULTICAST_DELEGATE_OneParam(FASOnMapZoneRegistrationChanged, AASMapZone*);

/**
 * 월드별 구역 레지스트리. 서버와 모든 클라이언트에 존재하므로
 * 어느 머신에서도 공간 조회가 동작한다. 서버 맵 매니저가 검증하고
 * 규칙을 소유한다. 이 클래스는 게임 상태를 보관하지 않는다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASMapWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UASMapWorldSubsystem* Get(const UObject* WorldContextObject);

	// 구역
	void RegisterZone(AASMapZone* Zone);
	void UnregisterZone(AASMapZone* Zone);

	/** ZoneId 순으로 정렬된 등록 구역. 여러 액터가 ID를 공유할 수 있다 (검증에서 보고). */
	const TArray<AASMapZone*>& GetZones() const;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	AASMapZone* FindZone(FName ZoneId) const;

	/** 영역에 해당 위치를 포함하는 첫 구역 (ZoneId 순). */
	UFUNCTION(BlueprintPure, Category = "AS|Map")
	AASMapZone* FindZoneAtLocation(const FVector& Location) const;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	FName FindZoneIdAtLocation(const FVector& Location) const;

	FASOnMapZoneRegistrationChanged OnZoneRegistered;
	FASOnMapZoneRegistrationChanged OnZoneUnregistered;

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

	TArray<TWeakObjectPtr<AASMapZone>> Zones;
	mutable TArray<AASMapZone*> SortedZones;
	mutable bool bSortedZonesDirty = true;
};
