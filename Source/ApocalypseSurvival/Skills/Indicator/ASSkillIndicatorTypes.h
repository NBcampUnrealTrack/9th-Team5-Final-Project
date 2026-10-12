#pragma once

#include "CoreMinimal.h"
#include "ASSkillIndicatorTypes.generated.h"

//스킬 조준 방식
UENUM(BlueprintType)
enum class EASSkillTargetingType : uint8
{
	//커서 방향으로 회전 (근접 부채꼴, 투사체)
	Direction,
	//커서 위치에 시전 (사거리 안으로 제한)
	Location,
	//시전자 중심 (회전 없음)
	Self
};

//인디케이터 모양
UENUM(BlueprintType)
enum class EASSkillIndicatorShape : uint8
{
	Circle,
	Cone,
	Line
};

//인디케이터 표시 정보 (스킬이 채워서 넘김)
USTRUCT(BlueprintType)
struct APOCALYPSESURVIVAL_API FASSkillIndicatorSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AS|Skill|Indicator")
	EASSkillTargetingType TargetingType = EASSkillTargetingType::Direction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AS|Skill|Indicator")
	EASSkillIndicatorShape Shape = EASSkillIndicatorShape::Circle;

	//원·부채꼴 반지름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AS|Skill|Indicator", meta = (Units = "cm"))
	float Radius = 100.0f;

	//부채꼴 각도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AS|Skill|Indicator", meta = (Units = "deg"))
	float Angle = 360.0f;

	//방향형 원·부채꼴 중심을 조준 방향으로 옮기는 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AS|Skill|Indicator", meta = (Units = "cm"))
	float ForwardOffset = 0.0f;

	//직선 길이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AS|Skill|Indicator", meta = (Units = "cm"))
	float Length = 0.0f;

	//직선 폭
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AS|Skill|Indicator", meta = (Units = "cm"))
	float Width = 0.0f;

	//위치형 최대 사거리 (목표 지점을 이 안으로 제한)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AS|Skill|Indicator", meta = (Units = "cm"))
	float MaxCastRange = 0.0f;

	//위치형 목표 지점을 사거리 안으로 제한 (false = 커서 위치 그대로, 걸어가서 시전 모드)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AS|Skill|Indicator")
	bool bClampToRange = true;

	//사거리 원 반지름 (0이면 숨김)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AS|Skill|Indicator", meta = (Units = "cm"))
	float RangeRingRadius = 0.0f;
};
