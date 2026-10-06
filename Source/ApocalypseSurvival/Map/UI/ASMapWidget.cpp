#include "Map/UI/ASMapWidget.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Map/Core/ASMapDefinition.h"
#include "Map/Core/ASMapFunctionLibrary.h"
#include "Map/Core/ASMapStateComponent.h"
#include "Map/Core/ASMapWorldSubsystem.h"
#include "Map/Vision/ASFogOfWarManager.h"
#include "Map/Vision/ASVisibilityComponent.h"
#include "Map/Vision/ASVisionDefinition.h"
#include "Map/Vision/ASVisionWorldSubsystem.h"
#include "Map/World/ASMapZone.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "ASMapWidget"

namespace ASMapWidgetPrivate
{
	FPaintGeometry MakeGeometry(const FGeometry& Geometry, const FVector2D& Position, const FVector2D& Size)
	{
		return Geometry.ToPaintGeometry(
			FVector2f(static_cast<float>(Size.X), static_cast<float>(Size.Y)),
			FSlateLayoutTransform(FVector2f(static_cast<float>(Position.X), static_cast<float>(Position.Y))));
	}

	void DrawRect(
		FSlateWindowElementList& Out,
		int32 Layer,
		const FGeometry& Geometry,
		const FSlateBrush* Brush,
		const FVector2D& Position,
		const FVector2D& Size,
		const FLinearColor& Color)
	{
		if (Size.X <= 0.0 || Size.Y <= 0.0)
		{
			return;
		}

		FSlateDrawElement::MakeBox(Out, Layer, MakeGeometry(Geometry, Position, Size), Brush, ESlateDrawEffect::None, Color);
	}

	void DrawCenteredText(
		FSlateWindowElementList& Out,
		int32 Layer,
		const FGeometry& Geometry,
		const FText& Text,
		const FSlateFontInfo& Font,
		const FVector2D& Center,
		const FLinearColor& Color)
	{
		FVector2D TextSize(120.0, 16.0);
		if (FSlateApplication::IsInitialized())
		{
			TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
		}

		FSlateDrawElement::MakeText(
			Out,
			Layer,
			MakeGeometry(Geometry, Center - TextSize * 0.5, TextSize),
			Text,
			Font,
			ESlateDrawEffect::None,
			Color);
	}
}

UASMapWidget::UASMapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UASMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetClipping(EWidgetClipping::ClipToBounds);
}

void UASMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// NativePaint가 const이므로 브러시 리소스를 여기서 할당한다.
	const UASMapDefinition* Definition = UASMapFunctionLibrary::GetMapDefinition(this);
	UTexture2D* MapTexture = Definition ? Definition->MapTexture.Get() : nullptr;
	if (MapTextureBrush.GetResourceObject() != MapTexture)
	{
		MapTextureBrush.SetResourceObject(MapTexture);
		if (MapTexture)
		{
			MapTextureBrush.ImageSize = FVector2D(MapTexture->GetSizeX(), MapTexture->GetSizeY());
		}
	}

	const UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this);
	const AASFogOfWarManager* FogManager = Subsystem ? Subsystem->GetFogOfWarManager() : nullptr;
	UTexture2D* FogTexture = FogManager ? FogManager->GetFogTexture() : nullptr;
	if (FogBrush.GetResourceObject() != FogTexture)
	{
		FogBrush.SetResourceObject(FogTexture);
		if (FogTexture)
		{
			FogBrush.ImageSize = FVector2D(FogTexture->GetSizeX(), FogTexture->GetSizeY());
		}
	}
}

bool UASMapWidget::ComputeView(const FVector2D& LocalSize, const UASMapDefinition* Definition, FMapView& OutView) const
{
	if (!Definition || LocalSize.X <= 1.0 || LocalSize.Y <= 1.0)
	{
		return false;
	}

	const FVector2D WorldSize = Definition->GetMapWorldSize();

	if (Mode == EASMapWidgetMode::Minimap)
	{
		FVector2D CenterUV(0.5, 0.5);
		if (const APawn* Pawn = UASMapFunctionLibrary::GetLocalPlayerPawn(this))
		{
			CenterUV = Definition->WorldToMapUV(Pawn->GetActorLocation());
		}

		// U는 월드 Y를, V는 월드 X를 따른다.
		const FVector2D HalfUV(MinimapViewRadius / WorldSize.Y, MinimapViewRadius / WorldSize.X);
		const double WidgetAspect = LocalSize.X / LocalSize.Y;
		OutView.UVMin = CenterUV - FVector2D(HalfUV.X * WidgetAspect, HalfUV.Y);
		OutView.UVMax = CenterUV + FVector2D(HalfUV.X * WidgetAspect, HalfUV.Y);
		OutView.Position = FVector2D::ZeroVector;
		OutView.Size = LocalSize;
		return true;
	}

	// 확장 맵: 월드 비율을 유지하며 전체 맵을 맞춘다.
	const double MapAspect = WorldSize.Y / WorldSize.X;
	FVector2D Size = LocalSize;
	if (LocalSize.X / LocalSize.Y > MapAspect)
	{
		Size.X = LocalSize.Y * MapAspect;
	}
	else
	{
		Size.Y = LocalSize.X / MapAspect;
	}

	OutView.UVMin = FVector2D::ZeroVector;
	OutView.UVMax = FVector2D::UnitVector;
	OutView.Position = (LocalSize - Size) * 0.5;
	OutView.Size = Size;
	return true;
}

FVector2D UASMapWidget::WorldToWidgetLocal(const FVector& WorldLocation) const
{
	const UASMapDefinition* Definition = UASMapFunctionLibrary::GetMapDefinition(this);
	if (!Definition || !bHasLastView)
	{
		return FVector2D::ZeroVector;
	}
	return LastView.UVToLocal(Definition->WorldToMapUV(WorldLocation));
}

FLinearColor UASMapWidget::GetMarkerColor(uint8 MarkerType) const
{
	switch (static_cast<EASMapMarkerType>(MarkerType))
	{
	case EASMapMarkerType::Elite:
		return EliteColor;
	case EASMapMarkerType::Objective:
		return ObjectiveColor;
	case EASMapMarkerType::Interactable:
		return InteractableColor;
	default:
		return EnemyColor;
	}
}

int32 UASMapWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	using namespace ASMapWidgetPrivate;

	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D LocalSize = FVector2D(AllottedGeometry.GetLocalSize());
	const UASMapDefinition* Definition = UASMapFunctionLibrary::GetMapDefinition(this);
	const UASMapStateComponent* MapState = UASMapFunctionLibrary::GetMapStateComponent(this);
	const UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(this);
	const UASVisionWorldSubsystem* VisionSubsystem = UASVisionWorldSubsystem::Get(this);
	const UWorld* World = GetWorld();

	DrawRect(OutDrawElements, ++Layer, AllottedGeometry, &SolidBrush, FVector2D::ZeroVector, LocalSize, BackgroundColor);

	FMapView View;
	if (!ComputeView(LocalSize, Definition, View) || !MapState || !Subsystem || !VisionSubsystem || !World)
	{
		bHasLastView = false;
		return Layer;
	}

	LastView = View;
	bHasLastView = true;

	const FSlateFontInfo Font = LabelFont.HasValidFont() ? LabelFont : FCoreStyle::GetDefaultFontStyle("Bold", 9);
	const FVector2D MapTopLeft = View.UVToLocal(FVector2D::ZeroVector);
	const FVector2D MapSize = View.UVToLocal(FVector2D::UnitVector) - MapTopLeft;

	auto WorldToLocal = [&View, Definition](const FVector& Location)
	{
		return View.UVToLocal(Definition->WorldToMapUV(Location));
	};

	// 1. 배경 이미지.
	if (MapTextureBrush.GetResourceObject())
	{
		DrawRect(OutDrawElements, ++Layer, AllottedGeometry, &MapTextureBrush, MapTopLeft, MapSize, FLinearColor::White);
	}

	// 2. 전장의 안개는 지형만 어둡게 한다. 구역 상태는 공유 맵 데이터이므로 그 위에서 읽을 수 있다.
	const AASFogOfWarManager* FogManager = VisionSubsystem->GetFogOfWarManager();
	const UASVisionDefinition* VisionDefinition = FogManager ? FogManager->GetVisionDefinition() : nullptr;
	if (bShowFog && Definition && VisionDefinition && FogBrush.GetResourceObject())
	{
		const FVector2D A = WorldToLocal(FVector(VisionDefinition->FogWorldMin, 0.0));
		const FVector2D B = WorldToLocal(FVector(VisionDefinition->FogWorldMax, 0.0));
		const FVector2D FogTopLeft(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		const FVector2D FogSize(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y));
		DrawRect(OutDrawElements, ++Layer, AllottedGeometry, &FogBrush, FogTopLeft, FogSize, FLinearColor::White);
	}

	// 3. 구역: 공유 위험도 채우기와 구역 액터의 이름.
	const FASMapStateSnapshot& Snapshot = MapState->GetSnapshot();
	const int32 DangerLayer = ++Layer;
	const int32 LabelLayer = ++Layer;

	for (const AASMapZone* Zone : Subsystem->GetZones())
	{
		const FASZoneRuntimeState* State = Snapshot.FindZone(Zone->GetZoneId());
		const int32 Danger = State ? State->DangerLevel : 1;

		TArray<FBox2D> Footprints;
		Zone->GetFootprints(Footprints);

		for (const FBox2D& Footprint : Footprints)
		{
			const FVector2D A = WorldToLocal(FVector(Footprint.Min.X, Footprint.Min.Y, 0.0));
			const FVector2D B = WorldToLocal(FVector(Footprint.Max.X, Footprint.Max.Y, 0.0));
			const FVector2D TopLeft(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
			const FVector2D Size(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y));

			FLinearColor Fill = Definition->GetDangerColor(Danger);
			Fill.A = ZoneFillOpacity;
			DrawRect(OutDrawElements, DangerLayer, AllottedGeometry, &SolidBrush, TopLeft, Size, Fill);
		}

		if (bShowZoneLabels)
		{
			const FVector2D Center = WorldToLocal(Zone->GetZoneCenter());
			FText Label = FText::Format(
				LOCTEXT("ZoneLabel", "{0} [{1}]"),
				Zone->GetDisplayName(),
				FText::AsNumber(Danger));

			DrawCenteredText(OutDrawElements, LabelLayer, AllottedGeometry, Label, Font, Center, FLinearColor::White);
		}
	}

	// 4. 모든 요소 위에 마커를 표시한다. 적은 안개 텍스처가 아닌 팀 시야로 필터링한다.
	Layer = LabelLayer + 1;

	for (const UASVisibilityComponent* Target : VisionSubsystem->GetVisibilityTargets())
	{
		const AActor* Owner = Target->GetOwner();
		if (!Owner || !Target->ShouldShowOnMap() || !Target->IsVisibleToLocalTeam())
		{
			continue;
		}

		const FVector2D Position = WorldToLocal(Owner->GetActorLocation());
		const FVector2D Size(MarkerSize, MarkerSize);
		DrawRect(OutDrawElements, Layer, AllottedGeometry, &SolidBrush, Position - Size * 0.5, Size,
			GetMarkerColor(static_cast<uint8>(Target->GetMarkerType())));
	}

	const APawn* LocalPawn = UASMapFunctionLibrary::GetLocalPlayerPawn(this);
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
			if (!IsValid(Pawn))
			{
				continue;
			}

			const bool bLocal = Pawn == LocalPawn;
			const float Size = bLocal ? MarkerSize * 1.6f : MarkerSize * 1.2f;
			const FVector2D Position = WorldToLocal(Pawn->GetActorLocation());
			DrawRect(OutDrawElements, Layer + (bLocal ? 1 : 0), AllottedGeometry, &SolidBrush,
				Position - FVector2D(Size, Size) * 0.5, FVector2D(Size, Size), bLocal ? LocalPlayerColor : AllyColor);
		}
	}

	return Layer + 1;
}

#undef LOCTEXT_NAMESPACE
