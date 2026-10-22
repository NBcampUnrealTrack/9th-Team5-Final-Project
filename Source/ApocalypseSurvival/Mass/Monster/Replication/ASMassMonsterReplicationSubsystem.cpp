#include "Mass/Monster/Replication/ASMassMonsterReplicationSubsystem.h"

#include "Engine/World.h"
#include "Mass/Monster/Replication/ASMassMonsterClientBubble.h"
#include "MassReplicationSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMassMonsterReplicationSubsystem)

bool UASMassMonsterReplicationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Standalone;
}

void UASMassMonsterReplicationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UMassReplicationSubsystem>();
}

void UASMassMonsterReplicationSubsystem::PostInitialize()
{
	Super::PostInitialize();

	UWorld* World = GetWorld();
	UMassReplicationSubsystem* ReplicationSubsystem = World
		? World->GetSubsystem<UMassReplicationSubsystem>()
		: nullptr;
	if (!ensureMsgf(ReplicationSubsystem,
		TEXT("[MassMonsterReplication] UMassReplicationSubsystem is unavailable.")))
	{
		return;
	}

	ReplicationSubsystem->RegisterBubbleInfoClass(AASMassMonsterClientBubbleInfo::StaticClass());
}
