#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ASInteractable.h"
#include "ASQuestNpc.generated.h"

/** Place in any test map now, and in the private lobby later. Replace the placeholder mesh in BP. */
UCLASS()
class APOCALYPSESURVIVAL_API AASQuestNpc : public AActor, public IASInteractable
{
	GENERATED_BODY()
public:
	AASQuestNpc();
	UFUNCTION(BlueprintPure, Category = "Quest|NPC")
	FName GetNpcId() const;
	UFUNCTION(BlueprintPure, Category = "Quest|NPC")
	FText GetDisplayName() const;
	FText GetIdleDialogue() const;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FVector GetInteractionLocation_Implementation() const override { return GetActorLocation(); }
	virtual float GetInteractionRange_Implementation() const override { return InteractionRange; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|NPC")
	TObjectPtr<class UASQuestNpcDefinition> Definition;
	/** Fallback for existing authored NPCs without a definition asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|NPC", AdvancedDisplay)
	FName NpcId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|NPC")
	FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|NPC", meta = (ClampMin = "1"))
	float InteractionRange = 250.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest|NPC")
	TObjectPtr<class UStaticMeshComponent> Mesh;
};
