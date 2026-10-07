#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ASInputConfig.generated.h"

class UInputAction;

/** 입력 액션 하나와 그것이 뜻하는 입력 태그. */
USTRUCT(BlueprintType)
struct FASInputTagAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * 어빌리티용 입력 목록. "이 키는 이 태그다"를 적어 두는 데이터 에셋이다.
 *
 * 컨트롤러가 이 목록을 읽어 키마다 핸들러를 걸고, 눌리면 태그를 ASC 에 넘긴다.
 * 이동·카메라처럼 어빌리티가 아닌 입력은 여기 넣지 않고 컨트롤러가 직접 묶는다.
 * 키를 추가할 때 C++ 를 고치지 않아도 되게 하려고 에셋으로 뺐다. Lyra 의 InputConfig 와 같은 역할이다.
 */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FASInputTagAction> AbilityInputActions;
};
