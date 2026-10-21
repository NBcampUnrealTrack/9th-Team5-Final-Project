#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Items/Data/ASItemGrant.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASMonsterDropSubsystem.generated.h"

class AController;
class AASDroppedItem;

/** 서버에서 몬스터 드랍 그룹을 추첨하고 월드 아이템 스택으로 생성한다. */
UCLASS()
class APOCALYPSESURVIVAL_API UASMonsterDropSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** 유효한 플레이어가 처치한 경우에만 그룹을 추첨해 월드 아이템을 생성한다. */
	bool SpawnDrops(const FDataTableRowHandle& DropGroup, const FVector& DeathLocation, AController* KillerController);

private:
	AASDroppedItem* SpawnDropStack(const FASItemGrant& Grant, int32 Quantity, const FVector& Location) const;
	FVector FindDropLocation(const FVector& Origin, int32 DropIndex, FRandomStream& Random) const;
};
