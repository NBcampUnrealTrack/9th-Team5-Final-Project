#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ASMassMonsterReplicationSubsystem.generated.h"

/**
 * Mass 몬스터용 client bubble class를 MassReplication에 등록한다.
 * NetDriver가 붙기 전에도 BubbleInfoClass를 등록해야 하므로 모든 게임 월드에 존재한다.
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASMassMonsterReplicationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void PostInitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void RegisterClientEntityTemplates();

	bool bClientEntityTemplatesRegistered = false;
};
