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
	FIntRect Dirty;
	if (ASFogMask::StepToward(ShownAlpha, TargetAlpha, ResolutionX, ResolutionY, MaxStep, Dirty))
	{
		UploadTexture(Dirty);
	}
	if (ShownAlpha == TargetAlpha)
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
		TargetAlpha.SetNumUninitialized(CellCount);
		ShownAlpha.SetNumUninitialized(CellCount);
		BlurScratch.SetNumUninitialized(CellCount);
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
	const int32 LocalTeam = UASVisionFunctionLibrary::GetLocalTeamId(this);
	const FVector2D WorldSize = VisionDefinition->GetFogWorldSize();

	FMemory::Memzero(VisibleCells.GetData(), VisibleCells.Num());

	for (const UASVisionSourceComponent* Source : Subsystem->GetVisionSources())
	{
		if (Source->GetTeamId() != LocalTeam || !Source->IsProvidingVision(VisionDefinition->DeadStateTag))
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

	for (int32 Index = 0; Index < VisibleCells.Num(); ++Index)
	{
		ExploredCells[Index] |= VisibleCells[Index];
	}

	if (bMaskDirty || FMemory::Memcmp(VisibleCells.GetData(), PreviousVisibleCells.GetData(), VisibleCells.Num()) != 0)
	{
		FMemory::Memcpy(PreviousVisibleCells.GetData(), VisibleCells.GetData(), VisibleCells.Num());
		bMaskDirty = false;
		// 게임 판정용 셀은 이진 상태를 유지하고 표시 알파만 부드럽게 만든다.
		const uint8 Explored = static_cast<uint8>(FMath::Clamp(ExploredAlpha, 0, 255));
		const uint8 Unexplored = static_cast<uint8>(FMath::Clamp(UnexploredAlpha, 0, 255));
		for (int32 Index = 0; Index < VisibleCells.Num(); ++Index)
		{
			TargetAlpha[Index] = VisibleCells[Index] ? 0 : (ExploredCells[Index] ? Explored : Unexplored);
		}
		ASFogMask::BlurAlpha(TargetAlpha, ResolutionX, ResolutionY, Settings.FogEdgeBlurTexels, BlurScratch);
		if (bInitializeShownAlpha)
		{
			// 초기 표시에는 검은 화면에서 시작하는 페이드를 적용하지 않는다.
			FMemory::Memcpy(ShownAlpha.GetData(), TargetAlpha.GetData(), TargetAlpha.Num());
			bInitializeShownAlpha = false;
			SetComponentTickEnabled(false);
			UploadTexture(FIntRect(0, 0, ResolutionX, ResolutionY));
		}
		else if (Settings.FogFadeSeconds <= 0.0f)
		{
			StepShownAlpha(255);
		}
		else
		{
			SetComponentTickEnabled(ShownAlpha != TargetAlpha);
		}
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
				VisibleCells[Y * ResolutionX + X] = 1;
			}
		}
	}
}

void UASFogDisplayComponent::UploadTexture(const FIntRect& Dirty)
{
	if (!FogTexture || ResolutionX <= 0 || ResolutionY <= 0 || Dirty.Width() <= 0 || Dirty.Height() <= 0)
	{
		return;
	}

	// 변경 사각형만 별도 버퍼로 복사하고 렌더 스레드의 완료 콜백에서 해제한다.
	const int32 Width = Dirty.Width();
	const int32 Height = Dirty.Height();
	uint8* Pixels = new uint8[Width * Height * 4];
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			uint8* Pixel = Pixels + (Y * Width + X) * 4;
			Pixel[0] = 0; // B
			Pixel[1] = 0; // G
			Pixel[2] = 0; // R
			Pixel[3] = ShownAlpha[(Dirty.Min.Y + Y) * ResolutionX + Dirty.Min.X + X];
		}
	}

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(Dirty.Min.X, Dirty.Min.Y, 0, 0, Width, Height);
	FogTexture->UpdateTextureRegions(
		0,
		1,
		Region,
		static_cast<uint32>(Width * 4),
		4,
		Pixels,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] SrcData;
			delete Regions;
		});
}
