#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ASCharacterSelectSubsystem.generated.h"

UCLASS()
class APOCALYPSESURVIAL_API UASCharacterSelectSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "CharacterSelect")
	void SetSelectedPawnClass(TSubclassOf<APawn> InPawnClass);
	
	UFUNCTION(BlueprintPure, Category = "CharacterSelect")
	TSubclassOf<APawn> GetSelectedPawnClass() const { return SelectedPawnClass; }
	
private:
	UPROPERTY()
	TSubclassOf<APawn> SelectedPawnClass;
};
