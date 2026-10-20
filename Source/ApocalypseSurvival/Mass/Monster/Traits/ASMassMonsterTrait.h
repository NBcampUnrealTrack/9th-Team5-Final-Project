#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "Mass/Monster/Data/ASMassMonsterFragments.h"
#include "ASMassMonsterTrait.generated.h"

class UASMonsterDefinition;

/**
 * 일반 몬스터 Entity의 프로젝트 전용 데이터 구성을 추가한다.
 * 이동, NavMesh, 회피, 표현, LOD Trait는 Entity Config에서 별도로 조합한다.
 */
UCLASS(meta = (DisplayName = "AS Mass Monster"))
class APOCALYPSESURVIVAL_API UASMassMonsterTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	/** 설정하면 기존 Actor 몬스터와 공유하는 능력치를 Parameters에 덮어쓴다. */
	UPROPERTY(EditAnywhere, Category = "Monster")
	TObjectPtr<UASMonsterDefinition> MonsterDefinition;

	/** Mass 전용 공격 패턴·쿨다운·갱신 주기와 Definition이 없을 때 사용할 기본값. */
	UPROPERTY(EditAnywhere, Category = "Monster")
	FASMassMonsterConfigFragment Parameters;

	/** 낮 스폰 Entity의 기본 귀환 반경. 실제 스폰 지점 설정으로 덮어쓸 수 있다. */
	UPROPERTY(EditAnywhere, Category = "Monster|Home", meta = (ClampMin = "0.0", Units = "cm"))
	float DefaultLeashRadius = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "Monster|Home", meta = (ClampMin = "1.0", Units = "cm"))
	float DefaultReturnAcceptanceRadius = 100.0f;
};

