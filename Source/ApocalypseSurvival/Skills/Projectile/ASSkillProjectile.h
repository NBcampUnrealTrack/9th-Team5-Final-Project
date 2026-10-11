#pragma once

#include "CoreMinimal.h"
#include "Skills/ASSkillAbility.h"
#include "ASSkillProjectile.generated.h"

class AASSkillProjectileActor;

//타격 타이밍마다 투사체(AASSkillProjectileActor)를 발사하는 스킬
UCLASS(Abstract)
class APOCALYPSESURVIVAL_API UASSkillProjectile : public UASSkillAbility
{
	GENERATED_BODY()

public:
	UASSkillProjectile();

protected:
	virtual void OnSkillEffect_Implementation() override;
	virtual FASSkillIndicatorSettings GetIndicatorSettings_Implementation() const override;

	//발사할 투사체 (메시·이펙트를 넣은 BP 자식 지정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Projectile")
	TSubclassOf<AASSkillProjectileActor> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Projectile", meta = (ClampMin = "1.0", Units = "cm/s"))
	float Speed = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Projectile", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxRange = 1500.0f;

	//0 = 첫 적에서 사라짐, -1 = 무한 관통
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Projectile", meta = (ClampMin = "-1"))
	int32 PierceCount = 0;

	//발사 위치로 쓸 캐릭터 메시 소켓 (비우면 캐릭터 앞)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Projectile")
	FName SpawnSocketName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Projectile", meta = (Units = "cm"))
	float SpawnForwardOffset = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AS|Skill|Projectile", meta = (Units = "cm"))
	float SpawnHeightOffset = 0.0f;
};
