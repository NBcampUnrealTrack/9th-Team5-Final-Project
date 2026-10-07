#include "MapSystem/Vision/ASVisionWorldSubsystem.h"

#include "Engine/World.h"
#include "MapSystem/Vision/ASBushArea.h"
#include "MapSystem/Vision/ASFogOfWarManager.h"
#include "MapSystem/Vision/ASVisibilityComponent.h"
#include "MapSystem/Vision/ASVisionSourceComponent.h"

UASVisionWorldSubsystem* UASVisionWorldSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UASVisionWorldSubsystem>() : nullptr;
}

bool UASVisionWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

void UASVisionWorldSubsystem::RegisterBush(AASBushArea* Bush)
{
	if (Bush)
	{
		Bushes.AddUnique(Bush);
	}
}

void UASVisionWorldSubsystem::UnregisterBush(AASBushArea* Bush)
{
	Bushes.Remove(Bush);
}

TArray<AASBushArea*> UASVisionWorldSubsystem::GetBushes() const
{
	return ResolveWeak(Bushes);
}

FName UASVisionWorldSubsystem::FindBushIdAtLocation(const FVector& Location) const
{
	for (const TWeakObjectPtr<AASBushArea>& Bush : Bushes)
	{
		if (Bush.IsValid() && Bush->ContainsLocation(Location))
		{
			return Bush->GetBushId();
		}
	}
	return NAME_None;
}

void UASVisionWorldSubsystem::RegisterVisionSource(UASVisionSourceComponent* Source)
{
	if (Source)
	{
		VisionSources.AddUnique(Source);
	}
}

void UASVisionWorldSubsystem::UnregisterVisionSource(UASVisionSourceComponent* Source)
{
	VisionSources.Remove(Source);
}

TArray<UASVisionSourceComponent*> UASVisionWorldSubsystem::GetVisionSources() const
{
	return ResolveWeak(VisionSources);
}

void UASVisionWorldSubsystem::RegisterVisibilityTarget(UASVisibilityComponent* Target)
{
	if (Target)
	{
		VisibilityTargets.AddUnique(Target);
	}
}

void UASVisionWorldSubsystem::UnregisterVisibilityTarget(UASVisibilityComponent* Target)
{
	VisibilityTargets.Remove(Target);
}

TArray<UASVisibilityComponent*> UASVisionWorldSubsystem::GetVisibilityTargets() const
{
	return ResolveWeak(VisibilityTargets);
}

void UASVisionWorldSubsystem::SetFogOfWarManager(AASFogOfWarManager* Manager)
{
	if (!Manager || !FogOfWarManager.IsValid())
	{
		FogOfWarManager = Manager;
	}
}
