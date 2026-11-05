#pragma once

#include "CoreMinimal.h"
#include "Items/Use/ASDeployableActor.h"
#include "Items/Data/ASItemAssetLoading.h"
#include "ASBarricadeActor.generated.h"

class APawn;
class UBoxComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UStaticMesh;

/** Player-built obstacle. AI chooses whether to attack it or route around it. */
UCLASS(Blueprintable)
class APOCALYPSESURVIVAL_API AASBarricadeActor : public AASDeployableActor
{
	GENERATED_BODY()

public:
	AASBarricadeActor();

	/** Server checks ground support, slope, range, overlap and line of sight before paying the item cost. */
	bool TryGetPlacementTransform(const APawn* Placer, const FVector& DesiredLocation,
		float PlacementRange, FTransform& OutTransform) const;

	/** Local-only preview; never enables collision or modifies navigation. */
	void SetPlacementPreview(bool bCanPlace);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ApplyPlacementState(bool bIsActive) override;
	virtual float CalculateMonsterTargetRadius() const override;

	/** Box dimensions define both movement blocking and the required placement footprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionComponent;

	/** Installed appearance is independent of the inventory/drop mesh on the item PDA. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Barricade|Visual")
	TSoftObjectPtr<UStaticMesh> MeshAsset;

	/** Translucent material with a vector parameter named Tint. Green means valid; red means blocked. */
	UPROPERTY(EditDefaultsOnly, Category = "Item|Barricade|Preview")
	TSoftObjectPtr<UMaterialInterface> PreviewMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Item|Barricade|Placement", meta = (ClampMin = "0", ClampMax = "60", Units = "deg"))
	float MaxGroundSlope = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Item|Barricade|Placement", meta = (ClampMin = "0", Units = "cm"))
	float MaxGroundHeightDifference = 10.0f;

private:
	FASItemAssetRequest ResourceRequest;
	void ApplyLoadedResources();
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PreviewMaterials;
};
