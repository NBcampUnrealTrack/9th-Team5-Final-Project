#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ASSkillSettingsSubsystem.generated.h"

class APlayerController;

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

	//컨트롤러의 로컬 플레이어 설정 (로컬이 아니면 null)
	static UASSkillSettingsSubsystem* Get(const APlayerController* PlayerController);

private:
	void SaveSettings() const;

	bool bWalkToCastWhenOutOfRange = true;
};
