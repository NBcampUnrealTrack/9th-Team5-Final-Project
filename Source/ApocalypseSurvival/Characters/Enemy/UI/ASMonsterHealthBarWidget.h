#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ASMonsterHealthBarWidget.generated.h"

class AASMonsterCharacter;
class UASAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
struct FOnAttributeChangeData;

/**
 * 몬스터 머리 위에 표시하는 가벼운 체력바.
 * Tick 이나 UMG Property Binding 대신 GAS Attribute 변경 이벤트로만 갱신한다.
 * 
 * Blueprint 자식에서 HealthProgressBar와 HealthText를 같은 이름으로 배치하면 그 Widget을 사용하고,
 * 별도 Blueprint가 없으면 코드에서 만드는 기본 레이아웃을 사용한다.
 */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API UASMonsterHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** nullptr을 전달하면 Attribute 구독을 해제한다. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|UI")
	void SetHealthSource(AASMonsterCharacter* InMonster);

	UFUNCTION(BlueprintCallable, Category = "Enemy|UI")
	void SetHealthValues(float InHealth, float InMaxHealth);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultWidgetTree();
	void UnbindHealthSource();
	void RefreshFromAttributes();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient)
	TObjectPtr<UASAbilitySystemComponent> AbilitySystemComponent;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
};