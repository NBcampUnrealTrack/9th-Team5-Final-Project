#include "Items/Use/ASBarricadeActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavAreas/NavArea_Null.h"
#include "UObject/ConstructorHelpers.h"

AASBarricadeActor::AASBarricadeActor()
{
	CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	CollisionComponent->InitBoxExtent(FVector(20.0f, 100.0f, 60.0f));
	RootComponent = CollisionComponent;
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComponent->SetGenerateOverlapEvents(false);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionComponent->bDynamicObstacle = true;
	CollisionComponent->SetAreaClassOverride(UNavArea_Null::StaticClass());
	CollisionComponent->SetCanEverAffectNavigation(false);
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCanEverAffectNavigation(false);
	MeshComponent->SetRelativeScale3D(FVector(0.4f, 2.0f, 1.2f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) MeshComponent->SetStaticMesh(Cube.Object);
	MeshComponent->SetVisibility(false);
	// Barricades are normal combat objectives in open areas and temporary blockers when they obstruct a route.
	SetDefaultMonsterTargetProfile(
		EASMonsterTargetCapability::PrimaryTarget | EASMonsterTargetCapability::BlockingTarget,
		20.0f);
}

bool AASBarricadeActor::TryGetPlacementTransform(const APawn* Placer, const FVector& DesiredLocation,
	const float PlacementRange, FTransform& OutTransform) const
{
	const UWorld* World = GetWorld();
	const FVector Extent = CollisionComponent->GetScaledBoxExtent();
	if (!World || !IsValid(Placer) || Placer->GetWorld() != World || DesiredLocation.ContainsNaN()
		|| !FMath::IsFinite(PlacementRange) || PlacementRange <= 0.0f || Extent.ContainsNaN()
		|| Extent.GetMin() <= 1.0f || !FMath::IsFinite(MaxGroundSlope) || MaxGroundSlope < 0.0f || MaxGroundSlope > 60.0f
		|| !FMath::IsFinite(MaxGroundHeightDifference) || MaxGroundHeightDifference < 0.0f) return false;
	const FVector Origin = Placer->GetActorLocation();
	FVector Offset = DesiredLocation - Origin;
	Offset.Z = 0.0f;
	Offset = Offset.GetClampedToMaxSize2D(PlacementRange);
	const FVector GroundSearch = Origin + Offset;
	FVector Direction = Offset.GetSafeNormal2D();
	if (Direction.IsNearlyZero()) Direction = Placer->GetActorForwardVector().GetSafeNormal2D();
	const FQuat Rotation = Direction.Rotation().Quaternion();
	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(ASBarricadeGround), false, Placer);
	GroundParams.AddIgnoredActor(this);
	const float MinNormalZ = FMath::Cos(FMath::DegreesToRadians(MaxGroundSlope));
	auto FindGround = [&](const FVector& Point, FHitResult& Hit)
	{
		return World->LineTraceSingleByObjectType(Hit, FVector(Point.X, Point.Y, Origin.Z + 200.0f),
			FVector(Point.X, Point.Y, Origin.Z - 500.0f), FCollisionObjectQueryParams(ECC_WorldStatic), GroundParams)
			&& Hit.ImpactNormal.Z >= MinNormalZ;
	};
	FHitResult Ground;
	if (!FindGround(GroundSearch, Ground)) return false;
	// All four corners must have support at roughly the same height; do not build across a ledge.
	float HighestGround = Ground.ImpactPoint.Z;
	for (const float X : {-1.0f, 1.0f})
	{
		for (const float Y : {-1.0f, 1.0f})
		{
			FHitResult Corner;
			const FVector Point = GroundSearch + Rotation.RotateVector(FVector(X * (Extent.X - 1.0f), Y * (Extent.Y - 1.0f), 0.0f));
			if (!FindGround(Point, Corner)
				|| FMath::Abs(Corner.ImpactPoint.Z - Ground.ImpactPoint.Z) > MaxGroundHeightDifference) return false;
			HighestGround = FMath::Max(HighestGround, Corner.ImpactPoint.Z);
		}
	}
	const FVector Location(GroundSearch.X, GroundSearch.Y, HighestGround + Extent.Z + 1.0f);
	FCollisionQueryParams PlacementParams(SCENE_QUERY_STAT(ASBarricadePlacement), false, this);
	// Keep the placer in this query: placement must not trap the player inside the obstacle.
	if (World->OverlapBlockingTestByChannel(Location, Rotation, ECC_Pawn,
		FCollisionShape::MakeBox(Extent), PlacementParams)) return false;
	FHitResult Obstruction;
	if (World->LineTraceSingleByChannel(Obstruction, Origin, Location, ECC_Visibility, GroundParams)) return false;
	OutTransform = FTransform(Rotation, Location);
	return true;
}

void AASBarricadeActor::SetPlacementPreview(const bool bCanPlace)
{
	check(!GetIsReplicated());
	MeshComponent->SetVisibility(true);
	if (PreviewMaterials.IsEmpty() && PreviewMaterial)
	{
		for (int32 Slot = 0; Slot < MeshComponent->GetNumMaterials(); ++Slot)
		{
			PreviewMaterials.Add(MeshComponent->CreateDynamicMaterialInstance(Slot, PreviewMaterial));
		}
	}
	for (UMaterialInstanceDynamic* Material : PreviewMaterials)
	{
		if (Material) Material->SetVectorParameterValue(TEXT("Tint"), bCanPlace
			? FLinearColor(0.1f, 1.0f, 0.25f) : FLinearColor(1.0f, 0.1f, 0.1f));
	}
}

void AASBarricadeActor::ApplyPlacementState(const bool bIsActive)
{
	Super::ApplyPlacementState(bIsActive);
	CollisionComponent->SetCollisionEnabled(
		bIsActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	CollisionComponent->SetCanEverAffectNavigation(bIsActive);
	MeshComponent->SetVisibility(bIsActive);
}

float AASBarricadeActor::CalculateMonsterTargetRadius() const
{
	const FVector Extent = CollisionComponent->GetScaledBoxExtent();
	return FMath::Max(Extent.X, Extent.Y);
}
