#include "Mass/Monster/Subsystems/ASMassMonsterFogMaterialSubsystem.h"

#include "ApocalypseSurvival.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MapSystem/Vision/ASFogDisplayComponent.h"
#include "MapSystem/Vision/ASFogOfWarManager.h"
#include "MassRepresentationSubsystem.h"
#include "MassVisualizationComponent.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterFogMaterialSubsystem)

namespace ASMassMonsterFogMaterial
{
	constexpr TCHAR FogTextureParameter[] = TEXT("FogTexture");
	constexpr TCHAR MapWorldMinParameter[] = TEXT("MapWorldMin");
	constexpr TCHAR MapWorldSizeParameter[] = TEXT("MapWorldSize");
	constexpr TCHAR LocalVisionCenterParameter[] = TEXT("LocalVisionCenter");
	constexpr TCHAR LocalVisionRadiusParameter[] = TEXT("LocalVisionRadius");
	constexpr TCHAR LocalVisionSoftnessParameter[] = TEXT("LocalVisionSoftness");
	constexpr TCHAR FogEnabledParameter[] = TEXT("FogEnabled");
	constexpr TCHAR FogMaskReadyParameter[] = TEXT("FogMaskReady");
	constexpr TCHAR MassMonsterMaterialPath[] = TEXT("/Game/Mass/Monster/Materials/");
	constexpr uint64 ScanIntervalFrames = 30;
}

bool UASMassMonsterFogMaterialSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UASMassMonsterFogMaterialSubsystem::Refresh(const AASFogOfWarManager* FogManager)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	FASFogMaterialState FogState;
	const bool bNewFogEnabled = FogManager && FogManager->GetVisionDefinition();
	if (bNewFogEnabled)
	{
		FogState = FogManager->GetFogMaterialState();
	}
	const bool bNewFogMaskReady = bNewFogEnabled && FogState.bHasLocalVisionMask;
	const bool bStateChanged = !bHasCachedState
		|| CurrentFogTexture != FogState.FogTexture
		|| CurrentMapWorldMin != FogState.MapWorldMin
		|| CurrentMapWorldSize != FogState.MapWorldSize
		|| CurrentLocalVisionCenter != FogState.LocalVisionCenter
		|| !FMath::IsNearlyEqual(CurrentLocalVisionRadius, FogState.LocalVisionRadius)
		|| !FMath::IsNearlyEqual(CurrentLocalVisionSoftness, FogState.LocalVisionSoftness)
		|| bFogEnabled != bNewFogEnabled
		|| bFogMaskReady != bNewFogMaskReady;

	CurrentFogTexture = FogState.FogTexture;
	CurrentMapWorldMin = FogState.MapWorldMin;
	CurrentMapWorldSize = FogState.MapWorldSize;
	CurrentLocalVisionCenter = FogState.LocalVisionCenter;
	CurrentLocalVisionRadius = FogState.LocalVisionRadius;
	CurrentLocalVisionSoftness = FogState.LocalVisionSoftness;
	bFogEnabled = bNewFogEnabled;
	bFogMaskReady = bNewFogMaskReady;
	bHasCachedState = true;

	if (bStateChanged)
	{
		for (const TPair<TObjectPtr<UMaterialInterface>, TObjectPtr<UMaterialInstanceDynamic>>& Pair : FogMaterialInstances)
		{
			if (Pair.Value)
			{
				ApplyFogParameters(*Pair.Value);
			}
		}
	}

	if (bStateChanged || GFrameCounter >= LastScanFrame + ASMassMonsterFogMaterial::ScanIntervalFrames)
	{
		LastScanFrame = GFrameCounter;
		ScanMassVisualizerMaterials();
	}
}

bool UASMassMonsterFogMaterialSubsystem::IsMassMonsterMaterial(const UMaterialInterface* Material) const
{
	for (const UMaterialInterface* Current = Material; Current; )
	{
		if (Current->GetPathName().StartsWith(ASMassMonsterFogMaterial::MassMonsterMaterialPath))
		{
			return true;
		}

		const UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Current);
		Current = MaterialInstance ? MaterialInstance->Parent : nullptr;
	}
	return false;
}

UMaterialInstanceDynamic* UASMassMonsterFogMaterialSubsystem::FindOrCreateFogMaterial(UMaterialInterface* Material)
{
	if (!Material)
	{
		return nullptr;
	}

	if (const TObjectPtr<UMaterialInstanceDynamic>* Existing = FogMaterialInstances.Find(Material))
	{
		return *Existing;
	}
	for (const TPair<TObjectPtr<UMaterialInterface>, TObjectPtr<UMaterialInstanceDynamic>>& Pair : FogMaterialInstances)
	{
		if (Pair.Value == Material)
		{
			return Pair.Value;
		}
	}

	// 다른 시각 효과가 이미 만든 MID를 다시 MID의 Parent로 사용하면 엔진이 Parent를 거부한다.
	// 기존 MID는 그대로 공유하고 Fog 파라미터만 추가로 설정한다.
	if (UMaterialInstanceDynamic* ExistingDynamic = Cast<UMaterialInstanceDynamic>(Material))
	{
		FogMaterialInstances.Add(Material, ExistingDynamic);
		ApplyFogParameters(*ExistingDynamic);
		return ExistingDynamic;
	}

	UMaterialInstanceDynamic* FogMaterial = UMaterialInstanceDynamic::Create(Material, this);
	if (FogMaterial)
	{
		FogMaterialInstances.Add(Material, FogMaterial);
		ApplyFogParameters(*FogMaterial);
	}
	return FogMaterial;
}

void UASMassMonsterFogMaterialSubsystem::ApplyFogParameters(UMaterialInstanceDynamic& Material) const
{
	Material.SetTextureParameterValue(ASMassMonsterFogMaterial::FogTextureParameter, CurrentFogTexture);
	Material.SetVectorParameterValue(ASMassMonsterFogMaterial::MapWorldMinParameter,
		FLinearColor(CurrentMapWorldMin.X, CurrentMapWorldMin.Y, 0.0f, 0.0f));
	Material.SetVectorParameterValue(ASMassMonsterFogMaterial::MapWorldSizeParameter,
		FLinearColor(CurrentMapWorldSize.X, CurrentMapWorldSize.Y, 0.0f, 0.0f));
	Material.SetVectorParameterValue(ASMassMonsterFogMaterial::LocalVisionCenterParameter,
		FLinearColor(CurrentLocalVisionCenter.X, CurrentLocalVisionCenter.Y, 0.0f, 0.0f));
	Material.SetScalarParameterValue(ASMassMonsterFogMaterial::LocalVisionRadiusParameter, CurrentLocalVisionRadius);
	Material.SetScalarParameterValue(ASMassMonsterFogMaterial::LocalVisionSoftnessParameter, CurrentLocalVisionSoftness);
	Material.SetScalarParameterValue(ASMassMonsterFogMaterial::FogEnabledParameter, bFogEnabled ? 1.0f : 0.0f);
	Material.SetScalarParameterValue(ASMassMonsterFogMaterial::FogMaskReadyParameter, bFogMaskReady ? 1.0f : 0.0f);
}

void UASMassMonsterFogMaterialSubsystem::ScanMassVisualizerMaterials()
{
	UWorld* World = GetWorld();
	UMassRepresentationSubsystem* RepresentationSubsystem = World
		? World->GetSubsystem<UMassRepresentationSubsystem>()
		: nullptr;
	UMassVisualizationComponent* VisualizationComponent = RepresentationSubsystem
		? RepresentationSubsystem->GetVisualizationComponent()
		: nullptr;
	AActor* VisualizerActor = VisualizationComponent ? VisualizationComponent->GetOwner() : nullptr;
	if (!VisualizerActor)
	{
		return;
	}

	TInlineComponentArray<UInstancedStaticMeshComponent*> ISMComponents(VisualizerActor);
	int32 BoundMaterialSlotCount = 0;
	for (UInstancedStaticMeshComponent* ISMComponent : ISMComponents)
	{
		if (!ISMComponent)
		{
			continue;
		}

		for (int32 MaterialIndex = 0; MaterialIndex < ISMComponent->GetNumMaterials(); ++MaterialIndex)
		{
			UMaterialInterface* Material = ISMComponent->GetMaterial(MaterialIndex);
			if (!IsMassMonsterMaterial(Material))
			{
				continue;
			}

			if (UMaterialInstanceDynamic* FogMaterial = FindOrCreateFogMaterial(Material))
			{
				if (ISMComponent->GetMaterial(MaterialIndex) != FogMaterial)
				{
					ISMComponent->SetMaterial(MaterialIndex, FogMaterial);
				}
				++BoundMaterialSlotCount;
			}
		}
	}

	if (BoundMaterialSlotCount != LastBoundMaterialSlotCount)
	{
		LastBoundMaterialSlotCount = BoundMaterialSlotCount;
		UE_LOG(LogApocalypseSurvival, Display,
			TEXT("[MassMonsterFog] Enabled=%d MaskReady=%d Texture=%s BoundMaterialSlots=%d Center=(%.1f,%.1f) Radius=%.1f Softness=%.1f"),
			bFogEnabled ? 1 : 0,
			bFogMaskReady ? 1 : 0,
			*GetNameSafe(CurrentFogTexture),
			BoundMaterialSlotCount,
			CurrentLocalVisionCenter.X,
			CurrentLocalVisionCenter.Y,
			CurrentLocalVisionRadius,
			CurrentLocalVisionSoftness);
	}
}
