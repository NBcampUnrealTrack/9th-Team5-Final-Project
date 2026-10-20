#include "Mass/Monster/TemporaryVisual/ASMassMonsterTemporaryVisualSubsystem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MassRepresentationSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterTemporaryVisualSubsystem)

namespace ASMassMonsterTemporaryVisual
{
	constexpr TCHAR ColorParameterName[] = TEXT("Color");

	FMassStaticMeshInstanceVisualizationMeshDesc MakeMeshDesc(
		UStaticMesh& Mesh,
		UMaterialInterface& Material)
	{
		FMassStaticMeshInstanceVisualizationMeshDesc MeshDesc;
		MeshDesc.Mesh = &Mesh;
		MeshDesc.MaterialOverrides.Add(&Material);
		MeshDesc.LocalTransform = FTransform::Identity;
		MeshDesc.SetSignificanceRange(EMassLOD::High, EMassLOD::Max);
		MeshDesc.bCastShadows = false;
		MeshDesc.Mobility = EComponentMobility::Movable;
		return MeshDesc;
	}
}

bool UASMassMonsterTemporaryVisualSubsystem::InitializeVisualDescriptors()
{
	return EnsureVisualDescriptors();
}

FStaticMeshInstanceVisualizationDescHandle UASMassMonsterTemporaryVisualSubsystem::GetVisualHandle(
	const EASMassMonsterTemporaryVisualState State)
{
	if (!DaySpawnHandle.IsValid() || !NightSpawnHandle.IsValid()
		|| !HitHandle.IsValid() || !DeadHandle.IsValid())
	{
		return FStaticMeshInstanceVisualizationDescHandle();
	}

	switch (State)
	{
	case EASMassMonsterTemporaryVisualState::NightSpawn: return NightSpawnHandle;
	case EASMassMonsterTemporaryVisualState::Hit: return HitHandle;
	case EASMassMonsterTemporaryVisualState::Dead: return DeadHandle;
	case EASMassMonsterTemporaryVisualState::DaySpawn:
	default: return DaySpawnHandle;
	}
}

bool UASMassMonsterTemporaryVisualSubsystem::EnsureVisualDescriptors()
{
	if (DaySpawnHandle.IsValid() && NightSpawnHandle.IsValid()
		&& HitHandle.IsValid() && DeadHandle.IsValid())
	{
		return true;
	}

	UWorld* World = GetWorld();
	UMassRepresentationSubsystem* RepresentationSubsystem = World
		? World->GetSubsystem<UMassRepresentationSubsystem>()
		: nullptr;
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!RepresentationSubsystem || !CubeMesh || !BaseMaterial)
	{
		return false;
	}

	auto CreateColorMaterial = [this, BaseMaterial](const FLinearColor Color)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (Material)
		{
			Material->SetVectorParameterValue(ASMassMonsterTemporaryVisual::ColorParameterName, Color);
		}
		return Material;
	};

	if (!DaySpawnMaterial)
	{
		DaySpawnMaterial = CreateColorMaterial(FLinearColor(1.0f, 0.8f, 0.0f, 1.0f));
		NightSpawnMaterial = CreateColorMaterial(FLinearColor(0.05f, 0.2f, 1.0f, 1.0f));
		HitMaterial = CreateColorMaterial(FLinearColor(1.0f, 0.03f, 0.03f, 1.0f));
		DeadMaterial = CreateColorMaterial(FLinearColor::Black);
	}

	auto Register = [RepresentationSubsystem, CubeMesh](UMaterialInstanceDynamic* Material)
	{
		if (!Material)
		{
			return FStaticMeshInstanceVisualizationDescHandle();
		}

		FStaticMeshInstanceVisualizationDesc Desc;
		Desc.Meshes.Add(ASMassMonsterTemporaryVisual::MakeMeshDesc(*CubeMesh, *Material));
		return RepresentationSubsystem->FindOrAddStaticMeshDesc(Desc);
	};

	DaySpawnHandle = Register(DaySpawnMaterial);
	NightSpawnHandle = Register(NightSpawnMaterial);
	HitHandle = Register(HitMaterial);
	DeadHandle = Register(DeadMaterial);
	return DaySpawnHandle.IsValid() && NightSpawnHandle.IsValid()
		&& HitHandle.IsValid() && DeadHandle.IsValid();
}
