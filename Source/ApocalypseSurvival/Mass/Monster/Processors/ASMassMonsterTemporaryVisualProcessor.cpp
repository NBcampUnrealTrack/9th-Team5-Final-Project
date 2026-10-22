#include "Mass/Monster/Processors/ASMassMonsterTemporaryVisualProcessor.h"

#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "Mass/Monster/Data/ASMassMonsterTags.h"
#include "Mass/Monster/Processors/ASMassMonsterVisualizationProcessors.h"
#include "Mass/Monster/TemporaryVisual/ASMassMonsterTemporaryVisualFragment.h"
#include "Mass/Monster/TemporaryVisual/ASMassMonsterTemporaryVisualSubsystem.h"
#include "Engine/World.h"
#include "MassExecutionContext.h"
#include "MassLODFragments.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterTemporaryVisualProcessor)

UASMassMonsterTemporaryVisualProcessor::UASMassMonsterTemporaryVisualProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteAfter.Add(UASMassMonsterVisualizationProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UASMassMonsterUpdateISMProcessor::StaticClass()->GetFName());
}

void UASMassMonsterTemporaryVisualProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FASMassMonsterHealthFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FASMassMonsterTemporaryVisualFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.SetChunkFilter(&FMassVisualizationChunkFragment::AreAnyEntitiesVisibleInChunk);
	EntityQuery.AddSharedRequirement<FMassRepresentationSubsystemSharedFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UMassRepresentationSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FASMassMonsterTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassStaticRepresentationTag>(EMassFragmentPresence::None);
}

void UASMassMonsterTemporaryVisualProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	UWorld* World = GetWorld();
	UASMassMonsterTemporaryVisualSubsystem* VisualSubsystem = World
		? World->GetSubsystem<UASMassMonsterTemporaryVisualSubsystem>()
		: nullptr;
	if (!VisualSubsystem)
	{
		return;
	}

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	EntityQuery.ForEachEntityChunk(Context, [VisualSubsystem, DeltaTime](FMassExecutionContext& Context)
	{
		UMassRepresentationSubsystem* RepresentationSubsystem =
			Context.GetMutableSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
		if (!RepresentationSubsystem)
		{
			return;
		}

		FMassInstancedStaticMeshInfoArrayView ISMInfos = RepresentationSubsystem->GetMutableInstancedStaticMeshInfos();
		const TConstArrayView<FASMassMonsterHealthFragment> Healths =
			Context.GetFragmentView<FASMassMonsterHealthFragment>();
		const TArrayView<FASMassMonsterTemporaryVisualFragment> Visuals =
			Context.GetMutableFragmentView<FASMassMonsterTemporaryVisualFragment>();
		const TArrayView<FMassRepresentationFragment> Representations =
			Context.GetMutableFragmentView<FMassRepresentationFragment>();
		const TConstArrayView<FMassRepresentationLODFragment> LODs =
			Context.GetFragmentView<FMassRepresentationLODFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterTemporaryVisualFragment& Visual = Visuals[EntityIt];
			EASMassMonsterTemporaryVisualState DesiredState;
			if (Healths[EntityIt].CurrentHealth <= 0.0f)
			{
				DesiredState = EASMassMonsterTemporaryVisualState::Dead;
			}
			else if (Visual.HitFlashTimeRemaining > 0.0f)
			{
				DesiredState = EASMassMonsterTemporaryVisualState::Hit;
				Visual.HitFlashTimeRemaining = FMath::Max(0.0f, Visual.HitFlashTimeRemaining - DeltaTime);
			}
			else
			{
				DesiredState = Visual.SpawnWorldBehavior == EASMassMonsterWorldBehavior::NightAssault
					? EASMassMonsterTemporaryVisualState::NightSpawn
					: EASMassMonsterTemporaryVisualState::DaySpawn;
			}

			FMassRepresentationFragment& Representation = Representations[EntityIt];
			// 첫 프레임의 Descriptor는 Entity Config Visualization Trait가 등록한 원본이다.
			// Hit/Dead Descriptor로 교체된 뒤에도 이 Handle을 유지해 다중 메시 외형으로 복구한다.
			if (!Visual.BaseVisualHandle.IsValid())
			{
				Visual.BaseVisualHandle = Representation.StaticMeshDescHandle;
			}
			if (!Visual.BaseVisualHandle.IsValid())
			{
				continue;
			}

			const FStaticMeshInstanceVisualizationDescHandle NewHandle = VisualSubsystem->GetVisualHandle(
				DesiredState,
				Visual.BaseVisualHandle);
			if (!NewHandle.IsValid())
			{
				continue;
			}

			if (Visual.bHasAppliedState && Visual.AppliedState == DesiredState
				&& Representation.StaticMeshDescHandle == NewHandle)
			{
				continue;
			}

			// 현재 Descriptor와 요청 Descriptor가 같으면 Instance를 재등록할 필요가 없다.
			if (Representation.StaticMeshDescHandle == NewHandle)
			{
				Visual.AppliedState = DesiredState;
				Visual.bHasAppliedState = true;
				continue;
			}

			const FMassRepresentationLODFragment& LOD = LODs[EntityIt];
			const FMassEntityHandle Entity = Context.GetEntity(EntityIt);
			if (Representation.CurrentRepresentation == EMassRepresentationType::StaticMeshInstance)
			{
				const int32 PreviousInfoIndex = Representation.StaticMeshDescHandle.ToIndex();
				if (ISMInfos.IsValidIndex(PreviousInfoIndex))
				{
					ISMInfos[PreviousInfoIndex].RemoveInstance(Entity, LOD.LODSignificance);
					if (Representation.PrevLODSignificance >= 0.0f
						&& Representation.PrevLODSignificance != LOD.LODSignificance)
					{
						ISMInfos[PreviousInfoIndex].RemoveInstance(Entity, Representation.PrevLODSignificance);
					}
				}
			}

			Representation.StaticMeshDescHandle = NewHandle;
			// Descriptor만 교체한 채 StaticMeshInstance 상태를 유지하면 UpdateISM이 새 Descriptor에
			// 기존 Instance가 있다고 가정할 수 있다. 그 결과 이전 색상 ISM이 남아 겹쳐 렌더링된다.
			// 한 프레임 Representation을 None으로 돌려 다음 Visualization 단계가 새 Descriptor에
			// Instance를 처음부터 등록하도록 강제한다.
			Representation.CurrentRepresentation = EMassRepresentationType::None;
			Representation.PrevLODSignificance = -1.0f;
			Visual.AppliedState = DesiredState;
			Visual.bHasAppliedState = true;
		}
	});
}
