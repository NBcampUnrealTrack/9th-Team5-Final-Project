#include "Skills/Area/ASSkillArea.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Skills/Data/ASSkillData.h"
#include "Skills/Area/ASSkillAreaActor.h"

UASSkillArea::UASSkillArea()
{
	AreaClass = AASSkillAreaActor::StaticClass();

	//기본은 원하는 곳에 깔기
	TargetingType = EASSkillTargetingType::Location;
}

void UASSkillArea::OnSkillEffect_Implementation()
{
	if (!HasSkillAuthority())
	{
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	FVector Aim = GetAimDirection().GetSafeNormal2D();
	if (Aim.IsNearlyZero())
	{
		Aim = Avatar->GetActorForwardVector().GetSafeNormal2D();
	}

	FVector Location = Avatar->GetActorLocation();
	if (!bAttachToOwner)
	{
		Location = TargetingType == EASSkillTargetingType::Location
			? GetTargetLocation()
			: Location + Aim * ForwardOffset;
	}

	const FTransform SpawnTransform(Aim.Rotation(), Location);
	UClass* ClassToSpawn = AreaClass ? AreaClass.Get() : AASSkillAreaActor::StaticClass();

	AASSkillAreaActor* Area = World->SpawnActorDeferred<AASSkillAreaActor>(
		ClassToSpawn, SpawnTransform, Avatar, Cast<APawn>(Avatar), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Area)
	{
		return;
	}

	Area->InitArea(MakeDamageSpec(), Avatar, Radius, Duration, TickInterval, bDamageOnSpawn, bCanHitPlayers, bDrawDebug);
	Area->FinishSpawning(SpawnTransform);

	if (bAttachToOwner)
	{
		Area->AttachToActor(Avatar, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void UASSkillArea::ApplySkillStats(const FASSkillStatsRow& Stats)
{
	Super::ApplySkillStats(Stats);

	Radius = Stats.AreaRadius;
	Duration = Stats.Duration;
	TickInterval = Stats.TickInterval;
}

FASSkillIndicatorSettings UASSkillArea::GetIndicatorSettings_Implementation() const
{
	FASSkillIndicatorSettings Settings = Super::GetIndicatorSettings_Implementation();
	Settings.Shape = EASSkillIndicatorShape::Circle;
	Settings.Radius = Radius;

	//시전자를 따라다니면 시전자 중심 원
	if (bAttachToOwner)
	{
		Settings.TargetingType = EASSkillTargetingType::Self;
		Settings.RangeRingRadius = 0.0f;
	}
	else if (Settings.TargetingType == EASSkillTargetingType::Direction)
	{
		Settings.ForwardOffset = ForwardOffset;
		Settings.RangeRingRadius = FMath::Max(0.0f, ForwardOffset) + Radius;
	}

	return Settings;
}
