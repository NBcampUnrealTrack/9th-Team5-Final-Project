#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ASSkillSettingsSubsystem.generated.h"

class APlayerController;

//스킬 시전 방식 (롤과 같은 3가지)
UENUM(BlueprintType)
enum class EASSkillCastMode : uint8
{
	//누르면 바로 시전
	QuickCast,
	//꾹 누르면 범위 표시, 떼면 시전
	QuickCastWithIndicator,
	//누르면 범위 표시, 좌클릭으로 시전 (우클릭·같은 키로 취소)
	NormalCast
};

/**
 * 플레이어 개인 스킬 조작 설정 (서버와 무관, 본인 화면 기준)
 * 옵션 UI에서 Set 함수만 호출하면 되고 값은 SaveGame으로 저장됨
 */
UCLASS()
class APOCALYPSESURVIVAL_API UASSkillSettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	//사거리 밖을 지정했을 때 걸어가서 시전하는지 (false = 사거리 끝에 시전)
	UFUNCTION(BlueprintPure, Category = "AS|Skill|Settings")
	bool GetWalkToCastWhenOutOfRange() const { return bWalkToCastWhenOutOfRange; }

	//설정 변경 후 바로 저장
	UFUNCTION(BlueprintCallable, Category = "AS|Skill|Settings")
	void SetWalkToCastWhenOutOfRange(bool bEnabled);

	//테스트용: 현재 값 반전
	UFUNCTION(BlueprintCallable, Category = "AS|Skill|Settings")
	void ToggleWalkToCastWhenOutOfRange();

	UFUNCTION(BlueprintPure, Category = "AS|Skill|Settings")
	EASSkillCastMode GetCastMode() const { return CastMode; }

	//설정 변경 후 바로 저장
	UFUNCTION(BlueprintCallable, Category = "AS|Skill|Settings")
	void SetCastMode(EASSkillCastMode NewCastMode);

	//테스트용: 빠른 시전 → 범위 표시 후 빠른 시전 → 일반 시전 순서로 변경
	UFUNCTION(BlueprintCallable, Category = "AS|Skill|Settings")
	void CycleCastMode();

	//컨트롤러의 로컬 플레이어 설정 (로컬이 아니면 null)
	static UASSkillSettingsSubsystem* Get(const APlayerController* PlayerController);

private:
	void SaveSettings() const;

	bool bWalkToCastWhenOutOfRange = true;

	EASSkillCastMode CastMode = EASSkillCastMode::NormalCast;
};
