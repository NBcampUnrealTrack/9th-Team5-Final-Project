#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "ASMapWidget.generated.h"

class UASMapDefinition;
class UTexture2D;

UENUM(BlueprintType)
enum class EASMapWidgetMode : uint8
{
	/** 위젯 안에 전체 맵을 맞춘다. */
	Expanded,
	/** 로컬 플레이어를 중심으로 하는 정사각형 창. */
	Minimap
};

/**
 * 최소 구성의 네이티브 맵. 공유 MapState에서 구역 위험도 색상과 라벨을 그린다. 또한
 * 로컬 안개 텍스처, 아군 (GameState 플레이어), 현재 보이는
 * 적과 목표물 (시야 대상)을 그린다. 이 클래스로 Widget Blueprint를
 * 만들고 크기를 지정한다. 자식 위젯은 필요하지 않다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UASMapWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
	EASMapWidgetMode Mode = EASMapWidgetMode::Minimap;

	/** 월드 단위로 나타낸 미니맵 창 너비의 절반. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map", meta = (ClampMin = "500.0", Units = "cm"))
	float MinimapViewRadius = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
	bool bShowFog = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map")
	bool bShowZoneLabels = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ZoneFillOpacity = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Style")
	FLinearColor BackgroundColor = FLinearColor(0.03f, 0.03f, 0.04f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Style")
	FLinearColor LocalPlayerColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Style")
	FLinearColor AllyColor = FLinearColor(0.2f, 0.8f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Style")
	FLinearColor EnemyColor = FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Style")
	FLinearColor EliteColor = FLinearColor(0.85f, 0.2f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Style")
	FLinearColor ObjectiveColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Style")
	FLinearColor InteractableColor = FLinearColor(0.6f, 1.0f, 0.6f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Style", meta = (ClampMin = "2.0"))
	float MarkerSize = 7.0f;

	/** 비워 두면 엔진 기본 폰트를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map|Style")
	FSlateFontInfo LabelFont;

	/** 마지막으로 그린 뷰에서 월드 위치의 위젯 로컬 위치 (Blueprint 오버레이용). */
	UFUNCTION(BlueprintPure, Category = "AS|Map")
	FVector2D WorldToWidgetLocal(const FVector& WorldLocation) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	struct FMapView
	{
		FVector2D UVMin = FVector2D::ZeroVector;
		FVector2D UVMax = FVector2D::UnitVector;
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;

		FVector2D UVToLocal(const FVector2D& UV) const
		{
			const FVector2D Range = (UVMax - UVMin).ComponentMax(FVector2D(UE_KINDA_SMALL_NUMBER, UE_KINDA_SMALL_NUMBER));
			return Position + (UV - UVMin) / Range * Size;
		}
	};

	bool ComputeView(const FVector2D& LocalSize, const UASMapDefinition* Definition, FMapView& OutView) const;
	FLinearColor GetMarkerColor(uint8 MarkerType) const;

	/** 기본 브러시: MakeBox에 전달한 색상으로 틴트한 단순 사각형. */
	FSlateBrush SolidBrush;
	FSlateBrush MapTextureBrush;
	FSlateBrush FogBrush;
	mutable FMapView LastView;
	mutable bool bHasLastView = false;
};
