#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "ASBarricadeActor.generated.h"

class APawn;
class UAbilitySystemComponent;
class UASHealthAttributeSet;
class UBoxComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
struct FOnAttributeChangeData;

/** Player-built obstacle. AI chooses whether to attack it or route around it. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASBarricadeActor : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AASBarricadeActor();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server checks ground support, slope, range, overlap and line of sight before paying the item cost. */
	bool TryGetPlacementTransform(const APawn* Placer, const FVector& DesiredLocation,
		float PlacementRange, FTransform& OutTransform) const;
	bool PreparePlacement(float MaxHealth, float Lifetime);
	void FinishPlacement();

	/** Local-only preview; never enables collision or modifies navigation. */
	void SetPlacementPreview(bool bCanPlace);

	UFUNCTION(BlueprintPure, Category = "Item|Barricade")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Item|Barricade")
	bool IsIntact() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Box dimensions define both movement blocking and the required placement footprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionComponent;

	/** Installed appearance is independent of the inventory/drop mesh on the item PDA. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UASHealthAttributeSet> HealthAttributes;

	/** Translucent material with a vector parameter named Tint. Green means valid; red means blocked. */
	UPROPERTY(EditDefaultsOnly, Category = "Item|Barricade|Preview")
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Item|Barricade|Placement", meta = (ClampMin = "0", ClampMax = "60", Units = "deg"))
	float MaxGroundSlope = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Item|Barricade|Placement", meta = (ClampMin = "0", Units = "cm"))
	float MaxGroundHeightDifference = 10.0f;

private:
	UFUNCTION()
	void OnRep_Placed();
	void UpdateCollision();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	UPROPERTY(ReplicatedUsing = OnRep_Placed)
	bool bPlaced = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PreviewMaterials;

	bool bPrepared = false;
	float PlacedLifetime = 0.0f;
};
