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
	// 월드 서브시스템 생성 시점에는 아직 NetDriver가 붙지 않아, 나중에 Listen Server가 될 월드도
	// NM_Standalone으로 보일 수 있다. BubbleInfoClass는 클라이언트가 동기화되기 전에 등록해야 한다.
	return World && World->IsGameWorld();
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

	// PendingNetGame이 이미 설정된 클라이언트 월드는 이 시점에 미리 준비한다.
	RegisterClientEntityTemplates();
}

void UASMassMonsterReplicationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 일부 travel 경로에서는 PostInitialize 뒤에 NetDriver가 연결된다. 최종 NetMode에서 한 번 더
	// 확인해 클라이언트 Entity Template 사전 생성을 보장한다.
	RegisterClientEntityTemplates();
}

void UASMassMonsterReplicationSubsystem::RegisterClientEntityTemplates()
{
	UWorld* World = GetWorld();
	if (bClientEntityTemplatesRegistered || !World || World->GetNetMode() != NM_Client)
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
	bClientEntityTemplatesRegistered = true;
}
