#include "Mass/Monster/Replication/ASMassMonsterReplicationSubsystem.h"

#include "ApocalypseSurvival.h"
#include "Engine/World.h"
#include "Mass/Monster/Replication/ASMassMonsterClientBubble.h"
#include "MassEntityConfigAsset.h"
#include "MassReplicationSubsystem.h"
#include "MassSpawnerSubsystem.h"
#include "UObject/SoftObjectPath.h"

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
	Collection.InitializeDependency<UMassSpawnerSubsystem>();
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

	UE_LOG(LogApocalypseSurvival, Log,
		TEXT("[MassMonsterReplication] Register BubbleInfoClass: World=%s NetMode=%d Class=%s"),
		*GetNameSafe(World), static_cast<int32>(World->GetNetMode()),
		*GetNameSafe(AASMassMonsterClientBubbleInfo::StaticClass()));
	ReplicationSubsystem->RegisterBubbleInfoClass(AASMassMonsterClientBubbleInfo::StaticClass());

	RegisterClientEntityTemplates();
}

void UASMassMonsterReplicationSubsystem::RegisterClientEntityTemplates()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client)
	{
		return;
	}

	static const TCHAR* const ReplicatedMonsterConfigPaths[] =
	{
		TEXT("/Game/Mass/Monster/DA_MassMonster_Test.DA_MassMonster_Test"),
		TEXT("/Game/Mass/Monster/DA_MassMonster_Summoner.DA_MassMonster_Summoner"),
		TEXT("/Game/Mass/Monster/DA_MassMonster_Suicide.DA_MassMonster_Suicide"),
		TEXT("/Game/Mass/Monster/DA_MassMonster_Ranged.DA_MassMonster_Ranged"),
		TEXT("/Game/Mass/Monster/DA_MassMonster_Melee.DA_MassMonster_Melee"),
		TEXT("/Game/Mass/Monster/DA_MassMonster_HasteSupport.DA_MassMonster_HasteSupport"),
		TEXT("/Game/Mass/Monster/DA_MassMonster_Charge.DA_MassMonster_Charge"),
	};

	int32 RegisteredTemplateCount = 0;
	for (const TCHAR* ConfigPathString : ReplicatedMonsterConfigPaths)
	{
		const FSoftObjectPath ConfigPath(ConfigPathString);
		UMassEntityConfigAsset* EntityConfig = Cast<UMassEntityConfigAsset>(ConfigPath.TryLoad());
		if (!ensureMsgf(EntityConfig,
			TEXT("[MassMonsterReplication] Failed to load replicated Entity Config: %s"),
			*ConfigPath.ToString()))
		{
			continue;
		}

		if (ensureMsgf(EntityConfig->GetOrCreateEntityTemplate(*World).IsValid(),
			TEXT("[MassMonsterReplication] Failed to register Entity Template: %s"),
			*ConfigPath.ToString()))
		{
			++RegisteredTemplateCount;
		}
	}

	UE_LOG(LogApocalypseSurvival, Log,
		TEXT("[MassMonsterReplication] Registered client Entity Templates: World=%s Count=%d"),
		*GetNameSafe(World), RegisteredTemplateCount);
}
