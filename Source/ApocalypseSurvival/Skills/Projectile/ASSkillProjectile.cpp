#include "Skills/Projectile/ASSkillProjectile.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Skills/Projectile/ASSkillProjectileActor.h"

UASSkillProjectile::UASSkillProjectile()
{
	ProjectileClass = AASSkillProjectileActor::StaticClass();
}

void UASSkillProjectile::OnSkillEffect_Implementation()
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

	FVector Location = Avatar->GetActorLocation() + Aim * SpawnForwardOffset + FVector(0.0f, 0.0f, SpawnHeightOffset);

	if (!SpawnSocketName.IsNone())
	{
		if (const ACharacter* Character = Cast<ACharacter>(Avatar))
		{
			const USkeletalMeshComponent* Mesh = Character->GetMesh();
			if (Mesh && Mesh->DoesSocketExist(SpawnSocketName))
			{
				Location = Mesh->GetSocketLocation(SpawnSocketName);
			}
		}
	}

	const FTransform SpawnTransform(Aim.Rotation(), Location);
	UClass* ClassToSpawn = ProjectileClass ? ProjectileClass.Get() : AASSkillProjectileActor::StaticClass();

	AASSkillProjectileActor* Projectile = World->SpawnActorDeferred<AASSkillProjectileActor>(
		ClassToSpawn, SpawnTransform, Avatar, Cast<APawn>(Avatar), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Projectile)
	{
		return;
	}

	Projectile->InitProjectile(MakeDamageSpec(), Avatar, Aim, Speed, MaxRange, PierceCount, bCanHitPlayers);
	Projectile->FinishSpawning(SpawnTransform);
}
