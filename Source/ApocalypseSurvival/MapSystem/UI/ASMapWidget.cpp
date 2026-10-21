#include "MapSystem/UI/ASMapWidget.h"

#include "AbilitySystemComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GAS/ASAbilityTags.h"
#include "GeomTools.h"
#include "MapSystem/Shared/ASMapAbilityUtils.h"
#include "MapSystem/Shared/ASMapShapeUtils.h"
#include "MapSystem/Zone/Core/ASMapDefinition.h"
#include "MapSystem/Zone/Core/ASMapFunctionLibrary.h"
#include "MapSystem/Zone/Core/ASMapStateComponent.h"
#include "MapSystem/Zone/Core/ASMapWorldSubsystem.h"
#include "MapSystem/Vision/ASFogOfWarManager.h"
#include "MapSystem/Vision/ASVisibilityComponent.h"
#include "MapSystem/Vision/ASVisionDefinition.h"
#include "MapSystem/Vision/ASVisionFunctionLibrary.h"
#include "MapSystem/Vision/ASVisionWorldSubsystem.h"
#include "MapSystem/Zone/ASMapZone.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateRenderer.h"
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

	void DrawTriangle(
		FSlateWindowElementList& Out,
		int32 Layer,
		const FGeometry& Geometry,
		const FSlateResourceHandle& ResourceHandle,
		const FVector2D& P0,
		const FVector2D& P1,
		const FVector2D& P2,
		const FLinearColor& Color)
	{
		const FVector2D Points[] = { P0, P1, P2 };
		const FColor VertexColor = Color.ToFColor(true);
		TArray<FSlateVertex> Vertices;
		Vertices.Reserve(3);
		for (const FVector2D& Point : Points)
		{
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
				Geometry.GetAccumulatedRenderTransform(),
				FVector2f(static_cast<float>(Point.X), static_cast<float>(Point.Y)),
				FVector2f(0.5f, 0.5f), VertexColor));
		}
		const TArray<SlateIndex> Indices = { 0, 1, 2 };
		FSlateDrawElement::MakeCustomVerts(Out, Layer, ResourceHandle, Vertices, Indices, nullptr, 0, 0);
	}

	void DrawCenteredText(
		FSlateWindowElementList& Out,
		int32 Layer,
		const FGeometry& Geometry,
		const FText& Text,
		const FSlateFontInfo& Font,
		const FVector2D& TextSize,
		const FVector2D& Center,
		const FLinearColor& Color)
	{
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

UASMapWidget::FZoneAreaCache& UASMapWidget::GetCachedZoneArea(const AASMapZone* Zone) const
{
	const bool bNewEntry = !ZoneAreaCache.Contains(TWeakObjectPtr<const AASMapZone>(Zone));
	FZoneAreaCache& Cache = ZoneAreaCache.FindOrAdd(TWeakObjectPtr<const AASMapZone>(Zone));
	if (bNewEntry)
	{
		const TArray<TArray<FVector2D>>& Outlines = Zone->GetAreaOutlines();
		Cache.WorldTriangles.Reset(Outlines.Num());
		for (const TArray<FVector2D>& Outline : Outlines)
		{
			TArray<FVector2D>& Triangles = Cache.WorldTriangles.AddDefaulted_GetRef();
			FString Error;
			if (!ASMapShapeUtils::ValidatePolygon(Outline, Error))
			{
				continue;
			}
			// UE 5.8의 귀 자르기는 양의 외적(CCW)을 요구한다. 출력 배열은 비워서 전달한다.
			TArray<FVector2D> CounterClockwise;
			FGeomTools2D::CorrectPolygonWinding(CounterClockwise, Outline, false);
			if (!FGeomTools2D::TriangulatePoly(Triangles, CounterClockwise))
			{
				Triangles.Reset();
			}
		}
	}
	return Cache;
}

struct UASMapWidget::FPaintContext
{
	const FGeometry& Geometry;
	FSlateWindowElementList& Out;
	const FMapView& View;
	const UASMapDefinition& Definition;
	FSlateFontInfo Font;
	/** Slate가 초기화되지 않으면 사용자 정점(구역 채우기, 화살표)을 그릴 수 없다. */
	bool bCanDrawCustomVerts;
	FSlateResourceHandle SolidResource;

	FVector2D WorldToLocal(const FVector& Location) const
	{
		return View.UVToLocal(Definition.WorldToMapUV(Location));
	}
};

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
		return Layer;
	}

	const bool bCanDrawCustomVerts = FSlateApplication::IsInitialized();
	const FPaintContext Context{
		AllottedGeometry,
		OutDrawElements,
		View,
		*Definition,
		LabelFont.HasValidFont() ? LabelFont : FCoreStyle::GetDefaultFontStyle("Bold", 9),
		bCanDrawCustomVerts,
		bCanDrawCustomVerts ? FSlateApplication::Get().GetRenderer()->GetResourceHandle(SolidBrush) : FSlateResourceHandle()};
	const FVector2D MapTopLeft = View.UVToLocal(FVector2D::ZeroVector);
	const FVector2D MapSize = View.UVToLocal(FVector2D::UnitVector) - MapTopLeft;

	// 1. 배경 이미지.
	if (MapTextureBrush.GetResourceObject())
	{
		DrawRect(OutDrawElements, ++Layer, AllottedGeometry, &MapTextureBrush, MapTopLeft, MapSize, FLinearColor::White);
	}

	// 2. 전장의 안개는 지형만 어둡게 한다. 구역 상태는 공유 맵 데이터이므로 그 위에서 읽을 수 있다.
	const AASFogOfWarManager* FogManager = VisionSubsystem->GetFogOfWarManager();
	const UASVisionDefinition* VisionDefinition = FogManager ? FogManager->GetVisionDefinition() : nullptr;
	if (bShowFog && VisionDefinition && FogBrush.GetResourceObject())
	{
		const FVector2D A = Context.WorldToLocal(FVector(VisionDefinition->FogWorldMin, 0.0));
		const FVector2D B = Context.WorldToLocal(FVector(VisionDefinition->FogWorldMax, 0.0));
		const FVector2D FogTopLeft(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		const FVector2D FogSize(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y));
		DrawRect(OutDrawElements, ++Layer, AllottedGeometry, &FogBrush, FogTopLeft, FogSize, FLinearColor(0, 0, 0, 1));
	}

	// 3. 구역: 공유 위험도 채우기와 구역 액터의 이름.
	const int32 FillLayer = ++Layer;
	const int32 LabelLayer = ++Layer;
	DrawZones(Context, *MapState, *Subsystem, FillLayer, LabelLayer);

	// 4. 모든 요소 위에 마커를 표시한다. 적은 안개 텍스처가 아닌 팀 시야로 필터링한다.
	Layer = LabelLayer + 1;
	DrawVisibilityMarkers(Context, *VisionSubsystem, Layer);
	DrawPlayers(Context, *World, Layer);
	return Layer + 3;
}

void UASMapWidget::DrawZones(const FPaintContext& Context, const UASMapStateComponent& MapState, const UASMapWorldSubsystem& Subsystem,
	int32 FillLayer, int32 LabelLayer) const
{
	using namespace ASMapWidgetPrivate;

	const TArray<AASMapZone*>& Zones = Subsystem.GetZones();
	for (auto It = ZoneAreaCache.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !Zones.Contains(It.Key().Get()))
		{
			It.RemoveCurrent();
		}
	}

	const FASMapStateSnapshot& Snapshot = MapState.GetSnapshot();
	for (const AASMapZone* Zone : Zones)
	{
		const FASZoneRuntimeState* State = Snapshot.FindZone(Zone->GetZoneId());
		const int32 Danger = State ? State->DangerLevel : 1;

		FZoneAreaCache& Area = GetCachedZoneArea(Zone);
		FLinearColor Fill = Context.Definition.GetDangerColor(Danger);
		Fill.A = ZoneFillOpacity;
		const FColor VertexColor = Fill.ToFColor(true);
		if (Context.bCanDrawCustomVerts)
		{
			for (const TArray<FVector2D>& Triangles : Area.WorldTriangles)
			{
				if (Triangles.IsEmpty())
				{
					continue;
				}
				TArray<FSlateVertex> Vertices;
				TArray<SlateIndex> Indices;
				Vertices.Reserve(Triangles.Num());
				Indices.Reserve(Triangles.Num());
				for (int32 Index = 0; Index < Triangles.Num(); ++Index)
				{
					const FVector2D Local = Context.WorldToLocal(FVector(Triangles[Index], 0.0));
					Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
						Context.Geometry.GetAccumulatedRenderTransform(),
						FVector2f(static_cast<float>(Local.X), static_cast<float>(Local.Y)),
						FVector2f(0.5f, 0.5f), VertexColor));
					Indices.Add(static_cast<SlateIndex>(Index));
				}
				FSlateDrawElement::MakeCustomVerts(Context.Out, FillLayer, Context.SolidResource,
					Vertices, Indices, nullptr, 0, 0);
			}
		}

		if (bShowZoneLabels)
		{
			const FVector2D Center = Context.WorldToLocal(Zone->GetZoneCenter());
			const FText DisplayName = Zone->GetDisplayName();
			const bool bLabelChanged = Area.DangerLevel != Danger || !Area.DisplayName.EqualTo(DisplayName);
			if (bLabelChanged)
			{
				Area.DangerLevel = Danger;
				Area.DisplayName = DisplayName;
				Area.Label = FText::Format(LOCTEXT("ZoneLabel", "{0} [{1}]"), DisplayName, FText::AsNumber(Danger));
			}
			if (bLabelChanged || !Area.Font.IsIdenticalTo(Context.Font))
			{
				Area.Font = Context.Font;
				Area.LabelSize = Context.bCanDrawCustomVerts
					? FVector2D(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Area.Label, Context.Font))
					: FVector2D(120.0, 16.0);
			}

			DrawCenteredText(Context.Out, LabelLayer, Context.Geometry, Area.Label, Context.Font, Area.LabelSize, Center, FLinearColor::White);
		}
	}
}

void UASMapWidget::DrawVisibilityMarkers(const FPaintContext& Context, const UASVisionWorldSubsystem& VisionSubsystem, int32 Layer) const
{
	using namespace ASMapWidgetPrivate;

	const int32 LocalTeam = UASVisionFunctionLibrary::GetLocalTeamId(this);
	for (const UASVisibilityComponent* Target : VisionSubsystem.GetVisibilityTargets())
	{
		const AActor* Owner = Target->GetOwner();
		if (!Owner || !Target->ShouldShowOnMap() || !Target->IsVisibleToTeam(LocalTeam))
		{
			continue;
		}

		const FVector2D Position = Context.WorldToLocal(Owner->GetActorLocation());
		const FVector2D Size(MarkerSize, MarkerSize);
		DrawRect(Context.Out, Layer, Context.Geometry, &SolidBrush, Position - Size * 0.5, Size,
			GetMarkerColor(static_cast<uint8>(Target->GetMarkerType())));
	}
}

void UASMapWidget::DrawPlayers(const FPaintContext& Context, const UWorld& World, int32 Layer) const
{
	using namespace ASMapWidgetPrivate;

	const AGameStateBase* GameState = World.GetGameState();
	if (!GameState)
	{
		return;
	}

	const APawn* LocalPawn = UASMapFunctionLibrary::GetLocalPlayerPawn(this);
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
		if (!IsValid(Pawn))
		{
			continue;
		}

		const UAbilitySystemComponent* AbilitySystem = ASMapAbilityUtils::FindAbilitySystemComponent(Pawn);
		if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(ASGameplayTags::State_Dead))
		{
			if (DeadPlayerColor.A > 0.0f)
			{
				const FVector2D Position = Context.WorldToLocal(Pawn->GetActorLocation());
				const FVector2D Size(MarkerSize, MarkerSize);
				DrawRect(Context.Out, Layer, Context.Geometry, &SolidBrush,
					Position - Size * 0.5, Size, DeadPlayerColor);
			}
			continue;
		}

		const bool bLocal = Pawn == LocalPawn;
		const float Size = bLocal ? MarkerSize * 1.6f : MarkerSize * 1.2f;
		const FVector2D Position = Context.WorldToLocal(Pawn->GetActorLocation());
		const int32 MarkerLayer = Layer + (bLocal ? 2 : 1);
		const FLinearColor Color = bLocal ? LocalPlayerColor : AllyColor;
		if (!Context.bCanDrawCustomVerts)
		{
			DrawRect(Context.Out, MarkerLayer, Context.Geometry, &SolidBrush,
				Position - FVector2D(Size, Size) * 0.5, FVector2D(Size, Size), Color);
			continue;
		}

		FVector Forward = Pawn->GetActorForwardVector();
		Forward.Z = 0.0;
		FVector2D Direction = Context.WorldToLocal(Pawn->GetActorLocation() + Forward * 100.0) - Position;
		if (!Direction.Normalize())
		{
			Direction = FVector2D(0.0, -1.0);
		}
		const FVector2D HalfLength = Direction * (Size * 0.5);
		const FVector2D HalfWidth = FVector2D(-Direction.Y, Direction.X) * (Size * 0.4);
		DrawTriangle(Context.Out, MarkerLayer, Context.Geometry, Context.SolidResource,
			Position + HalfLength, Position - HalfLength + HalfWidth, Position - HalfLength - HalfWidth, Color);
	}
}

#undef LOCTEXT_NAMESPACE
