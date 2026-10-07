#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASMapViewModelSubsystem.generated.h"

class UASMapViewModel;
class UASZoneHudViewModel;

/**
 * 이 월드의 로컬 플레이어용 맵 뷰모델을 소유하고 최신 상태로 유지한다.
 *
 * 월드 플레이가 시작되면 MVVM 전역 뷰모델 컬렉션에
 * 추가된다 ("ZoneHud", "Map"). 위젯은 Creation Type = Global Viewmodel
 * Collection으로 선택한다. 컬렉션은 게임 인스턴스마다 하나이므로
 * 게임 인스턴스당 로컬 플레이어 한 명을 전제로 한다 (분할 화면 없음).
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASMapViewModelSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UASMapViewModelSubsystem* Get(const UObject* WorldContextObject);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	UASZoneHudViewModel* GetZoneHudViewModel() const { return ZoneHudViewModel; }

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	UASMapViewModel* GetMapViewModel() const { return MapViewModel; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void RefreshZoneHud();
	void UnregisterViewModels();

	UPROPERTY(Transient)
	TObjectPtr<UASZoneHudViewModel> ZoneHudViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UASMapViewModel> MapViewModel;
};
