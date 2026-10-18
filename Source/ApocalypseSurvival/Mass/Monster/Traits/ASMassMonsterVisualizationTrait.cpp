#include "Mass/Monster/Traits/ASMassMonsterVisualizationTrait.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "MassActorSubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "MassLODFragments.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationTypes.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterVisualizationTrait)

UASMassMonsterVisualizationTrait::UASMassMonsterVisualizationTrait()
{
	Params.LODRepresentation[EMassLOD::High] = EMassRepresentationType::StaticMeshInstance;
	Params.LODRepresentation[EMassLOD::Medium] = EMassRepresentationType::StaticMeshInstance;
	Params.LODRepresentation[EMassLOD::Low] = EMassRepresentationType::StaticMeshInstance;
	Params.LODRepresentation[EMassLOD::Off] = EMassRepresentationType::None;
	Params.bKeepLowResActors = false;
	Params.bKeepActorExtraFrame = false;

	LODParams.BaseLODDistance[EMassLOD::High] = 0.0f;
	LODParams.BaseLODDistance[EMassLOD::Medium] = 2500.0f;
	LODParams.BaseLODDistance[EMassLOD::Low] = 6000.0f;
	LODParams.BaseLODDistance[EMassLOD::Off] = 15000.0f;

	LODParams.VisibleLODDistance[EMassLOD::High] = 0.0f;
	LODParams.VisibleLODDistance[EMassLOD::Medium] = 3000.0f;
	LODParams.VisibleLODDistance[EMassLOD::Low] = 8000.0f;
	LODParams.VisibleLODDistance[EMassLOD::Off] = 15000.0f;

	LODParams.LODMaxCount[EMassLOD::High] = 100;
	LODParams.LODMaxCount[EMassLOD::Medium] = 300;
	LODParams.LODMaxCount[EMassLOD::Low] = 1000;
	LODParams.LODMaxCount[EMassLOD::Off] = 0;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc =
			StaticMeshInstanceDesc.Meshes.AddDefaulted_GetRef();
		MeshDesc.Mesh = CubeMeshFinder.Object;
		MeshDesc.LocalTransform = FTransform::Identity;
		MeshDesc.SetSignificanceRange(EMassLOD::High, EMassLOD::Max);
		MeshDesc.bCastShadows = false;
		MeshDesc.Mobility = EComponentMobility::Movable;
	}
}

void UASMassMonsterVisualizationTrait::BuildTemplate(
	FMassEntityTemplateBuildContext& BuildContext,
	const UWorld& World) const
{
	if (!bAllowServerSideVisualization && World.IsNetMode(NM_DedicatedServer)
		&& !BuildContext.IsInspectingData())
	{
		return;
	}

	// 일반 몬스터는 Actor로 표현하지 않지만 Representation 파이프라인이 빈 Actor Fragment를 요구한다.
	BuildContext.AddFragment<FMassActorFragment>();
	BuildContext.AddFragment<FMassViewerInfoFragment>();
	BuildContext.AddTag<FMassCollectLODViewerInfoTag>();

	Super::BuildTemplate(BuildContext, World);
}
