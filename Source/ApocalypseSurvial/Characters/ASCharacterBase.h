#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "ASCharacterBase.generated.h"

class UASAbilitySystemComponent;
class UASAttributeSet;
struct FOnAttributeChangeData;

/**
 * Base for every character that takes part in combat.
 *
 * The class does not create an ability system itself: players borrow the one
 * on their AASPlayerState, enemies can own theirs. Subclasses call
 * InitAbilitySystem once the component is known on the local machine.
 */
UCLASS(Abstract)
class APOCALYPSESURVIAL_API AASCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AASCharacterBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "GAS")
	UASAttributeSet* GetAttributeSet() const { return AttributeSet; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Links this character to an ability system as its avatar.
	 * Safe to call again, e.g. when the PlayerState replicates more than once.
	 */
	void InitAbilitySystem(UASAbilitySystemComponent* InAbilitySystemComponent, UASAttributeSet* InAttributeSet, AActor* InOwnerActor);

	/** Called on every machine after InitAbilitySystem has linked the ability system. */
	virtual void OnAbilitySystemInitialized();

private:
	void UnbindAttributeDelegates();
	void HandleMoveSpeedChanged(const FOnAttributeChangeData& Data);

	UPROPERTY(Transient)
	TObjectPtr<UASAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<UASAttributeSet> AttributeSet;

	FDelegateHandle MoveSpeedChangedHandle;
};
