#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapSystem/Zone/Core/ASMapTypes.h"
#include "ASMapZone.generated.h"

class UBoxComponent;
class USceneComponent;
class UShapeComponent;
class UASZoneLinkDebugComponent;

/**
 * 고정 맵 구역 중 하나. 레벨에 배치하며 모든 머신에 존재한다.
 *
 * 구역 영역은 액터에 있는 모든 도형 컴포넌트의 합집합이다 (불규칙
 * 영역에는 박스, 구, 캡슐을 추가한다. 도형에 "NotArea" 태그를 붙이면
 * 제외한다). 도형에는 충돌이 없다. 포함 여부를 직접 계산하므로
 * 순간이동, 리스폰, 중첩 볼륨에서도 중복 집계되지 않는다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASMapZone : public AActor
{
	GENERATED_BODY()

public:
	AASMapZone();

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	FName GetZoneId() const { return ZoneId; }

	/** 이름이 비어 있으면 구역 ID를 표시한다. */
	UFUNCTION(BlueprintPure, Category = "AS|Map")
	FText GetDisplayName() const { return DisplayName.IsEmpty() ? FText::FromName(ZoneId) : DisplayName; }

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	int32 GetInitialDangerLevel() const { return InitialDangerLevel; }

	/** 이 구역의 HUD와 지도 표시 이름. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Map")
	FText DisplayName;

	/** 매치 시작 시 위험도 (1..5). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "1", ClampMax = "5"))
	int32 InitialDangerLevel = ASMap::MinDangerLevel;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	bool ContainsLocation(const FVector& Location) const;

	/** 영역 도형의 월드 XY 경계 사각형 (맵 위젯에서 사용). */
	void GetFootprints(TArray<FBox2D>& OutFootprints) const;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	FVector GetZoneCenter() const;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	TArray<FTransform> GetSpawnTransforms() const;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	TArray<FTransform> GetRespawnTransforms() const;

	bool HasAreaShapes() const;

	/** 직접 이동할 수 있는 구역. 어느 한쪽에서만 지정해도 양방향으로 연결된다. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Map")
	TArray<TObjectPtr<AASMapZone>> AdjacentZones;

	const TArray<TObjectPtr<AASMapZone>>& GetAdjacentZones() const { return AdjacentZones; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void RefreshAreaShapes() const;

#if WITH_EDITOR
	void RefreshLinkDebugComponents();
#endif

	/** 레벨 안에서 고유한 구역 식별자. None은 초기화 오류다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (AllowPrivateAccess = "true"))
	FName ZoneId;

	/** 단층 맵은 높이를 무시해 점프나 경사로에서도 구역을 벗어나지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (AllowPrivateAccess = "true"))
	bool bIgnoreHeight = true;

	/** 액터 기준 상대 위치. 적과 상자의 스폰 시스템에서 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (AllowPrivateAccess = "true", MakeEditWidget))
	TArray<FTransform> SpawnPoints;

	/** 액터 기준 상대 위치. GameMode가 플레이어 리스폰에 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (AllowPrivateAccess = "true", MakeEditWidget))
	TArray<FTransform> RespawnPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** 첫 영역 도형. Blueprint/인스턴스에서 크기를 바꾸거나 도형을 추가한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> MainArea;

#if WITH_EDITORONLY_DATA
	/** 레벨 편집 중 인접 구역 연결선을 표시한다. */
	UPROPERTY()
	TObjectPtr<UASZoneLinkDebugComponent> LinkDebugComponent;
#endif

	/** 영역 도형은 이 액터가 소유한다. 약한 포인터로 리플렉션 대상인 가변 캐시를 피한다. */
	mutable TArray<TWeakObjectPtr<UShapeComponent>> CachedAreaShapes;

	mutable bool bAreaShapesCached = false;
};
