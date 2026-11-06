#include "MapSystem/Vision/ASBushArea.h"

#include "ApocalypseSurvival.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "MapSystem/Shared/ASMapShapeUtils.h"
#include "MapSystem/Shared/ASMapPolygonAreaComponent.h"
#include "MapSystem/Vision/ASFogOfWarManager.h"
#include "MapSystem/Vision/ASVisionDefinition.h"
#include "MapSystem/Vision/ASVisionWorldSubsystem.h"

AASBushArea::AASBushArea()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MainArea = CreateDefaultSubobject<UBoxComponent>(TEXT("MainArea"));
	MainArea->SetupAttachment(SceneRoot);
	MainArea->SetBoxExtent(FVector(200.0, 200.0, 150.0));
	ASMapShapeUtils::ConfigureAreaShape(MainArea, FColor(60, 220, 60));
}

FName AASBushArea::GetBushId() const
{
	return BushId.IsNone() ? GetFName() : BushId;
}

bool AASBushArea::ContainsLocation(const FVector& Location) const
{
	return ASMapShapeUtils::AnyShapeContainsLocation(AreaShapes, Location, bIgnoreHeight);
}

void AASBushArea::BeginPlay()
{
	Super::BeginPlay();

	ASMapShapeUtils::GatherAreaComponents(this, AreaShapes);
	ASMapShapeUtils::DisableAreaShapeCollision(AreaShapes);
	for (const TWeakObjectPtr<UPrimitiveComponent>& Component : AreaShapes)
	{
		if (const UASMapPolygonAreaComponent* Polygon = Cast<UASMapPolygonAreaComponent>(Component.Get()))
		{
			FString Error;
			if (!Polygon->ValidateArea(Error))
			{
				UE_LOG(LogApocalypseSurvival, Warning, TEXT("Bush '%s', component '%s': %s"),
					*GetBushId().ToString(), *Polygon->GetName(), *Error);
			}
		}
	}

	if (UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this))
	{
		for (const AASBushArea* Other : Subsystem->GetBushes())
		{
			if (Other != this && Other->GetBushId() == GetBushId())
			{
				UE_LOG(LogApocalypseSurvival, Warning, TEXT("Bush id '%s' is used by %s and %s; they act as one bush."),
					*GetBushId().ToString(), *GetName(), *Other->GetName());
			}
		}

		Subsystem->RegisterBush(this);
		const AASFogOfWarManager* Manager = Subsystem->GetFogOfWarManager();
		if (Manager && Manager->HasAuthority())
		{
			if (const UASVisionDefinition* Definition = Manager->GetVisionDefinition())
			{
				TArray<FString> Errors;
				if (Definition->ValidateDefinition(Errors))
				{
					ValidateVisionWalls(*Definition);
				}
			}
		}
	}
}

void AASBushArea::ValidateVisionWalls(const UASVisionDefinition& Definition)
{
	UWorld* World = GetWorld();
	if (bVisionWallsValidated || !World)
	{
		return;
	}
	bVisionWallsValidated = true;

	FBox AreaBounds(ForceInit);
	for (const TWeakObjectPtr<UPrimitiveComponent>& WeakComponent : AreaShapes)
	{
		if (const UPrimitiveComponent* Component = WeakComponent.Get())
		{
			AreaBounds += Component->Bounds.GetBox();
		}
	}
	if (!AreaBounds.IsValid)
	{
		return;
	}
	const double MinZ = AreaBounds.Min.Z;
	const double MaxZ = AreaBounds.Max.Z + FMath::Max(0.0f, Definition.Settings.TraceHeightOffset);
	const double CenterZ = (MinZ + MaxZ) * 0.5;
	const double ExtentZ = (MaxZ - MinZ) * 0.5;
	const ECollisionChannel WallChannel = Definition.Settings.VisionWallChannel;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ASBushVisionWalls), false, this);
	Params.bIgnoreTouches = true;

	TArray<TArray<FVector2D>> Outlines;
	ASMapShapeUtils::GetAreaOutlines(AreaShapes, Outlines);
	TSet<const AActor*> WarnedActors;
	for (const TArray<FVector2D>& Outline : Outlines)
	{
		if (Outline.Num() < 3)
		{
			continue;
		}

		FBox2D Bounds(ForceInit);
		for (const FVector2D& Point : Outline)
		{
			Bounds += Point;
		}
		const FVector2D Center = Bounds.GetCenter();
		const FVector2D Extent = Bounds.GetExtent();
		if (Extent.X <= 0.0 || Extent.Y <= 0.0)
		{
			continue;
		}

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, FVector(Center.X, Center.Y, CenterZ), FQuat::Identity,
			WallChannel, FCollisionShape::MakeBox(FVector(Extent.X, Extent.Y, ExtentZ)), Params);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			const AActor* WallActor = Overlap.GetActor();
			const UPrimitiveComponent* Component = Overlap.GetComponent();
			if (!Overlap.bBlockingHit || !WallActor || WallActor == this || !Component || WarnedActors.Contains(WallActor))
			{
				continue;
			}

			const FBox ComponentBounds = Component->Bounds.GetBox();
			const TArray<FVector2D> ComponentOutline = {
				FVector2D(ComponentBounds.Min.X, ComponentBounds.Min.Y), FVector2D(ComponentBounds.Max.X, ComponentBounds.Min.Y),
				FVector2D(ComponentBounds.Max.X, ComponentBounds.Max.Y), FVector2D(ComponentBounds.Min.X, ComponentBounds.Max.Y)
			};
			if (ASMapShapeUtils::OutlinesOverlap(Outline, ComponentOutline))
			{
				WarnedActors.Add(WallActor);
				UE_LOG(LogApocalypseSurvival, Warning, TEXT("Bush '%s' contains vision wall '%s' (bounds overlap); walls inside bushes are ignored for Mass monster concealment."),
					*GetBushId().ToString(), *WallActor->GetName());
			}
		}
	}
}

void AASBushArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this))
	{
		Subsystem->UnregisterBush(this);
	}

	Super::EndPlay(EndPlayReason);
}
