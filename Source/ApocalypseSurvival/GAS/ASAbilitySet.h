#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "ASAbilitySet.generated.h"

class UASAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;

/** 세트가 부여할 어빌리티 하나. */
USTRUCT(BlueprintType)
struct FASAbilitySet_GameplayAbility
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> Ability = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "1"))
	int32 AbilityLevel = 1;

	/** 이 어빌리티를 발동시킬 입력 태그. 비워 두면 입력으로는 발동하지 않는다(자동 발동·패시브용). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/** 세트가 ASC 자신에게 적용할 지속/즉발 GameplayEffect 하나. */
USTRUCT(BlueprintType)
struct FASAbilitySet_GameplayEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TSubclassOf<UGameplayEffect> GameplayEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "1"))
	float EffectLevel = 1.0f;
};

/**
 * 세트가 부여한 어빌리티들의 핸들. 세트를 다시 걷어낼 때 쓴다.
 *
 * 부여한 쪽이 이 값을 들고 있다가 TakeFromAbilitySystem 을 부르면 그 세트가 준 것만 정확히 빠진다.
 * 무기처럼 장착·해제가 있는 세트에 필요하고, 캐릭터 기본 세트는 접속 중에는 걷어내지 않는다.
 */
USTRUCT(BlueprintType)
struct FASAbilitySet_GrantedHandles
{
	GENERATED_BODY()

	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);
	void TakeFromAbilitySystem(UASAbilitySystemComponent* AbilitySystemComponent);
	bool IsEmpty() const { return AbilitySpecHandles.IsEmpty() && GameplayEffectHandles.IsEmpty(); }

private:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;
};

/**
 * 어빌리티 묶음. "이 대상에게 이 어빌리티들을 준다"는 목록이다.
 *
 * 어빌리티 클래스와 입력 태그를 같이 적는다. 부여할 때 태그를 스펙에 붙이므로,
 * 같은 어빌리티를 다른 키에 묶거나 캐릭터마다 다른 키 배치를 줄 수 있다.
 * 캐릭터 기본 세트(회피, 스킬)가 첫 사용처이고, Lyra 의 AbilitySet 과 같은 구조다.
 */
UCLASS(BlueprintType, Const)
class APOCALYPSESURVIVAL_API UASAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 서버 전용. 목록의 어빌리티를 부여하고 GameplayEffect 를 ASC 자신에게 적용한다.
	 * @param OutGrantedHandles  나중에 걷어낼 일이 있으면 받아 둔다. 없으면 null.
	 * @param SourceObject       어빌리티가 자기 출처로 기억할 객체. 무기 세트라면 아이템 인스턴스.
	 */
	void GiveToAbilitySystem(UASAbilitySystemComponent* AbilitySystemComponent, FASAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Abilities", meta = (TitleProperty = "Ability"))
	TArray<FASAbilitySet_GameplayAbility> GrantedGameplayAbilities;

	/** 기본 마나 재생처럼 ASC 수명 동안 유지할 효과를 둔다. */
	UPROPERTY(EditDefaultsOnly, Category = "Effects", meta = (TitleProperty = "GameplayEffect"))
	TArray<FASAbilitySet_GameplayEffect> GrantedGameplayEffects;
};
