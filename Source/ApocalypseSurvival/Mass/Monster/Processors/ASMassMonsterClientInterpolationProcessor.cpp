#include "Mass/Monster/Processors/ASMassMonsterClientInterpolationProcessor.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "Mass/Monster/Replication/ASMassMonsterClientInterpolationFragment.h"
#include "MassNavigationProcessors.h"
#include "MassRepresentationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterClientInterpolationProcessor)

UASMassMonsterClientInterpolationProcessor::UASMassMonsterClientInterpolationProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client);
	// 이동·회전·Nav 높이 보정이 모두 끝난 뒤 수신 스냅샷만 Transform에 반영한다.
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
	ExecutionOrder.ExecuteAfter.Add(UMassNavigationSmoothHeightProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Representation);
}

void UASMassMonsterClientInterpolationProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FASMassMonsterClientInterpolationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FASMassMonsterClientReplicaTag>(EMassFragmentPresence::All);
}

void UASMassMonsterClientInterpolationProcessor::Execute(
	FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = FMath::Max(0.0f, Context.GetDeltaTimeSeconds());
	EntityQuery.ForEachEntityChunk(Context, [DeltaTime](FMassExecutionContext& QueryContext)
	{
		const TArrayView<FTransformFragment> Transforms =
			QueryContext.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FASMassMonsterClientInterpolationFragment> Interpolations =
			QueryContext.GetMutableFragmentView<FASMassMonsterClientInterpolationFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = QueryContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FASMassMonsterClientInterpolationFragment& Interpolation = Interpolations[EntityIt];
			if (!Interpolation.bInitialized)
			{
				continue;
			}

			Interpolation.ElapsedSeconds = FMath::Min(
				Interpolation.ElapsedSeconds + DeltaTime, Interpolation.DurationSeconds);
			const float Alpha = Interpolation.DurationSeconds > UE_SMALL_NUMBER
				? Interpolation.ElapsedSeconds / Interpolation.DurationSeconds
				: 1.0f;
			const float InterpolatedYaw = Interpolation.StartYawRadians
				+ FMath::FindDeltaAngleRadians(Interpolation.StartYawRadians, Interpolation.TargetYawRadians)
					* FMath::Clamp(Alpha, 0.0f, 1.0f);

			FTransform& Transform = Transforms[EntityIt].GetMutableTransform();
			Transform.SetLocation(FMath::Lerp(Interpolation.StartLocation, Interpolation.TargetLocation, Alpha));
			Transform.SetRotation(FQuat(FVector::UpVector, InterpolatedYaw));
		}
	});
}
