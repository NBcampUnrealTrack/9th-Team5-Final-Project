#include "Mass/Monster/Traits/ASMassMonsterReplicationTrait.h"

#include "Mass/Monster/Replication/ASMassMonsterClientBubble.h"
#include "Mass/Monster/Replication/ASMassMonsterReplicator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterReplicationTrait)

UASMassMonsterReplicationTrait::UASMassMonsterReplicationTrait()
{
	Params.BubbleInfoClass = AASMassMonsterClientBubbleInfo::StaticClass();
	Params.ReplicatorClass = UASMassMonsterReplicator::StaticClass();
}
