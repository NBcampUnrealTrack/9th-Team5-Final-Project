#include "Mass/Monster/Traits/ASMassMonsterVisualizationTrait.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/TemporaryVisual/ASMassMonsterTemporaryVisualSubsystem.h"
#include "MassActorSubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "MassLODFragments.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationTypes.h"
#include "MassRepresentationSubsystem.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterVisualizationTrait)

UASMassMonsterVisualizationTrait::UASMassMonsterVisualizationTrait()
{
	// Visualization LOD Processor와 동일한 필터를 사용해야 Viewer 정보가 전달된다.
	LODParams.FilterTag = FASMassMonsterTag::StaticStruct();

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
		// Entity Transform은 NavMesh 지면 좌표를 유지한다. 표현 메시만 중심 원점의 Cube 높이 절반만큼 올린다.
		// 실제 모델은 Entity Config의 Visualization Trait에서 메시 Pivot/바운드에 맞는 Local Transform을 지정한다.
		MeshDesc.LocalTransform = FTransform(FVector(0.0f, 0.0f, 50.0f));
		MeshDesc.SetSignificanceRange(EMassLOD::High, EMassLOD::Max);
		MeshDesc.bCastShadows = false;
		MeshDesc.Mobility = EComponentMobility::Movable;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[MassMonsterVisual] Required cube mesh is missing."));
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

	// 런타임 Processor가 ISM 배열을 순회하는 도중 Descriptor를 추가하면 MassRepresentation의
	// MTAccessDetector와 충돌한다. Entity Config Template 생성 시점에 상태 Descriptor를 미리 등록한다.
	if (!BuildContext.IsInspectingData() && StaticMeshInstanceDesc.IsValid())
	{
		if (UMassRepresentationSubsystem* RepresentationSubsystem = World.GetSubsystem<UMassRepresentationSubsystem>())
		{
			const FStaticMeshInstanceVisualizationDescHandle BaseHandle =
				RepresentationSubsystem->FindOrAddStaticMeshDesc(StaticMeshInstanceDesc);
			if (UASMassMonsterTemporaryVisualSubsystem* VisualSubsystem =
				World.GetSubsystem<UASMassMonsterTemporaryVisualSubsystem>())
			{
				VisualSubsystem->RegisterStateVisuals(StaticMeshInstanceDesc, BaseHandle);
			}
		}
	}

	Super::BuildTemplate(BuildContext, World);
	BuildContext.RequireTag<FASMassMonsterTag>();
}
