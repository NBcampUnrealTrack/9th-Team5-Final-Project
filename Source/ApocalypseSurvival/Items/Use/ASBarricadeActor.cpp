#include "Items/Use/ASBarricadeActor.h"

#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Items/GAS/ASItemEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavAreas/NavArea_Null.h"
#include "Net/UnrealNetwork.h"
#include "Skills/ASSkillTags.h"
#include "UObject/ConstructorHelpers.h"

AASBarricadeActor::AASBarricadeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
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
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	HealthAttributes = CreateDefaultSubobject<UASHealthAttributeSet>(TEXT("HealthAttributes"));
	SetCanBeDamaged(false);
}

UAbilitySystemComponent* AASBarricadeActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AASBarricadeActor::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystemComponent->AddAttributeSetSubobject(HealthAttributes.Get());
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthChanged);
	UpdateCollision();
}

void AASBarricadeActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UASHealthAttributeSet::GetHealthAttribute()).RemoveAll(this);
	CollisionComponent->SetCanEverAffectNavigation(false);
	Super::EndPlay(EndPlayReason);
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

bool AASBarricadeActor::PreparePlacement(const float MaxHealth, const float Lifetime)
{
	if (!HasAuthority() || bPrepared || bPlaced || !FMath::IsFinite(MaxHealth) || MaxHealth <= 0.0f
		|| !FMath::IsFinite(Lifetime) || Lifetime < 0.0f) return false;
	AbilitySystemComponent->SetNumericAttributeBase(UASHealthAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UASHealthAttributeSet::GetHealthAttribute(), MaxHealth);
	PlacedLifetime = Lifetime;
	bPrepared = true;
	return true;
}

void AASBarricadeActor::FinishPlacement()
{
	if (!HasAuthority() || !bPrepared || bPlaced) return;
	bPlaced = true;
	UpdateCollision();
	if (PlacedLifetime > 0.0f) SetLifeSpan(PlacedLifetime);
	ForceNetUpdate();
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

float AASBarricadeActor::GetHealth() const
{
	return HealthAttributes->GetHealth();
}

bool AASBarricadeActor::IsIntact() const
{
	return bPlaced && !IsActorBeingDestroyed() && GetHealth() > 0.0f;
}

float AASBarricadeActor::TakeDamage(const float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || !IsIntact() || !CanBeDamaged() || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f) return 0.0f;
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!IsIntact() || !FMath::IsFinite(ActualDamage) || ActualDamage <= 0.0f) return 0.0f;
	const float AppliedDamage = FMath::Min(GetHealth(), ActualDamage);
	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	Context.AddInstigator(EventInstigator ? EventInstigator->GetPawn() : DamageCauser, DamageCauser);
	const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(UASItemDamageEffect::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid()) return 0.0f;
	Spec.Data->SetSetByCallerMagnitude(ASGameplayTags::Data_Damage, -AppliedDamage);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	return AppliedDamage;
}

void AASBarricadeActor::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (HasAuthority() && bPlaced && Data.NewValue <= 0.0f)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionComponent->SetCanEverAffectNavigation(false);
		Destroy();
	}
}

void AASBarricadeActor::UpdateCollision()
{
	CollisionComponent->SetCollisionEnabled(bPlaced ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	CollisionComponent->SetCanEverAffectNavigation(bPlaced);
	MeshComponent->SetVisibility(bPlaced);
	SetCanBeDamaged(bPlaced);
}

void AASBarricadeActor::OnRep_Placed()
{
	UpdateCollision();
}

void AASBarricadeActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AASBarricadeActor, bPlaced);
}
