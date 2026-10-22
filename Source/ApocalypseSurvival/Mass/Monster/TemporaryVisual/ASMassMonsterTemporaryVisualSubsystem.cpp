#include "Mass/Monster/TemporaryVisual/ASMassMonsterTemporaryVisualSubsystem.h"

#include "Engine/StaticMesh.h"
#include "MassRepresentationSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterTemporaryVisualSubsystem)

namespace ASMassMonsterTemporaryVisual
{
	constexpr TCHAR ColorParameterName[] = TEXT("Color");
}

void UASMassMonsterTemporaryVisualSubsystem::RegisterStateVisuals(
	const FStaticMeshInstanceVisualizationDesc& BaseDesc,
	const FStaticMeshInstanceVisualizationDescHandle BaseHandle)
{
	if (!BaseHandle.IsValid())
	{
		return;
	}

	FStateVisualHandles& Handles = StateVisualHandlesByBaseHandle.FindOrAdd(BaseHandle.ToIndex());
	if (!Handles.Hit.IsValid())
	{
		Handles.Hit = CreateStateVisual(BaseDesc, FLinearColor(1.0f, 0.03f, 0.03f, 1.0f));
	}
	if (!Handles.Dead.IsValid())
	{
		Handles.Dead = CreateStateVisual(BaseDesc, FLinearColor::Black);
	}
}

FStaticMeshInstanceVisualizationDescHandle UASMassMonsterTemporaryVisualSubsystem::GetVisualHandle(
	const EASMassMonsterTemporaryVisualState State,
	const FStaticMeshInstanceVisualizationDescHandle BaseHandle)
{
	// 낮/밤은 행동 상태일 뿐, Entity Config에 설정된 기본 외형은 바꾸지 않는다.
	if (State == EASMassMonsterTemporaryVisualState::DaySpawn
		|| State == EASMassMonsterTemporaryVisualState::NightSpawn)
	{
		return BaseHandle;
	}

	if (!BaseHandle.IsValid())
	{
		return FStaticMeshInstanceVisualizationDescHandle();
	}

	const FStateVisualHandles* Handles = StateVisualHandlesByBaseHandle.Find(BaseHandle.ToIndex());
	if (!Handles)
	{
		return FStaticMeshInstanceVisualizationDescHandle();
	}

	return State == EASMassMonsterTemporaryVisualState::Hit ? Handles->Hit : Handles->Dead;
}

FStaticMeshInstanceVisualizationDescHandle UASMassMonsterTemporaryVisualSubsystem::CreateStateVisual(
	const FStaticMeshInstanceVisualizationDesc& BaseDesc,
	const FLinearColor& Color)
{
	UMassRepresentationSubsystem* RepresentationSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<UMassRepresentationSubsystem>()
		: nullptr;
	if (!RepresentationSubsystem)
	{
		return FStaticMeshInstanceVisualizationDescHandle();
	}

	// Descriptor를 복사하므로 다중 메시, 상대 위치, 회전, 스케일, LOD 범위를 그대로 보존한다.
	FStaticMeshInstanceVisualizationDesc StateDesc = BaseDesc;
	for (FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc : StateDesc.Meshes)
	{
		if (!MeshDesc.Mesh)
		{
			continue;
		}

		const int32 MaterialSlotCount = MeshDesc.Mesh->GetStaticMaterials().Num();
		const int32 OverrideCount = FMath::Max(MaterialSlotCount, MeshDesc.MaterialOverrides.Num());
		MeshDesc.MaterialOverrides.SetNum(OverrideCount);

		for (int32 MaterialIndex = 0; MaterialIndex < OverrideCount; ++MaterialIndex)
		{
			UMaterialInterface* BaseMaterial = MeshDesc.MaterialOverrides[MaterialIndex];
			if (!BaseMaterial && MaterialIndex < MaterialSlotCount)
			{
				BaseMaterial = MeshDesc.Mesh->GetMaterial(MaterialIndex);
			}
			if (!BaseMaterial)
			{
				continue;
			}

			UMaterialInstanceDynamic* StateMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			if (StateMaterial)
			{
				StateMaterial->SetVectorParameterValue(ASMassMonsterTemporaryVisual::ColorParameterName, Color);
				StateMaterials.Add(StateMaterial);
				MeshDesc.MaterialOverrides[MaterialIndex] = StateMaterial;
			}
		}
	}

	return RepresentationSubsystem->FindOrAddStaticMeshDesc(StateDesc);
}
