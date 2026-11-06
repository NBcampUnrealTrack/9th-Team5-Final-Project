#include "Quests/World/ASQuestLootChest.h"
#include "Items/Data/ASItemDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AASQuestLootChest::AASQuestLootChest()
{
	if (UStaticMeshComponent* DisplayMesh = FindComponentByClass<UStaticMeshComponent>())
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (Shape.Succeeded()) DisplayMesh->SetStaticMesh(Shape.Object);
		DisplayMesh->SetRelativeScale3D(FVector(.8f, .6f, .5f));
		DisplayMesh->SetRelativeLocation(FVector(0, 0, 25));
		DisplayMesh->SetCanEverAffectNavigation(false);
	}
}

void AASQuestLootChest::PostInitializeComponents()
{
	if (HasAuthority())
	{
		if (UASItemDefinition* Item = QuestItem.LoadSynchronous())
		{
			if (Item->ItemType == EASItemType::Quest && QuestItemQuantity > 0)
			{
				FASItemGrant Grant;
				Grant.ItemDefinition = Item; Grant.Quantity = QuestItemQuantity;
				SetInitialLoot({Grant});
			}
		}
		if (!ChestDisplayName.IsEmpty()) SetLootDisplayName(ChestDisplayName);
	}
	Super::PostInitializeComponents();
}

void AASQuestLootChest::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority() || RandomSelectionGroup.IsNone()) return;
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (bSelectionResolved) return;
		TArray<AASQuestLootChest*> Candidates;
		for (TActorIterator<AASQuestLootChest> It(GetWorld()); It; ++It)
			if (It->RandomSelectionGroup == RandomSelectionGroup) Candidates.Add(*It);
		if (Candidates.IsEmpty()) return;
		AASQuestLootChest* Chosen = Candidates[FMath::RandHelper(Candidates.Num())];
		for (AASQuestLootChest* Candidate : Candidates)
		{
			Candidate->bSelectionResolved = true;
			if (Candidate != Chosen) Candidate->Destroy();
		}
	}));
}
