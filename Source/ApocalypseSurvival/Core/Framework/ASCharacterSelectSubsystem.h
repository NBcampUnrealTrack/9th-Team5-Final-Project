#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ASCharacterSelectSubsystem.generated.h"

class UASCharacterDefinition;

/*
 * 게임 시작 전에 고른 캐릭터를 기억한다.
 * GameInstance 수명이라 메뉴 -> 게임 맵으로 이동해도 유지된다.
 */

UCLASS()
class APOCALYPSESURVIVAL_API UASCharacterSelectSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "CharacterSelect")
	void SetSelectedCharacter(const UASCharacterDefinition* InDefinition);
	
	UFUNCTION(BlueprintCallable, Category = "CharacterSelect")
	void ClearSelectedCharacter();
	
	const UASCharacterDefinition* GetSelectedCharacter() const;
	
	bool HasSelectedCharacter() const;
	
	// 게임모드 스폰용: 선택한 캐릭터 정의의 캐릭터 BP (선택 없으면 nullptr)	
	TSubclassOf<APawn> GetSelectedPawnClass() const;
	
private:
	UPROPERTY()
	TObjectPtr<const UASCharacterDefinition> SelectedCharacter;
};
