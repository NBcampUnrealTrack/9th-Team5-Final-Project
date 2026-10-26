#include "MapSystem/Vision/ASFogDisplayComponent.h"

#include "Engine/LocalPlayer.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "MapSystem/Vision/ASVisionDefinition.h"
#include "MapSystem/Vision/ASFogMask.h"
#include "MapSystem/Vision/ASFogOfWarManager.h"
#include "MapSystem/Vision/ASVisionEvaluatorComponent.h"
#include "MapSystem/Vision/ASVisionFunctionLibrary.h"
#include "MapSystem/Vision/ASVisionSourceComponent.h"
#include "MapSystem/Vision/ASVisionWorldSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "RHI.h"

UASFogDisplayComponent::UASFogDisplayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	SetIsReplicatedByDefault(false);
}

bool UASFogDisplayComponent::HasLocalPlayer() const
{
	const UWorld* World = GetWorld();
	return GetNetMode() != NM_DedicatedServer && World && World->GetFirstLocalPlayerFromController() != nullptr;
}

const UASVisionSourceComponent* UASFogDisplayComponent::GetLocalVisionSource() const
{
	const UWorld* World = GetWorld();
	const ULocalPlayer* Player = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	const APlayerController* Controller = Player ? Player->GetPlayerController(World) : nullptr;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	const UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this);
	if (!Pawn || !Subsystem || !VisionDefinition)
	{
		return nullptr;
	}
	const int32 LocalTeam = UASVisionFunctionLibrary::GetLocalTeamId(this);
	for (const UASVisionSourceComponent* Source : Subsystem->GetVisionSources())
	{
		if (Source->GetOwner() == Pawn && Source->GetTeamId() == LocalTeam
			&& Source->IsProvidingVision(VisionDefinition->DeadStateTag))
		{
			return Source;
		}
	}
	return nullptr;
}

void UASFogDisplayComponent::UpdateLocalVisionParameters()
{
	// FogTexture와 같은 UpdateLocalMask Snapshot만 렌더링에 공개한다.
	// 여기서 현재 Pawn 위치나 낮/밤 반경을 다시 계산하면, 텍스처와 서로 다른 시점의
	// 시야가 섞여 대시·반경 축소 때 밝은 잔상이 생긴다.
	if (WorldFogMaterial)
	{
		WorldFogMaterial->SetVectorParameterValue(TEXT("LocalVisionCenter"),
			FLinearColor(LocalVisionCenter.X, LocalVisionCenter.Y, 0.0f, 0.0f));
		WorldFogMaterial->SetScalarParameterValue(TEXT("LocalVisionRadius"), LocalVisionRadius);
		WorldFogMaterial->SetScalarParameterValue(TEXT("LocalVisionSoftness"), LocalVisionSoftness);
	}
}

void UASFogDisplayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!VisionDefinition || !HasLocalPlayer())
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (NeedsImmediateLocalMaskRefresh())
	{
		// 대시나 시야 반경 급변은 다음 UpdateHz 타이머까지 기다리지 않는다.
		// UpdateLocalMask가 FogTexture와 Material Snapshot을 한 번에 갱신한다.
		UpdateLocalMask();
		return;
	}

	AdvancePendingFade();
}

void UASFogDisplayComponent::UpdateShownAlpha(float Fraction)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASFogOfWar_FadeStep);

	ShownChangedRegions.Reset();
	for (int32 TileIndex = 0; TileIndex < PendingFadeTiles.Num(); ++TileIndex)
	{
		if (!PendingFadeTiles[TileIndex])
		{
			continue;
		}
		const FIntRect Region = TileRegion(TileIndex);
		FIntRect Dirty;
		bool bChanged = ASFogMask::LerpToward(ShownAlpha, FadeFromAlpha, TargetAlpha, ResolutionX, ResolutionY,
			Fraction, Region, Dirty);
		FIntRect LocalTileRegion = Region;
		LocalTileRegion.Clip(LocalRegion);
		if (LocalTileRegion.Width() > 0 && LocalTileRegion.Height() > 0)
		{
			auto LerpLocalAlpha = [&](TArray<uint8>& Shown, const TArray<uint8>& From, const TArray<uint8>& Target)
			{
				FIntRect LocalDirty;
				if (ASFogMask::LerpToward(Shown, From, Target, ResolutionX, ResolutionY, Fraction, LocalTileRegion, LocalDirty))
				{
					if (bChanged)
					{
						Dirty.Union(LocalDirty);
					}
					else
					{
						Dirty = LocalDirty;
					}
					bChanged = true;
				}
			};
			LerpLocalAlpha(DisplayShownAlpha, DisplayFadeFromAlpha, DisplayTargetAlpha);
			LerpLocalAlpha(OthersShownAlpha, OthersFadeFromAlpha, OthersTargetAlpha);
		}
		if (bChanged)
		{
			ShownChangedRegions.Add(Dirty);
		}
		if (Fraction >= 1.0f)
		{
			PendingFadeTiles[TileIndex] = 0;
		}
	}
	UploadTexture(ShownChangedRegions);
	if (Fraction >= 1.0f)
	{
		bHasPendingFade = false;
	}
}

void UASFogDisplayComponent::Configure(UASVisionDefinition* Definition, const FVector2D& WorldMin, const FVector2D& WorldMax)
{
	const FIntPoint WantedResolution = ASFogMask::ComputeResolution(
		(WorldMax - WorldMin).ComponentMax(FVector2D(1.0, 1.0)), Definition->Settings.MaskResolution);
	if (Definition != VisionDefinition || WantedResolution.X != ResolutionX || WantedResolution.Y != ResolutionY
		|| WorldMin != FogWorldMin || WorldMax != FogWorldMax)
	{
		VisionDefinition = Definition;
		ResolutionX = WantedResolution.X;
		ResolutionY = WantedResolution.Y;
		FogWorldMin = WorldMin;
		FogWorldMax = WorldMax;

		const int32 CellCount = ResolutionX * ResolutionY;
		VisibleCells.Init(0, CellCount);
		MassVisibilityBufferCells.Init(0, CellCount);
		MassVisibilityDistanceScratch.Reset();
		DisplayCells.Init(0, CellCount);
		OthersCells.Init(0, CellCount);
		ExploredCells.Init(0, CellCount);
		RawAlpha.Init(0, CellCount);
		TargetAlpha.SetNumUninitialized(CellCount);
		ShownAlpha.SetNumUninitialized(CellCount);
		FadeFromAlpha.Init(0, CellCount);
		DisplayRawAlpha.Init(0, CellCount);
		DisplayTargetAlpha.SetNumUninitialized(CellCount);
		DisplayShownAlpha.SetNumUninitialized(CellCount);
		DisplayFadeFromAlpha.Init(0, CellCount);
		OthersRawAlpha.Init(0, CellCount);
		OthersTargetAlpha.SetNumUninitialized(CellCount);
		OthersShownAlpha.SetNumUninitialized(CellCount);
		OthersFadeFromAlpha.Init(0, CellCount);
		BlurScratch.Reset();
		TilesX = FMath::DivideAndRoundUp(ResolutionX, TileSize);
		TilesY = FMath::DivideAndRoundUp(ResolutionY, TileSize);
		PreviousVisibleTiles.Init(0, TilesX * TilesY);
		CurrentVisibleTiles.Init(0, TilesX * TilesY);
		PendingFadeTiles.Init(0, TilesX * TilesY);
		LocalRegion = FIntRect(0, 0, 0, 0);
		RawChangedRegions.Reset();
		RawChangedRegions.Reserve(TilesX * TilesY);
		ShownChangedRegions.Reset();
		ShownChangedRegions.Reserve(TilesX * TilesY);
		OcclusionPoints.Reserve(512);
		ExtendedOcclusionPoints.Reserve(512);
		bMaskDirty = true;
		bHasLocalVisionMask = false;
		bInitializeShownAlpha = true;
		bHasPendingFade = false;

		if (GetNetMode() != NM_DedicatedServer)
		{
			FogTexture = UTexture2D::CreateTransient(ResolutionX, ResolutionY, PF_B8G8R8A8);
			if (FogTexture)
			{
				FogTexture->SRGB = false;
				FogTexture->Filter = TF_Bilinear;
				FogTexture->AddressX = TA_Clamp;
				FogTexture->AddressY = TA_Clamp;
				FogTexture->UpdateResource();
			}
		}
	}
	SetComponentTickEnabled(HasLocalPlayer());
}

FASFogMaterialState UASFogDisplayComponent::GetMaterialState() const
{
	FASFogMaterialState State;
	State.FogTexture = FogTexture;
	State.MapWorldMin = FogWorldMin;
	State.MapWorldSize = GetFogWorldSize();
	State.LocalVisionCenter = LocalVisionCenter;
	State.LocalVisionRadius = LocalVisionRadius;
	State.LocalVisionSoftness = LocalVisionSoftness;
	State.bHasLocalVisionMask = bHasLocalVisionMask;
	return State;
}

void UASFogDisplayComponent::UpdateWorldFog()
{
	UMaterialInterface* FogMaterial = VisionDefinition ? VisionDefinition->WorldFogMaterial.Get() : nullptr;
	if (!FogMaterial || !FogTexture)
	{
		RemoveWorldFog();
		return;
	}

	if (!WorldFogMaterial || WorldFogMaterial->Parent != FogMaterial)
	{
		WorldFogMaterial = UMaterialInstanceDynamic::Create(FogMaterial, GetOwner());
		if (WorldFogVolume)
		{
			WorldFogVolume->Settings.WeightedBlendables.Array.Reset();
			WorldFogVolume->Settings.AddBlendable(WorldFogMaterial, 1.0f);
		}
	}

	UpdateLocalVisionParameters();

	// 시야 정의나 해상도가 바뀌면 텍스처를 다시 생성한다.
	WorldFogMaterial->SetTextureParameterValue(TEXT("FogTexture"), FogTexture);
	const FVector2D WorldSize = GetFogWorldSize();
	WorldFogMaterial->SetVectorParameterValue(TEXT("MapWorldMin"), FLinearColor(FogWorldMin.X, FogWorldMin.Y, 0.0f, 0.0f));
	WorldFogMaterial->SetVectorParameterValue(TEXT("MapWorldSize"), FLinearColor(WorldSize.X, WorldSize.Y, 0.0f, 0.0f));

	if (!WorldFogVolume)
	{
		// 로컬 표시 전용: 각 머신은 자기 팀의 안개를 그린다.
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.ObjectFlags |= RF_Transient;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		WorldFogVolume = GetWorld()->SpawnActor<APostProcessVolume>(SpawnParams);
		if (WorldFogVolume)
		{
			WorldFogVolume->SetReplicates(false);
			WorldFogVolume->bUnbound = true;
			WorldFogVolume->Priority = 1000.0f;
			WorldFogVolume->Settings.AddBlendable(WorldFogMaterial, 1.0f);
		}
	}
}

void UASFogDisplayComponent::RemoveWorldFog()
{
	if (WorldFogVolume)
	{
		WorldFogVolume->Destroy();
		WorldFogVolume = nullptr;
	}
	WorldFogMaterial = nullptr;
}

void UASFogDisplayComponent::ResetExplored()
{
	FMemory::Memzero(ExploredCells.GetData(), ExploredCells.Num());
	bMaskDirty = true;
}

void UASFogDisplayComponent::UpdateLocalMask()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASFogOfWar_LocalMask);

	if (!HasLocalPlayer())
	{
		SetComponentTickEnabled(false);
		return;
	}

	const UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this);
	const AASFogOfWarManager* Manager = Cast<AASFogOfWarManager>(GetOwner());
	const UASVisionEvaluatorComponent* Evaluator = Manager ? Manager->GetVisionEvaluator() : nullptr;
	if (!Subsystem || !Manager || !Evaluator || !VisionDefinition || ResolutionX <= 0 || ResolutionY <= 0)
	{
		return;
	}

	FLocalMaskContext Context{
		Manager->IsWallOcclusionEnabled(), Manager->GetNightAlpha(),
		UASVisionFunctionLibrary::GetLocalTeamId(this), GetFogWorldSize(),
		FMath::Clamp(VisionDefinition->Settings.FogEdgeBlurTexels, 0, 8), GetLocalVisionSource()
	};
	if (Context.LocalSource)
	{
		Context.LocalOrigin = Evaluator->GetTraceLocation(Context.LocalSource->GetOwner());
		Context.LocalCenter = FVector2D(Context.LocalOrigin.X, Context.LocalOrigin.Y);
		Context.LocalRadius = Context.LocalSource->GetVisionRadius(VisionDefinition->Settings, Context.NightAlpha);
	}
	AdvancePendingFade();
	ClearPreviousVisibleCells();
	const FIntRect PreviousLocalRegion = RasterizeTeamSources(*Subsystem, *Evaluator, Context);
	// VisibleCells는 여기부터 현재 로컬 팀 시야를 나타낸다. 초기 빈 배열을 표현 컬링에 쓰지 않는다.
	bHasLocalVisionMask = true;
	RebuildMassVisibilityBuffer();
	const bool bLocalRegionChanged = LocalRegion != PreviousLocalRegion;
	const bool bFullRecompute = RebuildRawAlpha(PreviousLocalRegion, bLocalRegionChanged);
	if (!bFullRecompute && RawChangedRegions.IsEmpty() && !bLocalRegionChanged)
	{
		// 래스터 결과가 동일한 경우에는 이 Snapshot도 같은 텍스처 상태로 취급할 수 있다.
		LocalVisionCenter = Context.LocalCenter;
		LocalVisionRadius = Context.LocalRadius;
		LocalVisionSoftness = FMath::Max(1.0f, VisionDefinition->Settings.LocalVisionSoftness);
		UpdateLocalVisionParameters();
		return;
	}
	// B 채널은 페이드된 월드 안개가 아니라 현재 논리 시야 자체를 기록한다.
	// 시야가 바뀐 타일은 다음 페이드 Tick을 기다리지 않고 즉시 반영한다.
	if (!bFullRecompute && !RawChangedRegions.IsEmpty())
	{
		UploadTexture(RawChangedRegions);
	}
	BlurChangedRegions(bFullRecompute, bLocalRegionChanged, Context.BlurRadius);

	// 현재 시야는 부드러운 탐색 안개와 달리 즉시 표시해야 한다. 이전 로컬 영역에서
	// 빠져나온 셀은 A 채널에 남아 있던 밝은 Fade 값을 목표값으로 확정해 잔상을 제거한다.
	TArray<FIntRect> ImmediateUploadRegions;
	if (bLocalRegionChanged)
	{
		ResolveExitedLocalRegion(PreviousLocalRegion, ImmediateUploadRegions);
	}
	if (LocalRegion.Width() > 0 && LocalRegion.Height() > 0)
	{
		CommitLocalMaterialChannels(LocalRegion);
		ImmediateUploadRegions.Add(LocalRegion);
	}
	if (!ImmediateUploadRegions.IsEmpty())
	{
		UploadTexture(ImmediateUploadRegions);
	}
	StartLocalFade(VisionDefinition->Settings.FogFadeSeconds);

	// 위에서 업로드한 FogTexture와 완전히 같은 시점의 원점/반경만 Material에 전달한다.
	LocalVisionCenter = Context.LocalCenter;
	LocalVisionRadius = Context.LocalRadius;
	LocalVisionSoftness = FMath::Max(1.0f, VisionDefinition->Settings.LocalVisionSoftness);
	UpdateLocalVisionParameters();
}

void UASFogDisplayComponent::AdvancePendingFade()
{
	// 타이머가 tick보다 먼저 온다.
	if (bHasPendingFade)
	{
		const float FadeSeconds = VisionDefinition->Settings.FogFadeSeconds;
		const float Fraction = FadeSeconds <= 0.0f ? 1.0f
			: FMath::Clamp((GetWorld()->GetTimeSeconds() - FadeStartTime) / FadeSeconds, 0.0f, 1.0f);
		UpdateShownAlpha(Fraction);
	}
}

void UASFogDisplayComponent::ClearPreviousVisibleCells()
{
	for (int32 TileIndex = 0; TileIndex < PreviousVisibleTiles.Num(); ++TileIndex)
	{
		if (!PreviousVisibleTiles[TileIndex])
		{
			continue;
		}
		const FIntRect Region = TileRegion(TileIndex);
		for (int32 Y = Region.Min.Y; Y < Region.Max.Y; ++Y)
		{
			const int32 Start = Y * ResolutionX + Region.Min.X;
			FMemory::Memzero(VisibleCells.GetData() + Start, Region.Width());
			FMemory::Memzero(DisplayCells.GetData() + Start, Region.Width());
			FMemory::Memzero(OthersCells.GetData() + Start, Region.Width());
		}
	}
}

bool UASFogDisplayComponent::NeedsImmediateLocalMaskRefresh() const
{
	if (!bHasLocalVisionMask || !VisionDefinition || ResolutionX <= 0 || ResolutionY <= 0)
	{
		return false;
	}

	const AASFogOfWarManager* Manager = Cast<AASFogOfWarManager>(GetOwner());
	const UASVisionEvaluatorComponent* Evaluator = Manager ? Manager->GetVisionEvaluator() : nullptr;
	const UASVisionSourceComponent* Source = GetLocalVisionSource();
	if (!Manager || !Evaluator || !Source)
	{
		return LocalVisionRadius > 0.0f;
	}

	const FVector Center = Evaluator->GetTraceLocation(Source->GetOwner());
	const FVector2D CurrentCenter(Center.X, Center.Y);
	const float CurrentRadius = Source->GetVisionRadius(VisionDefinition->Settings, Manager->GetNightAlpha());
	const FVector2D WorldSize = GetFogWorldSize();
	const float TexelWorldSize = FMath::Max(WorldSize.Y / ResolutionX, WorldSize.X / ResolutionY);
	const float RefreshThreshold = FMath::Max(1.0f, TexelWorldSize);

	return FVector2D::Distance(CurrentCenter, LocalVisionCenter) >= RefreshThreshold
		|| FMath::Abs(CurrentRadius - LocalVisionRadius) >= RefreshThreshold;
}

void UASFogDisplayComponent::TraceSource(const UASVisionEvaluatorComponent& Evaluator, const UASVisionSourceComponent* Source,
	const FVector& Origin, float Radius, float DisplayRadius, const UASVisionSourceComponent* LocalSource)
{
	const FASVisionSettings& Settings = VisionDefinition->Settings;
	const int32 RayCount = FMath::Clamp(Settings.OcclusionRayCount, 16, 512);
	OcclusionPoints.Reset();
	ExtendedOcclusionPoints.Reset();
	for (int32 RayIndex = 0; RayIndex < RayCount; ++RayIndex)
	{
		const float Angle = 2.0f * UE_PI * static_cast<float>(RayIndex) / static_cast<float>(RayCount);
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const FVector End = Origin + Direction * DisplayRadius;
		// 확장 반경까지 한 번만 추적한다.
		FHitResult Hit;
		float Distance = DisplayRadius;
		if (Evaluator.TraceVisionWall(Hit, Origin, End, Source->GetOwner()))
		{
			Distance = FMath::Min(DisplayRadius, static_cast<float>(Hit.Distance) + Settings.WallRevealPadding);
		}
		OcclusionPoints.Add(WorldToGrid(Origin + Direction * FMath::Min(Radius, Distance)));
		if (Source == LocalSource)
		{
			ExtendedOcclusionPoints.Add(WorldToGrid(Origin + Direction * Distance));
		}
	}
}

FIntRect UASFogDisplayComponent::RasterizeTeamSources(const UASVisionWorldSubsystem& Subsystem,
	const UASVisionEvaluatorComponent& Evaluator, const FLocalMaskContext& Context)
{
	const FASVisionSettings& Settings = VisionDefinition->Settings;
	for (const UASVisionSourceComponent* Source : Subsystem.GetVisionSources())
	{
		if (Source == Context.LocalSource || Source->GetTeamId() != Context.LocalTeam
			|| !Source->IsProvidingVision(VisionDefinition->DeadStateTag))
		{
			continue;
		}
		const FVector Origin = Evaluator.GetTraceLocation(Source->GetOwner());
		const float Radius = Source->GetVisionRadius(Settings, Context.NightAlpha);
		const FVector2D CenterGrid = WorldToGrid(Origin);
		if (!Context.bWallOcclusion)
		{
			const FVector2D Radii(Radius / Context.WorldSize.Y * ResolutionX, Radius / Context.WorldSize.X * ResolutionY);
			RasterizeEllipse(OthersCells, CenterGrid, Radii);
			continue;
		}
		TraceSource(Evaluator, Source, Origin, Radius, Radius, Context.LocalSource);
		for (int32 Index = 0; Index < OcclusionPoints.Num(); ++Index)
		{
			RasterizeTriangle(OthersCells, CenterGrid, OcclusionPoints[Index], OcclusionPoints[(Index + 1) % OcclusionPoints.Num()]);
		}
	}

	const FIntRect PreviousLocalRegion = LocalRegion;
	LocalRegion = FIntRect(0, 0, 0, 0);
	FVector2D LocalCenter = FVector2D::ZeroVector;
	FVector2D LocalRadii = FVector2D::ZeroVector;
	FVector2D DisplayRadii = FVector2D::ZeroVector;
	if (Context.LocalSource)
	{
		const FVector& Origin = Context.LocalOrigin;
		const float Radius = Context.LocalRadius;
		const float DisplayRadius = Radius + FMath::Max(0.0f, Settings.LocalVisionMargin);
		LocalCenter = WorldToGrid(Origin);
		LocalRadii = FVector2D(Radius / Context.WorldSize.Y * ResolutionX, Radius / Context.WorldSize.X * ResolutionY);
		DisplayRadii = FVector2D(DisplayRadius / Context.WorldSize.Y * ResolutionX, DisplayRadius / Context.WorldSize.X * ResolutionY);
		FVector2D Min = LocalCenter - DisplayRadii;
		FVector2D Max = LocalCenter + DisplayRadii;
		if (Context.bWallOcclusion)
		{
			TraceSource(Evaluator, Context.LocalSource, Origin, Radius, DisplayRadius, Context.LocalSource);
			Min = LocalCenter;
			Max = LocalCenter;
			for (const FVector2D& Point : ExtendedOcclusionPoints)
			{
				Min.X = FMath::Min(Min.X, Point.X);
				Min.Y = FMath::Min(Min.Y, Point.Y);
				Max.X = FMath::Max(Max.X, Point.X);
				Max.Y = FMath::Max(Max.Y, Point.Y);
			}
		}
		if (Context.bWallOcclusion || (DisplayRadii.X > 0.0 && DisplayRadii.Y > 0.0))
		{
			const int32 MinX = FMath::FloorToInt(Min.X);
			const int32 MinY = FMath::FloorToInt(Min.Y);
			const int32 MaxX = FMath::CeilToInt(Max.X);
			const int32 MaxY = FMath::CeilToInt(Max.Y);
			// 로컬 전용 타일도 복사 전에 표시한다.
			MarkVisibleTiles(FMath::Max(0, MinX), FMath::Max(0, MinY), FMath::Min(ResolutionX - 1, MaxX), FMath::Min(ResolutionY - 1, MaxY));
			const int32 Padding = Context.BlurRadius + 1;
			LocalRegion = FIntRect(FMath::Clamp(MinX - Padding, 0, ResolutionX), FMath::Clamp(MinY - Padding, 0, ResolutionY),
				FMath::Clamp(MaxX + 1 + Padding, 0, ResolutionX), FMath::Clamp(MaxY + 1 + Padding, 0, ResolutionY));
			if (LocalRegion.Width() <= 0 || LocalRegion.Height() <= 0)
			{
				LocalRegion = FIntRect(0, 0, 0, 0);
			}
		}
	}

	for (int32 TileIndex = 0; TileIndex < CurrentVisibleTiles.Num(); ++TileIndex)
	{
		if (!CurrentVisibleTiles[TileIndex])
		{
			continue;
		}
		const FIntRect Region = TileRegion(TileIndex);
		for (int32 Y = Region.Min.Y; Y < Region.Max.Y; ++Y)
		{
			const int32 Start = Y * ResolutionX + Region.Min.X;
			FMemory::Memcpy(VisibleCells.GetData() + Start, OthersCells.GetData() + Start, Region.Width());
			FMemory::Memcpy(DisplayCells.GetData() + Start, OthersCells.GetData() + Start, Region.Width());
		}
	}
	if (Context.LocalSource)
	{
		if (!Context.bWallOcclusion)
		{
			RasterizeEllipse(VisibleCells, LocalCenter, LocalRadii);
			RasterizeEllipse(DisplayCells, LocalCenter, DisplayRadii);
		}
		else
		{
			for (int32 Index = 0; Index < OcclusionPoints.Num(); ++Index)
			{
				const int32 Next = (Index + 1) % OcclusionPoints.Num();
				RasterizeTriangle(VisibleCells, LocalCenter, OcclusionPoints[Index], OcclusionPoints[Next]);
				RasterizeTriangle(DisplayCells, LocalCenter, ExtendedOcclusionPoints[Index], ExtendedOcclusionPoints[Next]);
			}
		}
	}
	return PreviousLocalRegion;
}

bool UASFogDisplayComponent::RebuildRawAlpha(const FIntRect& PreviousLocalRegion, bool bLocalRegionChanged)
{
	RawChangedRegions.Reset();
	const uint8 Explored = VisionDefinition->ExploredAlpha;
	const uint8 Unexplored = VisionDefinition->UnexploredAlpha;
	for (int32 TileIndex = 0; TileIndex < PreviousVisibleTiles.Num(); ++TileIndex)
	{
		const FIntRect Region = TileRegion(TileIndex);
		FIntRect LocalTileRegion = Region;
		LocalTileRegion.Clip(LocalRegion);
		const bool bLocalTile = LocalTileRegion.Width() > 0 && LocalTileRegion.Height() > 0;
		if (!bMaskDirty && !PreviousVisibleTiles[TileIndex] && !CurrentVisibleTiles[TileIndex] && !bLocalTile)
		{
			continue;
		}
		int32 ChangedMinX = Region.Max.X;
		int32 ChangedMinY = Region.Max.Y;
		int32 ChangedMaxX = Region.Min.X;
		int32 ChangedMaxY = Region.Min.Y;
		for (int32 Y = Region.Min.Y; Y < Region.Max.Y; ++Y)
		{
			for (int32 X = Region.Min.X; X < Region.Max.X; ++X)
			{
				const int32 Index = Y * ResolutionX + X;
				if (CurrentVisibleTiles[TileIndex])
				{
					ExploredCells[Index] |= VisibleCells[Index];
				}
				const uint8 BaseAlpha = ExploredCells[Index] ? Explored : Unexplored;
				const uint8 Alpha = VisibleCells[Index] ? 0 : BaseAlpha;
				bool bLocalChanged = false;
				if (X >= LocalTileRegion.Min.X && X < LocalTileRegion.Max.X && Y >= LocalTileRegion.Min.Y && Y < LocalTileRegion.Max.Y)
				{
					const uint8 DisplayAlpha = DisplayCells[Index] ? 0 : BaseAlpha;
					const uint8 OthersAlpha = OthersCells[Index] ? 0 : BaseAlpha;
					bLocalChanged = DisplayRawAlpha[Index] != DisplayAlpha || OthersRawAlpha[Index] != OthersAlpha;
					DisplayRawAlpha[Index] = DisplayAlpha;
					OthersRawAlpha[Index] = OthersAlpha;
				}
				if (!bMaskDirty && (RawAlpha[Index] != Alpha || bLocalChanged))
				{
					ChangedMinX = FMath::Min(ChangedMinX, X);
					ChangedMinY = FMath::Min(ChangedMinY, Y);
					ChangedMaxX = FMath::Max(ChangedMaxX, X + 1);
					ChangedMaxY = FMath::Max(ChangedMaxY, Y + 1);
				}
				RawAlpha[Index] = Alpha;
			}
		}
		if (ChangedMinX < ChangedMaxX && ChangedMinY < ChangedMaxY)
		{
			RawChangedRegions.Add(FIntRect(ChangedMinX, ChangedMinY, ChangedMaxX, ChangedMaxY));
		}
	}

	Swap(PreviousVisibleTiles, CurrentVisibleTiles);
	FMemory::Memzero(CurrentVisibleTiles.GetData(), CurrentVisibleTiles.Num());
	const bool bFullRecompute = bMaskDirty;
	bMaskDirty = false;
	if (bLocalRegionChanged)
	{
		if (!bInitializeShownAlpha)
		{
			// 새로 들어온 셀은 현재 A에서 시작한다.
			CopyLocalAlphaOutsideRegion(DisplayShownAlpha, OthersShownAlpha, ShownAlpha, LocalRegion, PreviousLocalRegion);
		}
		MarkPendingTiles(LocalRegion);
		MarkPendingTiles(PreviousLocalRegion);
	}
	return bFullRecompute;
}

void UASFogDisplayComponent::CopyLocalAlphaOutsideRegion(TArray<uint8>& Display, TArray<uint8>& Others,
	const TArray<uint8>& Alpha, const FIntRect& Region, const FIntRect& Excluded)
{
	for (int32 Y = Region.Min.Y; Y < Region.Max.Y; ++Y)
	{
		const int32 Row = Y * ResolutionX;
		auto CopySpan = [&](int32 MinX, int32 MaxX)
		{
			if (MinX < MaxX)
			{
				FMemory::Memcpy(Display.GetData() + Row + MinX, Alpha.GetData() + Row + MinX, MaxX - MinX);
				FMemory::Memcpy(Others.GetData() + Row + MinX, Alpha.GetData() + Row + MinX, MaxX - MinX);
			}
		};
		if (Y < Excluded.Min.Y || Y >= Excluded.Max.Y)
		{
			CopySpan(Region.Min.X, Region.Max.X);
		}
		else
		{
			CopySpan(Region.Min.X, FMath::Min(Region.Max.X, Excluded.Min.X));
			CopySpan(FMath::Max(Region.Min.X, Excluded.Max.X), Region.Max.X);
		}
	}
}

void UASFogDisplayComponent::BlurLocalAlpha(const FIntRect& Region, int32 BlurRadius)
{
	FIntRect LocalTileRegion = Region;
	LocalTileRegion.Clip(LocalRegion);
	if (LocalTileRegion.Width() <= 0 || LocalTileRegion.Height() <= 0)
	{
		return;
	}
	const FIntRect Samples(FMath::Max(0, LocalTileRegion.Min.X - BlurRadius), FMath::Max(0, LocalTileRegion.Min.Y - BlurRadius),
		FMath::Min(ResolutionX, LocalTileRegion.Max.X + BlurRadius), FMath::Min(ResolutionY, LocalTileRegion.Max.Y + BlurRadius));
	// 흐림이 읽는 바깥 여백은 A의 원본과 같다.
	CopyLocalAlphaOutsideRegion(DisplayRawAlpha, OthersRawAlpha, RawAlpha, Samples, LocalRegion);
	ASFogMask::BlurAlphaRegion(DisplayRawAlpha, DisplayTargetAlpha, ResolutionX, ResolutionY, BlurRadius, LocalTileRegion, BlurScratch);
	ASFogMask::BlurAlphaRegion(OthersRawAlpha, OthersTargetAlpha, ResolutionX, ResolutionY, BlurRadius, LocalTileRegion, BlurScratch);
}

void UASFogDisplayComponent::BlurChangedRegions(bool bFullRecompute, bool bLocalRegionChanged, int32 BlurRadius)
{
	if (bFullRecompute)
	{
		ASFogMask::BlurAlphaRegion(RawAlpha, TargetAlpha, ResolutionX, ResolutionY, BlurRadius, FIntRect(0, 0, ResolutionX, ResolutionY), BlurScratch);
		BlurLocalAlpha(LocalRegion, BlurRadius);
		FMemory::Memset(PendingFadeTiles.GetData(), 1, PendingFadeTiles.Num());
	}
	else
	{
		for (const FIntRect& Changed : RawChangedRegions)
		{
			// 두 흐림 패스의 영향 범위.
			const FIntRect Region(FMath::Max(0, Changed.Min.X - BlurRadius), FMath::Max(0, Changed.Min.Y - BlurRadius),
				FMath::Min(ResolutionX, Changed.Max.X + BlurRadius), FMath::Min(ResolutionY, Changed.Max.Y + BlurRadius));
			ASFogMask::BlurAlphaRegion(RawAlpha, TargetAlpha, ResolutionX, ResolutionY, BlurRadius, Region, BlurScratch);
			if (!bLocalRegionChanged)
			{
				BlurLocalAlpha(Region, BlurRadius);
			}
			MarkPendingTiles(Region);
		}
	}
	if (!bFullRecompute && bLocalRegionChanged)
	{
		BlurLocalAlpha(LocalRegion, BlurRadius);
	}
}

void UASFogDisplayComponent::CommitLocalMaterialChannels(const FIntRect& Region)
{
	const FIntRect Clamped(
		FMath::Clamp(Region.Min.X, 0, ResolutionX), FMath::Clamp(Region.Min.Y, 0, ResolutionY),
		FMath::Clamp(Region.Max.X, 0, ResolutionX), FMath::Clamp(Region.Max.Y, 0, ResolutionY));
	if (Clamped.Width() <= 0 || Clamped.Height() <= 0)
	{
		return;
	}

	for (int32 Y = Clamped.Min.Y; Y < Clamped.Max.Y; ++Y)
	{
		const int32 Start = Y * ResolutionX + Clamped.Min.X;
		const int32 Width = Clamped.Width();
		FMemory::Memcpy(DisplayShownAlpha.GetData() + Start, DisplayTargetAlpha.GetData() + Start, Width);
		FMemory::Memcpy(DisplayFadeFromAlpha.GetData() + Start, DisplayTargetAlpha.GetData() + Start, Width);
		FMemory::Memcpy(OthersShownAlpha.GetData() + Start, OthersTargetAlpha.GetData() + Start, Width);
		FMemory::Memcpy(OthersFadeFromAlpha.GetData() + Start, OthersTargetAlpha.GetData() + Start, Width);
	}
}

void UASFogDisplayComponent::ResolveExitedLocalRegion(const FIntRect& PreviousLocalRegion,
	TArray<FIntRect>& OutUploadRegions)
{
	const FIntRect Previous(
		FMath::Clamp(PreviousLocalRegion.Min.X, 0, ResolutionX), FMath::Clamp(PreviousLocalRegion.Min.Y, 0, ResolutionY),
		FMath::Clamp(PreviousLocalRegion.Max.X, 0, ResolutionX), FMath::Clamp(PreviousLocalRegion.Max.Y, 0, ResolutionY));
	if (Previous.Width() <= 0 || Previous.Height() <= 0)
	{
		return;
	}

	const FIntRect Overlap(
		FMath::Max(Previous.Min.X, LocalRegion.Min.X), FMath::Max(Previous.Min.Y, LocalRegion.Min.Y),
		FMath::Min(Previous.Max.X, LocalRegion.Max.X), FMath::Min(Previous.Max.Y, LocalRegion.Max.Y));
	TArray<FIntRect, TInlineAllocator<4>> ExitedRegions;
	auto AddIfValid = [&ExitedRegions](const FIntRect& Region)
	{
		if (Region.Width() > 0 && Region.Height() > 0)
		{
			ExitedRegions.Add(Region);
		}
	};

	if (Overlap.Width() <= 0 || Overlap.Height() <= 0)
	{
		AddIfValid(Previous);
	}
	else
	{
		AddIfValid(FIntRect(Previous.Min.X, Previous.Min.Y, Previous.Max.X, Overlap.Min.Y));
		AddIfValid(FIntRect(Previous.Min.X, Overlap.Max.Y, Previous.Max.X, Previous.Max.Y));
		AddIfValid(FIntRect(Previous.Min.X, Overlap.Min.Y, Overlap.Min.X, Overlap.Max.Y));
		AddIfValid(FIntRect(Overlap.Max.X, Overlap.Min.Y, Previous.Max.X, Overlap.Max.Y));
	}

	for (const FIntRect& Region : ExitedRegions)
	{
		for (int32 Y = Region.Min.Y; Y < Region.Max.Y; ++Y)
		{
			const int32 Start = Y * ResolutionX + Region.Min.X;
			const int32 Width = Region.Width();
			FMemory::Memcpy(ShownAlpha.GetData() + Start, TargetAlpha.GetData() + Start, Width);
			FMemory::Memcpy(FadeFromAlpha.GetData() + Start, TargetAlpha.GetData() + Start, Width);

			// 이탈한 셀은 더 이상 G/R 전용 영역이 아니다. 다음 프레임에 재진입해도
			// 이전 로컬 시야의 밝은 값이 재사용되지 않도록 전부 현재 A 목표값으로 정리한다.
			FMemory::Memcpy(DisplayRawAlpha.GetData() + Start, TargetAlpha.GetData() + Start, Width);
			FMemory::Memcpy(DisplayTargetAlpha.GetData() + Start, TargetAlpha.GetData() + Start, Width);
			FMemory::Memcpy(DisplayShownAlpha.GetData() + Start, TargetAlpha.GetData() + Start, Width);
			FMemory::Memcpy(DisplayFadeFromAlpha.GetData() + Start, TargetAlpha.GetData() + Start, Width);
			FMemory::Memcpy(OthersRawAlpha.GetData() + Start, TargetAlpha.GetData() + Start, Width);
			FMemory::Memcpy(OthersTargetAlpha.GetData() + Start, TargetAlpha.GetData() + Start, Width);
			FMemory::Memcpy(OthersShownAlpha.GetData() + Start, TargetAlpha.GetData() + Start, Width);
			FMemory::Memcpy(OthersFadeFromAlpha.GetData() + Start, TargetAlpha.GetData() + Start, Width);
		}
		OutUploadRegions.Add(Region);
	}
}

void UASFogDisplayComponent::StartLocalFade(float FadeSeconds)
{
	if (bInitializeShownAlpha)
	{
		FMemory::Memcpy(ShownAlpha.GetData(), TargetAlpha.GetData(), TargetAlpha.Num());
		for (int32 Y = LocalRegion.Min.Y; Y < LocalRegion.Max.Y; ++Y)
		{
			const int32 Start = Y * ResolutionX + LocalRegion.Min.X;
			FMemory::Memcpy(DisplayShownAlpha.GetData() + Start, DisplayTargetAlpha.GetData() + Start, LocalRegion.Width());
			FMemory::Memcpy(OthersShownAlpha.GetData() + Start, OthersTargetAlpha.GetData() + Start, LocalRegion.Width());
		}
		bInitializeShownAlpha = false;
		FMemory::Memzero(PendingFadeTiles.GetData(), PendingFadeTiles.Num());
		bHasPendingFade = false;
		UploadTexture({ FIntRect(0, 0, ResolutionX, ResolutionY) });
	}
	else if (FadeSeconds <= 0.0f)
	{
		UpdateShownAlpha(1.0f);
	}
	else
	{
		// 진행 중인 타일도 현재 표시값에서 다시 보간한다.
		for (int32 TileIndex = 0; TileIndex < PendingFadeTiles.Num(); ++TileIndex)
		{
			if (!PendingFadeTiles[TileIndex])
			{
				continue;
			}
			const FIntRect Region = TileRegion(TileIndex);
			FIntRect LocalTileRegion = Region;
			LocalTileRegion.Clip(LocalRegion);
			bool bSettled = true;
			for (int32 Y = Region.Min.Y; Y < Region.Max.Y; ++Y)
			{
				const int32 Start = Y * ResolutionX + Region.Min.X;
				if (FMemory::Memcmp(ShownAlpha.GetData() + Start, TargetAlpha.GetData() + Start, Region.Width()) != 0)
				{
					bSettled = false;
					break;
				}
			}
			if (bSettled && LocalTileRegion.Width() > 0 && LocalTileRegion.Height() > 0)
			{
				for (int32 Y = LocalTileRegion.Min.Y; Y < LocalTileRegion.Max.Y; ++Y)
				{
					const int32 Start = Y * ResolutionX + LocalTileRegion.Min.X;
					if (FMemory::Memcmp(DisplayShownAlpha.GetData() + Start, DisplayTargetAlpha.GetData() + Start, LocalTileRegion.Width()) != 0
						|| FMemory::Memcmp(OthersShownAlpha.GetData() + Start, OthersTargetAlpha.GetData() + Start, LocalTileRegion.Width()) != 0)
					{
						bSettled = false;
						break;
					}
				}
			}
			// 이미 목표에 도달한 타일은 대기에서 뺀다.
			if (bSettled)
			{
				PendingFadeTiles[TileIndex] = 0;
				continue;
			}
			for (int32 Y = Region.Min.Y; Y < Region.Max.Y; ++Y)
			{
				const int32 Start = Y * ResolutionX + Region.Min.X;
				FMemory::Memcpy(FadeFromAlpha.GetData() + Start, ShownAlpha.GetData() + Start, Region.Width());
			}
			if (LocalTileRegion.Width() > 0 && LocalTileRegion.Height() > 0)
			{
				for (int32 Y = LocalTileRegion.Min.Y; Y < LocalTileRegion.Max.Y; ++Y)
				{
					const int32 Start = Y * ResolutionX + LocalTileRegion.Min.X;
					FMemory::Memcpy(DisplayFadeFromAlpha.GetData() + Start, DisplayShownAlpha.GetData() + Start, LocalTileRegion.Width());
					FMemory::Memcpy(OthersFadeFromAlpha.GetData() + Start, OthersShownAlpha.GetData() + Start, LocalTileRegion.Width());
				}
			}
		}
		bHasPendingFade = PendingFadeTiles.Contains(1);
		if (bHasPendingFade)
		{
			FadeStartTime = GetWorld()->GetTimeSeconds();
		}
	}
}

FVector2D UASFogDisplayComponent::WorldToGrid(const FVector& Location) const
{
	if (!VisionDefinition)
	{
		return FVector2D::ZeroVector;
	}
	const FVector2D Size = GetFogWorldSize();
	const FVector2D UV(
		(Location.Y - FogWorldMin.Y) / Size.Y,
		1.0 - (Location.X - FogWorldMin.X) / Size.X);
	return FVector2D(UV.X * ResolutionX, UV.Y * ResolutionY);
}

int32 UASFogDisplayComponent::GridIndexAt(const FVector& Location) const
{
	if (ResolutionX <= 0 || ResolutionY <= 0)
	{
		return INDEX_NONE;
	}

	const FVector2D Grid = WorldToGrid(Location);
	const int32 X = FMath::FloorToInt(Grid.X);
	const int32 Y = FMath::FloorToInt(Grid.Y);
	if (X < 0 || Y < 0 || X >= ResolutionX || Y >= ResolutionY)
	{
		return INDEX_NONE;
	}

	return Y * ResolutionX + X;
}

bool UASFogDisplayComponent::IsLocationExplored(const FVector& Location) const
{
	const int32 Index = GridIndexAt(Location);
	return ExploredCells.IsValidIndex(Index) && ExploredCells[Index] != 0;
}

bool UASFogDisplayComponent::IsLocationVisibleLocally(const FVector& Location) const
{
	const int32 Index = GridIndexAt(Location);
	return VisibleCells.IsValidIndex(Index) && VisibleCells[Index] != 0;
}

bool UASFogDisplayComponent::IsLocationInLocalVisionBuffer(const FVector& Location) const
{
	const int32 Index = GridIndexAt(Location);
	return MassVisibilityBufferCells.IsValidIndex(Index) && MassVisibilityBufferCells[Index] != 0;
}

void UASFogDisplayComponent::RebuildMassVisibilityBuffer()
{
	if (MassVisibilityBufferCells.Num() != VisibleCells.Num())
	{
		MassVisibilityBufferCells.Init(0, VisibleCells.Num());
	}
	else
	{
		FMemory::Memzero(MassVisibilityBufferCells.GetData(), MassVisibilityBufferCells.Num());
	}

	if (!VisionDefinition || ResolutionX <= 0 || ResolutionY <= 0)
	{
		return;
	}

	const FVector2D WorldSize = GetFogWorldSize();
	const float CellSize = FMath::Max(1.0f, FMath::Max(WorldSize.Y / ResolutionX, WorldSize.X / ResolutionY));
	const int32 BufferRadiusCells = FMath::CeilToInt(
		FMath::Max(0.0f, VisionDefinition->Settings.MassVisibilityBufferDistance) / CellSize);
	// 각 seed에서 반경만큼 원형을 반복 기록하지 않고 3-4 chamfer distance transform으로
	// 바깥쪽 ISM 유지 Buffer를 O(전체 Grid)에 계산한다.
	constexpr int32 InfiniteDistance = MAX_int32 / 8;
	auto BuildChamferDistance = [this, InfiniteDistance](const bool bDistanceFromVisible, const bool bTreatGridEdgeAsSeed)
	{
		MassVisibilityDistanceScratch.SetNumUninitialized(VisibleCells.Num());
		for (int32 Y = 0; Y < ResolutionY; ++Y)
		{
			for (int32 X = 0; X < ResolutionX; ++X)
			{
				const int32 Index = Y * ResolutionX + X;
				const bool bVisible = VisibleCells[Index] != 0;
				const bool bGridEdge = X == 0 || Y == 0 || X + 1 == ResolutionX || Y + 1 == ResolutionY;
				const bool bSeed = bDistanceFromVisible ? bVisible : !bVisible || (bTreatGridEdgeAsSeed && bGridEdge);
				MassVisibilityDistanceScratch[Index] = bSeed ? 0 : InfiniteDistance;
			}
		}

		auto RelaxDistance = [this](const int32 Index, const int32 NeighborIndex, const int32 Cost)
		{
			MassVisibilityDistanceScratch[Index] = FMath::Min(
				MassVisibilityDistanceScratch[Index],
				MassVisibilityDistanceScratch[NeighborIndex] + Cost);
		};

		for (int32 Y = 0; Y < ResolutionY; ++Y)
		{
			for (int32 X = 0; X < ResolutionX; ++X)
			{
				const int32 Index = Y * ResolutionX + X;
				if (X > 0)
				{
					RelaxDistance(Index, Index - 1, 3);
				}
				if (Y > 0)
				{
					RelaxDistance(Index, Index - ResolutionX, 3);
					if (X > 0)
					{
						RelaxDistance(Index, Index - ResolutionX - 1, 4);
					}
					if (X + 1 < ResolutionX)
					{
						RelaxDistance(Index, Index - ResolutionX + 1, 4);
					}
				}
			}
		}

		for (int32 Y = ResolutionY - 1; Y >= 0; --Y)
		{
			for (int32 X = ResolutionX - 1; X >= 0; --X)
			{
				const int32 Index = Y * ResolutionX + X;
				if (X + 1 < ResolutionX)
				{
					RelaxDistance(Index, Index + 1, 3);
				}
				if (Y + 1 < ResolutionY)
				{
					RelaxDistance(Index, Index + ResolutionX, 3);
					if (X > 0)
					{
						RelaxDistance(Index, Index + ResolutionX - 1, 4);
					}
					if (X + 1 < ResolutionX)
					{
						RelaxDistance(Index, Index + ResolutionX + 1, 4);
					}
				}
			}
		}
	};

	BuildChamferDistance(true, false);
	const int32 BufferDistanceThreshold = BufferRadiusCells * 3;
	for (int32 Index = 0; Index < MassVisibilityBufferCells.Num(); ++Index)
	{
		MassVisibilityBufferCells[Index] = MassVisibilityDistanceScratch[Index] <= BufferDistanceThreshold ? 1 : 0;
	}

}

FIntRect UASFogDisplayComponent::TileRegion(int32 TileIndex) const
{
	const int32 MinX = (TileIndex % TilesX) * TileSize;
	const int32 MinY = (TileIndex / TilesX) * TileSize;
	return FIntRect(MinX, MinY, FMath::Min(MinX + TileSize, ResolutionX), FMath::Min(MinY + TileSize, ResolutionY));
}

void UASFogDisplayComponent::MarkVisibleTiles(int32 MinX, int32 MinY, int32 MaxX, int32 MaxY)
{
	if (MinX > MaxX || MinY > MaxY)
	{
		return;
	}
	for (int32 Y = MinY / TileSize; Y <= MaxY / TileSize; ++Y)
	{
		for (int32 X = MinX / TileSize; X <= MaxX / TileSize; ++X)
		{
			CurrentVisibleTiles[Y * TilesX + X] = 1;
		}
	}
}

void UASFogDisplayComponent::MarkPendingTiles(const FIntRect& Region)
{
	if (Region.Width() <= 0 || Region.Height() <= 0)
	{
		return;
	}
	for (int32 Y = Region.Min.Y / TileSize; Y <= (Region.Max.Y - 1) / TileSize; ++Y)
	{
		for (int32 X = Region.Min.X / TileSize; X <= (Region.Max.X - 1) / TileSize; ++X)
		{
			PendingFadeTiles[Y * TilesX + X] = 1;
		}
	}
}

void UASFogDisplayComponent::RasterizeEllipse(TArray<uint8>& Cells, const FVector2D& Center, const FVector2D& Radii)
{
	if (Radii.X <= 0.0 || Radii.Y <= 0.0)
	{
		return;
	}

	const int32 MinX = FMath::Max(0, FMath::FloorToInt(Center.X - Radii.X));
	const int32 MaxX = FMath::Min(ResolutionX - 1, FMath::CeilToInt(Center.X + Radii.X));
	const int32 MinY = FMath::Max(0, FMath::FloorToInt(Center.Y - Radii.Y));
	const int32 MaxY = FMath::Min(ResolutionY - 1, FMath::CeilToInt(Center.Y + Radii.Y));
	MarkVisibleTiles(MinX, MinY, MaxX, MaxY);

	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		const double DY = (Y + 0.5 - Center.Y) / Radii.Y;
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const double DX = (X + 0.5 - Center.X) / Radii.X;
			if (DX * DX + DY * DY <= 1.0)
			{
				Cells[Y * ResolutionX + X] = 1;
			}
		}
	}
}

void UASFogDisplayComponent::RasterizeTriangle(TArray<uint8>& Cells, const FVector2D& A, const FVector2D& B, const FVector2D& C)
{
	const int32 MinX = FMath::Max(0, FMath::FloorToInt(FMath::Min3(A.X, B.X, C.X)));
	const int32 MaxX = FMath::Min(ResolutionX - 1, FMath::CeilToInt(FMath::Max3(A.X, B.X, C.X)));
	const int32 MinY = FMath::Max(0, FMath::FloorToInt(FMath::Min3(A.Y, B.Y, C.Y)));
	const int32 MaxY = FMath::Min(ResolutionY - 1, FMath::CeilToInt(FMath::Max3(A.Y, B.Y, C.Y)));
	if (MinX > MaxX || MinY > MaxY)
	{
		return;
	}

	auto Edge = [](const FVector2D& P0, const FVector2D& P1, const FVector2D& P)
	{
		return (P1.X - P0.X) * (P.Y - P0.Y) - (P1.Y - P0.Y) * (P.X - P0.X);
	};

	const double Area = Edge(A, B, C);
	if (FMath::IsNearlyZero(Area))
	{
		return;
	}
	MarkVisibleTiles(MinX, MinY, MaxX, MaxY);

	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		int32 SpanMinX;
		int32 SpanMaxX;
		if (ASFogMask::TriangleRowSpan(A, B, C, Area, Y, MinX, MaxX, SpanMinX, SpanMaxX))
		{
			FMemory::Memset(Cells.GetData() + Y * ResolutionX + SpanMinX, 1, SpanMaxX - SpanMinX + 1);
		}
	}
}

void UASFogDisplayComponent::UploadTexture(const TArray<FIntRect>& DirtyRegions)
{
	if (!FogTexture || ResolutionX <= 0 || ResolutionY <= 0 || DirtyRegions.IsEmpty())
	{
		return;
	}

	// 모든 영역에 같은 행 간격을 쓰고 세로로 쌓아 한 번에 업로드한다.
	int32 PitchWidth = 0;
	int32 TotalRows = 0;
	for (const FIntRect& Dirty : DirtyRegions)
	{
		PitchWidth = FMath::Max(PitchWidth, Dirty.Width());
		TotalRows += Dirty.Height();
	}
	uint8* Pixels = new uint8[PitchWidth * TotalRows * 4];
	FUpdateTextureRegion2D* Regions = new FUpdateTextureRegion2D[DirtyRegions.Num()];
	int32 SourceY = 0;
	for (int32 RegionIndex = 0; RegionIndex < DirtyRegions.Num(); ++RegionIndex)
	{
		const FIntRect& Dirty = DirtyRegions[RegionIndex];
		Regions[RegionIndex] = FUpdateTextureRegion2D(Dirty.Min.X, Dirty.Min.Y, 0, SourceY, Dirty.Width(), Dirty.Height());
		for (int32 Y = 0; Y < Dirty.Height(); ++Y)
		{
			for (int32 X = 0; X < Dirty.Width(); ++X)
			{
				uint8* Pixel = Pixels + ((SourceY + Y) * PitchWidth + X) * 4;
				const int32 GridX = Dirty.Min.X + X;
				const int32 GridY = Dirty.Min.Y + Y;
				const int32 Index = GridY * ResolutionX + GridX;
				// FogTexture는 PF_B8G8R8A8이다.
				// B: 현재 로컬 팀 논리 시야 (0 = 시야 밖, 255 = 현재 시야).
				// G/R/A: 기존 월드 안개와 미니맵의 페이드된 알파 표현.
				Pixel[0] = VisibleCells.IsValidIndex(Index) && VisibleCells[Index] ? 255 : 0;
				const bool bLocal = GridX >= LocalRegion.Min.X && GridX < LocalRegion.Max.X
					&& GridY >= LocalRegion.Min.Y && GridY < LocalRegion.Max.Y;
				Pixel[1] = bLocal ? DisplayShownAlpha[Index] : ShownAlpha[Index]; // G
				Pixel[2] = bLocal ? OthersShownAlpha[Index] : ShownAlpha[Index]; // R
				Pixel[3] = ShownAlpha[Index];
			}
		}
		SourceY += Dirty.Height();
	}

	FogTexture->UpdateTextureRegions(
		0,
		static_cast<uint32>(DirtyRegions.Num()),
		Regions,
		static_cast<uint32>(PitchWidth * 4),
		4,
		Pixels,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] SrcData;
			delete[] Regions;
		});
}
