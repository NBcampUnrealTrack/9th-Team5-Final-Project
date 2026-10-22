#pragma once

#include "CoreMinimal.h"
#include "Mass/Monster/Replication/ASMassMonsterReplicatedAgent.h"
#include "MassClientBubbleHandler.h"
#include "MassClientBubbleInfoBase.h"
#include "MassClientBubbleSerializerBase.h"
#include "MassReplicationTransformHandlers.h"
#include "ASMassMonsterClientBubble.generated.h"

struct FMassEntityView;

/** 몬스터 Fast Array 수신 시 클라이언트 Mass Entity의 생성, 갱신, 제거를 담당한다. */
class APOCALYPSESURVIVAL_API FASMassMonsterClientBubbleHandler
	: public TClientBubbleHandlerBase<FASMassMonsterFastArrayItem>
{
public:
	typedef TClientBubbleHandlerBase<FASMassMonsterFastArrayItem> Super;
	typedef TMassClientBubbleTransformHandler<FASMassMonsterFastArrayItem> FTransformHandler;

	FASMassMonsterClientBubbleHandler()
		: TransformHandler(*this)
	{
	}

#if UE_REPLICATION_COMPILE_SERVER_CODE
	const FTransformHandler& GetTransformHandler() const { return TransformHandler; }
	FTransformHandler& GetTransformHandlerMutable() { return TransformHandler; }

	/** 표현 데이터가 달라진 경우에만 Fast Array 항목을 dirty 처리한다. */
	bool SetVisualData(const FMassReplicatedAgentHandle Handle,
		const FASReplicatedMonsterVisualData& NewVisualData)
	{
		FASMassMonsterFastArrayItem& Item = GetAgentItem(Handle);
		FASReplicatedMonsterVisualData& CurrentVisualData = Item.Agent.GetVisualDataMutable();
		if (CurrentVisualData.HealthRatio == NewVisualData.HealthRatio
			&& CurrentVisualData.WorldBehavior == NewVisualData.WorldBehavior)
		{
			return false;
		}

		CurrentVisualData = NewVisualData;
		MarkItemDirty(Item);
		return true;
	}
#endif

protected:
#if UE_REPLICATION_COMPILE_CLIENT_CODE
	virtual void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize) override;
	virtual void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize) override;

	void ApplyVisualData(const FMassEntityView& EntityView,
		const FASReplicatedMonsterVisualData& VisualData, bool bIsInitialState) const;
#endif

private:
	FTransformHandler TransformHandler;
};

/** PlayerController당 한 개씩 생성되는 몬스터 Fast Array serializer다. */
USTRUCT()
struct APOCALYPSESURVIVAL_API FASMassMonsterClientBubbleSerializer : public FMassClientBubbleSerializerBase
{
	GENERATED_BODY()

	FASMassMonsterClientBubbleSerializer()
	{
		Bubble.Initialize(Monsters, *this);
	}

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<
			FASMassMonsterFastArrayItem,
			FASMassMonsterClientBubbleSerializer>(Monsters, DeltaParams, *this);
	}

	FASMassMonsterClientBubbleHandler& GetBubble() { return Bubble; }
	const FASMassMonsterClientBubbleHandler& GetBubble() const { return Bubble; }

private:
	FASMassMonsterClientBubbleHandler Bubble;

	/** 서버와 클라이언트의 배열 인덱스는 다를 수 있으므로 NetID로만 식별한다. */
	UPROPERTY(Transient)
	TArray<FASMassMonsterFastArrayItem> Monsters;
};

template<>
struct TStructOpsTypeTraits<FASMassMonsterClientBubbleSerializer>
	: public TStructOpsTypeTraitsBase2<FASMassMonsterClientBubbleSerializer>
{
	enum
	{
		WithNetDeltaSerializer = true,
		WithCopy = false,
	};
};

/** 소유 클라이언트에만 몬스터 Fast Array를 전송하는 replication actor다. */
UCLASS()
class APOCALYPSESURVIVAL_API AASMassMonsterClientBubbleInfo : public AMassClientBubbleInfoBase
{
	GENERATED_BODY()

public:
	AASMassMonsterClientBubbleInfo(const FObjectInitializer& ObjectInitializer);

	FASMassMonsterClientBubbleSerializer& GetMonsterSerializer() { return MonsterSerializer; }
	const FASMassMonsterClientBubbleSerializer& GetMonsterSerializer() const { return MonsterSerializer; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated, Transient)
	FASMassMonsterClientBubbleSerializer MonsterSerializer;
};
