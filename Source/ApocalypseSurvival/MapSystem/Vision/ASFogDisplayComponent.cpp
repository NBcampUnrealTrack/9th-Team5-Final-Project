#include "MapSystem/Vision/ASFogDisplayComponent.h"

#include "Engine/PostProcessVolume.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
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
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	SetIsReplicatedByDefault(false);
}

bool UASFogDisplayComponent::HasLocalPlayer() const
{
	const UWorld* World = GetWorld();
	return GetNetMode() != NM_DedicatedServer && World && World->GetFirstLocalPlayerFromController() != nullptr;
}

void UASFogDisplayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!VisionDefinition || !HasLocalPlayer())
	{
		SetComponentTickEnabled(false);
		return;
	}

	const float FadeSeconds = VisionDefinition->Settings.FogFadeSeconds;
	const int32 MaxStep = FadeSeconds <= 0.0f ? 255
		: FMath::Max(1, FMath::CeilToInt(255.0f * FMath::Clamp(DeltaTime / FadeSeconds, 0.0f, 1.0f)));
	StepShownAlpha(MaxStep);
}

void UASFogDisplayComponent::StepShownAlpha(int32 MaxStep)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ASFogOfWar_FadeStep);

	TArray<FIntRect> DirtyRegions;
	for (int32 TileIndex = 0; TileIndex < PendingFadeTiles.Num(); ++TileIndex)
	{
		if (!PendingFadeTiles[TileIndex])
		{
			continue;
		}
		FIntRect Dirty;
		bool bReachedTarget;
		if (ASFogMask::StepToward(ShownAlpha, TargetAlpha, ResolutionX, ResolutionY, MaxStep,
			TileRegion(TileIndex), Dirty, bReachedTarget))
		{
			DirtyRegions.Add(Dirty);
		}
		if (bReachedTarget)
		{
			PendingFadeTiles[TileIndex] = 0;
		}
	}
	UploadTexture(DirtyRegions);
	if (!PendingFadeTiles.Contains(1))
	{
		SetComponentTickEnabled(false);
	}
}

void UASFogDisplayComponent::Configure(UASVisionDefinition* Definition)
{
	const FIntPoint WantedResolution = ASFogMask::ComputeResolution(Definition->GetFogWorldSize(), Definition->Settings.MaskResolution);
	if (Definition != VisionDefinition || WantedResolution.X != ResolutionX || WantedResolution.Y != ResolutionY
		|| Definition->FogWorldMin != ConfiguredFogWorldMin || Definition->FogWorldMax != ConfiguredFogWorldMax)
	{
		VisionDefinition = Definition;
		ResolutionX = WantedResolution.X;
		ResolutionY = WantedResolution.Y;
		ConfiguredFogWorldMin = Definition->FogWorldMin;
		ConfiguredFogWorldMax = Definition->FogWorldMax;

		const int32 CellCount = ResolutionX * ResolutionY;
		VisibleCells.Init(0, CellCount);
		PreviousVisibleCells.Init(0, CellCount);
		ExploredCells.Init(0, CellCount);
		RawAlpha.Init(0, CellCount);
		TargetAlpha.SetNumUninitialized(CellCount);
		ShownAlpha.SetNumUninitialized(CellCount);
		BlurScratch.Reset();
		TilesX = FMath::DivideAndRoundUp(ResolutionX, TileSize);
		TilesY = FMath::DivideAndRoundUp(ResolutionY, TileSize);
		PreviousVisibleTiles.Init(0, TilesX * TilesY);
		CurrentVisibleTiles.Init(0, TilesX * TilesY);
		PendingFadeTiles.Init(0, TilesX * TilesY);
		RawChangedRegions.Reset();
		RawChangedRegions.Reserve(TilesX * TilesY);
		OcclusionPoints.Reserve(512);
		bMaskDirty = true;
		bInitializeShownAlpha = true;
		SetComponentTickEnabled(false);

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
}

void UASFogDisplayComponent::UpdateWorldFog()
{
	UMaterialInterface* FogMaterial = VisionDefinition ? VisionDefinition->WorldFogMaterial.Get() : nullptr;
	if (!bApplyWorldFog || !FogMaterial || !FogTexture)
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

	// 시야 정의나 해상도가 바뀌면 텍스처를 다시 생성한다.
	WorldFogMaterial->SetTextureParameterValue(TEXT("FogTexture"), FogTexture);
	const FVector2D WorldSize = VisionDefinition->GetFogWorldSize();
	WorldFogMaterial->SetVectorParameterValue(TEXT("MapWorldMin"), FLinearColor(VisionDefinition->FogWorldMin.X, VisionDefinition->FogWorldMin.Y, 0.0f, 0.0f));
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
	// 로컬 플레이어가 죽거나 관전 중이어도 계속 갱신한다. 팀원은 여전히 시야를 공유한다.
	if (!Subsystem || !Manager || !Evaluator || !VisionDefinition || ResolutionX <= 0 || ResolutionY <= 0)
	{
		return;
	}

	const FASVisionSettings& Settings = VisionDefinition->Settings;
	const bool bWallOcclusion = Manager->IsWallOcclusionEnabled();
	const float NightAlpha = Manager->GetNightAlpha();
	const int32 LocalTeam = UASVisionFunctionLibrary::GetLocalTeamId(this);
	const FVector2D WorldSize = VisionDefinition->GetFogWorldSize();

	for (int32 TileIndex = 0; TileIndex < PreviousVisibleTiles.Num(); ++TileIndex)
	{
		if (!PreviousVisibleTiles[TileIndex])
		{
			continue;
		}
		const FIntRect Region = TileRegion(TileIndex);
		for (int32 Y = Region.Min.Y; Y < Region.Max.Y; ++Y)
		{
			FMemory::Memzero(VisibleCells.GetData() + Y * ResolutionX + Region.Min.X, Region.Width());
		}
	}

	for (const UASVisionSourceComponent* Source : Subsystem->GetVisionSources())
	{
		if (Source->GetTeamId() != LocalTeam || !Source->IsProvidingVision(VisionDefinition->DeadStateTag))
		{
			continue;
		}

		const AActor* SourceActor = Source->GetOwner();
		const FVector Origin = Evaluator->GetTraceLocation(SourceActor);
		const float Radius = Source->GetVisionRadius(Settings, NightAlpha);
		const FVector2D CenterGrid = WorldToGrid(Origin);

		if (!bWallOcclusion)
		{
			// U는 월드 Y를, V는 월드 X를 따른다.
			const FVector2D Radii(Radius / WorldSize.Y * ResolutionX, Radius / WorldSize.X * ResolutionY);
			RasterizeEllipse(CenterGrid, Radii);
			continue;
		}

		const int32 RayCount = FMath::Clamp(Settings.OcclusionRayCount, 16, 512);
		OcclusionPoints.Reset();

		for (int32 RayIndex = 0; RayIndex < RayCount; ++RayIndex)
		{
			const float Angle = 2.0f * UE_PI * static_cast<float>(RayIndex) / static_cast<float>(RayCount);
			const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
			const FVector End = Origin + Direction * Radius;

			FHitResult Hit;
			FVector EndPoint = End;
			if (Evaluator->TraceVisionWall(Hit, Origin, End, SourceActor))
			{
				const float Distance = FMath::Min(Radius, static_cast<float>(Hit.Distance) + Settings.WallRevealPadding);
				EndPoint = Origin + Direction * Distance;
			}

			OcclusionPoints.Add(WorldToGrid(EndPoint));
		}

		for (int32 Index = 0; Index < OcclusionPoints.Num(); ++Index)
		{
			RasterizeTriangle(CenterGrid, OcclusionPoints[Index], OcclusionPoints[(Index + 1) % OcclusionPoints.Num()]);
		}
	}

	RawChangedRegions.Reset();
	const uint8 Explored = static_cast<uint8>(FMath::Clamp(ExploredAlpha, 0, 255));
	const uint8 Unexplored = static_cast<uint8>(FMath::Clamp(UnexploredAlpha, 0, 255));
	for (int32 TileIndex = 0; TileIndex < PreviousVisibleTiles.Num(); ++TileIndex)
	{
		if (!bMaskDirty && !PreviousVisibleTiles[TileIndex] && !CurrentVisibleTiles[TileIndex])
		{
			continue;
		}
		const FIntRect Region = TileRegion(TileIndex);
		bool bVisibleChanged = false;
		for (int32 Y = Region.Min.Y; Y < Region.Max.Y; ++Y)
		{
			const int32 Start = Y * ResolutionX + Region.Min.X;
			bVisibleChanged |= FMemory::Memcmp(VisibleCells.GetData() + Start,
				PreviousVisibleCells.GetData() + Start, Region.Width()) != 0;
			if (CurrentVisibleTiles[TileIndex])
			{
				for (int32 X = Region.Min.X; X < Region.Max.X; ++X)
				{
					const int32 Index = Y * ResolutionX + X;
					ExploredCells[Index] |= VisibleCells[Index];
				}
			}
		}
		if (!bMaskDirty && !bVisibleChanged && !CurrentVisibleTiles[TileIndex])
		{
			continue;
		}
		// 게임 판정용 셀은 이진 상태를 유지하고 표시 알파만 부드럽게 만든다.
		int32 ChangedMinX = Region.Max.X;
		int32 ChangedMinY = Region.Max.Y;
		int32 ChangedMaxX = Region.Min.X;
		int32 ChangedMaxY = Region.Min.Y;
		for (int32 Y = Region.Min.Y; Y < Region.Max.Y; ++Y)
		{
			const int32 Start = Y * ResolutionX + Region.Min.X;
			if (bMaskDirty || bVisibleChanged)
			{
				FMemory::Memcpy(PreviousVisibleCells.GetData() + Start, VisibleCells.GetData() + Start, Region.Width());
			}
			for (int32 X = Region.Min.X; X < Region.Max.X; ++X)
			{
				const int32 Index = Y * ResolutionX + X;
				const uint8 Alpha = VisibleCells[Index] ? 0 : (ExploredCells[Index] ? Explored : Unexplored);
				if (!bMaskDirty && RawAlpha[Index] != Alpha)
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

	// 이번 경계는 변화가 없는 패스에서도 다음 지우기 영역이 된다.
	Swap(PreviousVisibleTiles, CurrentVisibleTiles);
	FMemory::Memzero(CurrentVisibleTiles.GetData(), CurrentVisibleTiles.Num());
	const bool bFullRecompute = bMaskDirty;
	bMaskDirty = false;
	if (!bFullRecompute && RawChangedRegions.IsEmpty())
	{
		return;
	}

	const int32 BlurRadius = FMath::Clamp(Settings.FogEdgeBlurTexels, 0, 8);
	if (bFullRecompute)
	{
		ASFogMask::BlurAlphaRegion(RawAlpha, TargetAlpha, ResolutionX, ResolutionY,
			BlurRadius, FIntRect(0, 0, ResolutionX, ResolutionY), BlurScratch);
		FMemory::Memset(PendingFadeTiles.GetData(), 1, PendingFadeTiles.Num());
	}
	else
	{
		for (const FIntRect& Changed : RawChangedRegions)
		{
			// 원본 변경 셀에서 반경만큼만 넓히면 두 흐림 패스의 영향 범위를 모두 포함한다.
			const FIntRect Region(FMath::Max(0, Changed.Min.X - BlurRadius), FMath::Max(0, Changed.Min.Y - BlurRadius),
				FMath::Min(ResolutionX, Changed.Max.X + BlurRadius), FMath::Min(ResolutionY, Changed.Max.Y + BlurRadius));
			ASFogMask::BlurAlphaRegion(RawAlpha, TargetAlpha, ResolutionX, ResolutionY,
				BlurRadius, Region, BlurScratch);
			for (int32 Y = Region.Min.Y / TileSize; Y <= (Region.Max.Y - 1) / TileSize; ++Y)
			{
				for (int32 X = Region.Min.X / TileSize; X <= (Region.Max.X - 1) / TileSize; ++X)
				{
					PendingFadeTiles[Y * TilesX + X] = 1;
				}
			}
		}
	}
	if (bInitializeShownAlpha)
	{
		// 초기 표시에는 검은 화면에서 시작하는 페이드를 적용하지 않는다.
		FMemory::Memcpy(ShownAlpha.GetData(), TargetAlpha.GetData(), TargetAlpha.Num());
		bInitializeShownAlpha = false;
		FMemory::Memzero(PendingFadeTiles.GetData(), PendingFadeTiles.Num());
		SetComponentTickEnabled(false);
		UploadTexture({ FIntRect(0, 0, ResolutionX, ResolutionY) });
	}
	else if (Settings.FogFadeSeconds <= 0.0f)
	{
		StepShownAlpha(255);
	}
	else
	{
		SetComponentTickEnabled(PendingFadeTiles.Contains(1));
	}
}

FVector2D UASFogDisplayComponent::WorldToGrid(const FVector& Location) const
{
	if (!VisionDefinition)
	{
		return FVector2D::ZeroVector;
	}
	const FVector2D UV = VisionDefinition->WorldToFogUV(Location);
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

void UASFogDisplayComponent::RasterizeEllipse(const FVector2D& Center, const FVector2D& Radii)
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
				VisibleCells[Y * ResolutionX + X] = 1;
			}
		}
	}
}

void UASFogDisplayComponent::RasterizeTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C)
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
			FMemory::Memset(VisibleCells.GetData() + Y * ResolutionX + SpanMinX, 1, SpanMaxX - SpanMinX + 1);
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
				Pixel[0] = 0; // B
				Pixel[1] = 0; // G
				Pixel[2] = 0; // R
				Pixel[3] = ShownAlpha[(Dirty.Min.Y + Y) * ResolutionX + Dirty.Min.X + X];
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
