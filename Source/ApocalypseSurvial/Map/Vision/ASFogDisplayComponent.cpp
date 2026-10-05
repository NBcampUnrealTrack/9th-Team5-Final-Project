#include "Map/Vision/ASFogDisplayComponent.h"

#include "Engine/PostProcessVolume.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Map/Core/ASMapDefinition.h"
#include "Map/Core/ASMapFunctionLibrary.h"
#include "Map/Core/ASMapStateComponent.h"
#include "Map/Vision/ASFogOfWarManager.h"
#include "Map/Vision/ASVisionEvaluatorComponent.h"
#include "Map/Vision/ASVisionFunctionLibrary.h"
#include "Map/Vision/ASVisionSourceComponent.h"
#include "Map/Vision/ASVisionWorldSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "RHI.h"

UASFogDisplayComponent::UASFogDisplayComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UASFogDisplayComponent::Configure(UASMapDefinition* Definition)
{
	const int32 WantedResolution = FMath::Clamp(Definition->Vision.MaskResolution, 64, 2048);
	if (Definition != MapDefinition || WantedResolution != Resolution)
	{
		MapDefinition = Definition;
		Resolution = WantedResolution;

		const int32 CellCount = Resolution * Resolution;
		VisibleCells.Init(0, CellCount);
		PreviousVisibleCells.Init(0, CellCount);
		ExploredCells.Init(0, CellCount);
		bMaskDirty = true;

		if (GetNetMode() != NM_DedicatedServer)
		{
			FogTexture = UTexture2D::CreateTransient(Resolution, Resolution, PF_B8G8R8A8);
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
	UMaterialInterface* FogMaterial = MapDefinition ? MapDefinition->WorldFogMaterial.Get() : nullptr;
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

	// 맵 정의나 해상도가 바뀌면 텍스처를 다시 생성한다.
	WorldFogMaterial->SetTextureParameterValue(TEXT("FogTexture"), FogTexture);
	const FVector2D WorldSize = MapDefinition->GetMapWorldSize();
	WorldFogMaterial->SetVectorParameterValue(TEXT("MapWorldMin"), FLinearColor(MapDefinition->MapWorldMin.X, MapDefinition->MapWorldMin.Y, 0.0f, 0.0f));
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

	const UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this);
	const UASMapStateComponent* MapState = UASMapFunctionLibrary::GetMapStateComponent(this);
	const AASFogOfWarManager* Manager = Cast<AASFogOfWarManager>(GetOwner());
	const UASVisionEvaluatorComponent* Evaluator = Manager ? Manager->GetVisionEvaluator() : nullptr;
	// 로컬 플레이어가 죽거나 관전 중이어도 계속 갱신한다. 팀원은 여전히 시야를 공유한다.
	if (!Subsystem || !MapState || !Evaluator || Resolution <= 0)
	{
		return;
	}

	const FASVisionSettings& Settings = MapDefinition->Vision;
	const bool bWallOcclusion = MapState->IsWallOcclusionEnabled();
	const int32 LocalTeam = UASVisionFunctionLibrary::GetLocalTeamId(this);
	const FVector2D WorldSize = MapDefinition->GetMapWorldSize();

	FMemory::Memzero(VisibleCells.GetData(), VisibleCells.Num());

	for (const UASVisionSourceComponent* Source : Subsystem->GetVisionSources())
	{
		if (Source->GetTeamId() != LocalTeam || !Source->IsProvidingVision(MapDefinition->DeadStateTag))
		{
			continue;
		}

		const AActor* SourceActor = Source->GetOwner();
		const FVector Origin = Evaluator->GetTraceLocation(SourceActor);
		const float Radius = Source->GetVisionRadius(Settings.DefaultVisionRadius);
		const FVector2D CenterGrid = WorldToGrid(Origin);

		if (!bWallOcclusion)
		{
			// U는 월드 Y를, V는 월드 X를 따른다.
			const FVector2D Radii(Radius / WorldSize.Y * Resolution, Radius / WorldSize.X * Resolution);
			RasterizeEllipse(CenterGrid, Radii);
			continue;
		}

		const int32 RayCount = FMath::Clamp(Settings.OcclusionRayCount, 16, 512);
		TArray<FVector2D> Points;
		Points.Reserve(RayCount);

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

			Points.Add(WorldToGrid(EndPoint));
		}

		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			RasterizeTriangle(CenterGrid, Points[Index], Points[(Index + 1) % Points.Num()]);
		}
	}

	for (int32 Index = 0; Index < VisibleCells.Num(); ++Index)
	{
		ExploredCells[Index] |= VisibleCells[Index];
	}

	if (bMaskDirty || FMemory::Memcmp(VisibleCells.GetData(), PreviousVisibleCells.GetData(), VisibleCells.Num()) != 0)
	{
		FMemory::Memcpy(PreviousVisibleCells.GetData(), VisibleCells.GetData(), VisibleCells.Num());
		bMaskDirty = false;
		UploadTexture();
	}
}

FVector2D UASFogDisplayComponent::WorldToGrid(const FVector& Location) const
{
	return MapDefinition ? MapDefinition->WorldToMapUV(Location) * static_cast<double>(Resolution) : FVector2D::ZeroVector;
}

int32 UASFogDisplayComponent::GridIndexAt(const FVector& Location) const
{
	if (Resolution <= 0)
	{
		return INDEX_NONE;
	}

	const FVector2D Grid = WorldToGrid(Location);
	const int32 X = FMath::FloorToInt(Grid.X);
	const int32 Y = FMath::FloorToInt(Grid.Y);
	if (X < 0 || Y < 0 || X >= Resolution || Y >= Resolution)
	{
		return INDEX_NONE;
	}

	return Y * Resolution + X;
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

void UASFogDisplayComponent::RasterizeEllipse(const FVector2D& Center, const FVector2D& Radii)
{
	if (Radii.X <= 0.0 || Radii.Y <= 0.0)
	{
		return;
	}

	const int32 MinX = FMath::Max(0, FMath::FloorToInt(Center.X - Radii.X));
	const int32 MaxX = FMath::Min(Resolution - 1, FMath::CeilToInt(Center.X + Radii.X));
	const int32 MinY = FMath::Max(0, FMath::FloorToInt(Center.Y - Radii.Y));
	const int32 MaxY = FMath::Min(Resolution - 1, FMath::CeilToInt(Center.Y + Radii.Y));

	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		const double DY = (Y + 0.5 - Center.Y) / Radii.Y;
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const double DX = (X + 0.5 - Center.X) / Radii.X;
			if (DX * DX + DY * DY <= 1.0)
			{
				VisibleCells[Y * Resolution + X] = 1;
			}
		}
	}
}

void UASFogDisplayComponent::RasterizeTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C)
{
	const int32 MinX = FMath::Max(0, FMath::FloorToInt(FMath::Min3(A.X, B.X, C.X)));
	const int32 MaxX = FMath::Min(Resolution - 1, FMath::CeilToInt(FMath::Max3(A.X, B.X, C.X)));
	const int32 MinY = FMath::Max(0, FMath::FloorToInt(FMath::Min3(A.Y, B.Y, C.Y)));
	const int32 MaxY = FMath::Min(Resolution - 1, FMath::CeilToInt(FMath::Max3(A.Y, B.Y, C.Y)));
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

	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const FVector2D P(X + 0.5, Y + 0.5);
			const double W0 = Edge(B, C, P);
			const double W1 = Edge(C, A, P);
			const double W2 = Edge(A, B, P);

			const bool bInside = Area > 0.0
				? (W0 >= 0.0 && W1 >= 0.0 && W2 >= 0.0)
				: (W0 <= 0.0 && W1 <= 0.0 && W2 <= 0.0);

			if (bInside)
			{
				VisibleCells[Y * Resolution + X] = 1;
			}
		}
	}
}

void UASFogDisplayComponent::UploadTexture()
{
	if (!FogTexture || Resolution <= 0)
	{
		return;
	}

	const int32 CellCount = Resolution * Resolution;
	uint8* Pixels = new uint8[CellCount * 4];
	const uint8 Explored = static_cast<uint8>(FMath::Clamp(ExploredAlpha, 0, 255));
	const uint8 Unexplored = static_cast<uint8>(FMath::Clamp(UnexploredAlpha, 0, 255));

	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		uint8* Pixel = Pixels + Index * 4;
		Pixel[0] = 0; // B
		Pixel[1] = 0; // G
		Pixel[2] = 0; // R
		Pixel[3] = VisibleCells[Index] ? 0 : (ExploredCells[Index] ? Explored : Unexplored);
	}

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Resolution, Resolution);
	FogTexture->UpdateTextureRegions(
		0,
		1,
		Region,
		static_cast<uint32>(Resolution * 4),
		4,
		Pixels,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] SrcData;
			delete Regions;
		});
}
