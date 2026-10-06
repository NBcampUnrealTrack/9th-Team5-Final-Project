#include "Map/Vision/ASBushArea.h"

#include "ApocalypseSurvival.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Map/Core/ASMapShapeUtils.h"
#include "Map/Vision/ASVisionWorldSubsystem.h"

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

	ASMapShapeUtils::GatherAreaShapes(this, AreaShapes);

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
