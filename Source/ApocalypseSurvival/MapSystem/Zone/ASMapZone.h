#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapSystem/Zone/Core/ASMapTypes.h"
#include "ASMapZone.generated.h"

class UASMapPolygonAreaComponent;
class USceneComponent;
class UPrimitiveComponent;
class UASZoneLinkDebugComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * 고정 맵 구역 중 하나. 레벨에 배치하며 모든 머신에 존재한다.
 *
 * 구역 영역은 다각형 영역과 박스, 구, 캡슐 등 도형 컴포넌트의 합집합이다.
 * 다각형은 스플라인 점을 드래그해 편집하고 Alt+드래그로 점을 추가하며
 * 컨텍스트 메뉴에서 점을 삭제한다. 접선은 무시한다. 컴포넌트에 "NotArea"
 * 태그를 붙이면 제외한다. 영역에는 충돌이 없다. 포함 여부를 직접 계산하므로
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary")
	bool bShowBoundary = true;

	/** 구역 경계선 색. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary")
	FLinearColor BoundaryColor = FLinearColor::White;

	/** 구역 액터의 Z 위치를 기준으로 한 경계선 윗면 높이. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary", meta = (Units = "cm"))
	float BoundaryHeight = 2.f;

	/** 겹치는 리본과 원판에도 같은 밀도를 유지하는 디더 마스크 불투명도. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boundary", meta = (ClampMin = "0", ClampMax = "1"))
	float BoundaryOpacity = 0.7f;

	UPROPERTY(EditDefaultsOnly, Category = "Boundary")
	TObjectPtr<UMaterialInterface> BoundaryMaterial;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	bool ContainsLocation(const FVector& Location) const;

	/** 영역 컴포넌트의 월드 XY 외곽선 (맵 위젯에서 사용). */
	const TArray<TArray<FVector2D>>& GetAreaOutlines() const;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	FVector GetZoneCenter() const;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	TArray<FTransform> GetSpawnTransforms() const;

	UFUNCTION(BlueprintPure, Category = "AS|Map")
	TArray<FTransform> GetRespawnTransforms() const;

	bool HasAreaShapes() const;

	/** 모든 다각형 오류에 구역 ID와 컴포넌트 이름을 붙여 추가한다. */
	bool ValidateArea(TArray<FString>& OutErrors) const;

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
	void BuildBoundary();

#if WITH_EDITOR
	void RefreshLinkDebugComponents();
#endif

	/** 레벨 안에서 고유한 구역 식별자. None은 초기화 오류다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (AllowPrivateAccess = "true"))
	FName ZoneId;

	/** 단층 맵은 높이를 무시해 점프나 경사로에서도 구역을 벗어나지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (AllowPrivateAccess = "true"))
	bool bIgnoreHeight = true;

	/**
	 * 뷰포트 위젯으로 편집하는 액터 기준 상대 스폰 지점.
	 * UASMapFunctionLibrary::GetZoneSpawnPoints가 구역 정보와 함께 스폰 시스템에 반환한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (AllowPrivateAccess = "true", MakeEditWidget))
	TArray<FTransform> SpawnPoints;

	/**
	 * 뷰포트 위젯으로 편집하는 액터 기준 상대 리스폰 지점.
	 * UASMapFunctionLibrary::GetZoneRespawnPoints가 구역 정보와 함께 스폰 시스템에 반환한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (AllowPrivateAccess = "true", MakeEditWidget))
	TArray<FTransform> RespawnPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** 기본 다각형 영역. 뷰포트에서 점을 편집하거나 영역 컴포넌트를 추가한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASMapPolygonAreaComponent> MainPolygon;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BoundaryPlaneMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> BoundaryCylinderMesh;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> BoundaryComponent;

	/** 안쪽 중심선 꼭짓점의 둥근 이음새. */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> BoundaryJointComponent;

#if WITH_EDITORONLY_DATA
	/** 레벨 편집 중 인접 구역 연결선과 스폰·리스폰 지점을 표시한다. */
	UPROPERTY()
	TObjectPtr<UASZoneLinkDebugComponent> LinkDebugComponent;
#endif

	/** 영역 컴포넌트는 이 액터가 소유한다. 약한 포인터로 리플렉션 대상인 가변 캐시를 피한다. */
	mutable TArray<TWeakObjectPtr<UPrimitiveComponent>> CachedAreaShapes;
	// 플레이 중 영역 도형이 바뀌어도 외곽선·중심 캐시는 갱신되지 않는다 (포함 판정은 컴포넌트를 직접 검사하므로 영향 없음).
	mutable TArray<TArray<FVector2D>> CachedOutlines;
	mutable FVector CachedCenter = FVector::ZeroVector;

	mutable bool bAreaShapesCached = false;
};
