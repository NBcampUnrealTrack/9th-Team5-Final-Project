#include "Mass/Monster/Traits/ASMassMonsterReplicationTrait.h"

#include "ApocalypseSurvival.h"
#include "Engine/World.h"
#include "Mass/Monster/Replication/ASMassMonsterClientBubble.h"
#include "Mass/Monster/Replication/ASMassMonsterClientInterpolationFragment.h"
#include "Mass/Monster/Replication/ASMassMonsterReplicator.h"
#include "MassMovementFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterReplicationTrait)

UASMassMonsterReplicationTrait::UASMassMonsterReplicationTrait()
{
	Params.BubbleInfoClass = AASMassMonsterClientBubbleInfo::StaticClass();
	Params.ReplicatorClass = UASMassMonsterReplicator::StaticClass();
}

void UASMassMonsterReplicationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
	const UWorld& World) const
{
	UE_LOG(LogApocalypseSurvival, Log,
		TEXT("[MassMonsterReplication] BuildTemplate: World=%s NetMode=%d Inspecting=%d Bubble=%s Replicator=%s"),
		*GetNameSafe(&World), static_cast<int32>(World.GetNetMode()), BuildContext.IsInspectingData(),
		*GetNameSafe(Params.BubbleInfoClass.Get()), *GetNameSafe(Params.ReplicatorClass.Get()));

	Super::BuildTemplate(BuildContext, World);

	if (World.GetNetMode() == NM_Client && !BuildContext.IsInspectingData())
	{
		// 서버가 유일하게 이동을 계산한다. 클라이언트는 수신 스냅샷을 보간해 표시만 한다.
		BuildContext.AddTag<FASMassMonsterClientReplicaTag>();
		BuildContext.AddTag<FMassCustomMovementTag>();
		BuildContext.AddFragment<FASMassMonsterClientInterpolationFragment>();
	}
}
