#include "Skills/Area/ASSkillArea.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Skills/Area/ASSkillAreaActor.h"

UASSkillArea::UASSkillArea()
{
	AreaClass = AASSkillAreaActor::StaticClass();
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
		Location += Aim * ForwardOffset;
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
