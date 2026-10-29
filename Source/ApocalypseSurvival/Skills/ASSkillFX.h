#pragma once

#include "CoreMinimal.h"

class UFXSystemAsset;
class UFXSystemComponent;

//스킬 이펙트 재생 도우미 (Niagara·Cascade 둘 다 받음)
namespace ASSkillFX
{
	//위치에 한 번 재생 (끝나면 자동 삭제). 컴포넌트는 월드 소유라 스폰한 액터가 사라져도 남음
	APOCALYPSESURVIVAL_API UFXSystemComponent* SpawnAtLocation(const UObject* WorldContext, UFXSystemAsset* Asset,
		const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator, const FVector& Scale = FVector::OneVector);
}
