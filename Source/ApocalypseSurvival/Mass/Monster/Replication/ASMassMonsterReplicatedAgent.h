#pragma once

#include "CoreMinimal.h"
#include "Mass/Monster/Data/ASMassMonsterTypes.h"
#include "MassClientBubbleHandler.h"
#include "MassReplicationTransformHandlers.h"
#include "MassReplicationTypes.h"
#include "ASMassMonsterReplicatedAgent.generated.h"

/**
 * 원격 클라이언트의 Mass 몬스터 표현에만 필요한 상태다.
 * AI, 타깃, 보상 등 서버 게임플레이 데이터는 포함하지 않는다.
 */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASReplicatedMonsterVisualData
{
	GENERATED_BODY()

	/** 서버 현재 체력 / 최대 체력을 0..65535 범위로 양자화한 값이다. 0은 사망 상태다. */
	UPROPERTY(Transient)
	uint16 HealthRatio = MAX_uint16;

	/** 낮/밤에 따라 선택되는 클라이언트 전용 외형 상태다. */
	UPROPERTY(Transient)
	EASMassMonsterWorldBehavior WorldBehavior = EASMassMonsterWorldBehavior::DayTerritory;
};

/** MassReplication Fast Array가 몬스터 한 개체에 대해 전송하는 데이터다. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterReplicatedAgent : public FReplicatedAgentBase
{
	GENERATED_BODY()

	FASMassMonsterReplicatedAgent() = default;

	FASMassMonsterReplicatedAgent(const FMassNetworkID InNetID, const FMassEntityTemplateID InTemplateID)
		: FReplicatedAgentBase(InNetID, InTemplateID)
	{
	}

	const FReplicatedAgentPositionYawData& GetReplicatedPositionYawData() const
	{
		return PositionYaw;
	}

	FReplicatedAgentPositionYawData& GetReplicatedPositionYawDataMutable()
	{
		return PositionYaw;
	}

	const FASReplicatedMonsterVisualData& GetVisualData() const
	{
		return Visual;
	}

	FASReplicatedMonsterVisualData& GetVisualDataMutable()
	{
		return Visual;
	}

private:
	UPROPERTY(Transient)
	FReplicatedAgentPositionYawData PositionYaw;

	UPROPERTY(Transient)
	FASReplicatedMonsterVisualData Visual;
};

/** Fast Array serializer에 들어가는 몬스터 단위 항목이다. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterFastArrayItem : public FMassFastArrayItemBase
{
	GENERATED_BODY()

	FASMassMonsterFastArrayItem() = default;

	FASMassMonsterFastArrayItem(const FASMassMonsterReplicatedAgent& InAgent,
		const FMassReplicatedAgentHandle InHandle)
		: FMassFastArrayItemBase(InHandle)
		, Agent(InAgent)
	{
	}

	/** FMassFastArrayItemBase가 요구하는 replicated agent 타입 별칭이다. */
	typedef FASMassMonsterReplicatedAgent FReplicatedAgentType;

	UPROPERTY()
	FASMassMonsterReplicatedAgent Agent;
};
