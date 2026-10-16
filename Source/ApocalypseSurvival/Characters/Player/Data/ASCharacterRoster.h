#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ASCharacterRoster.generated.h"

class UASCharacterDefinition;

/* 캐릭터 선택 화면에 나오는 캐릭터 목록 (배열 순서 = 화면 순서) */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASCharacterRoster : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roster")
	TArray<TObjectPtr<const UASCharacterDefinition>> Characters;
	
	// 범위 밖이면 mullptr
	const UASCharacterDefinition* GetCharacterAt(int32 Index) const;
	
#if WITH_EDITOR
	// 빈 칸, 중복 경고
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
