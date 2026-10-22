#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASMassMonsterReplicationSubsystem.generated.h"

/**
 * Mass 몬스터용 client bubble class를 MassReplication에 등록한다.
 * 서버와 클라이언트 양쪽에서 같은 Entity Config template을 만들 수 있도록 네트워크 월드에만 존재한다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterReplicationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void PostInitialize() override;
};
