#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "ASPlayerState.generated.h"

class UASAbilitySystemComponent;
class UASAttributeSet;

/**
 * Owns the player's ability system so attributes, cooldowns and granted
 * abilities persist while the pawn is destroyed and respawned.
 */
UCLASS()
class APOCALYPSESURVIAL_API AASPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AASPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UASAbilitySystemComponent* GetASAbilitySystemComponent() const { return AbilitySystemComponent; }

	UASAttributeSet* GetAttributeSet() const { return AttributeSet; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UASAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UASAttributeSet> AttributeSet;
};
