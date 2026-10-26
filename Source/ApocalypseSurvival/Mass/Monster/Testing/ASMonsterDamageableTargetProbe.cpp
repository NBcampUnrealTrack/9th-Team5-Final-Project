#include "Mass/Monster/Testing/ASMonsterDamageableTargetProbe.h"

#include "Components/StaticMeshComponent.h"
#include "Mass/Monster/Targeting/ASMonsterTargetableComponent.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ASMonsterDamageableTargetProbe)

AASMonsterDamageableTargetProbe::AASMonsterDamageableTargetProbe()
{
	bReplicates = true;
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetIsReplicated(true);

	MonsterTargetableComponent = CreateDefaultSubobject<UASMonsterTargetableComponent>(TEXT("MonsterTargetableComponent"));
	MonsterTargetableComponent->ConfigureTargetProfile(
		EASMonsterTargetCapability::BlockingTarget,
		0.0f, 100.0f);
}

void AASMonsterDamageableTargetProbe::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		CurrentHealth = FMath::Max(1.0f, MaxHealth);
		bDestroyedByMonster = false;
	}
	ApplyDestroyedState();
}

void AASMonsterDamageableTargetProbe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AASMonsterDamageableTargetProbe, CurrentHealth);
	DOREPLIFETIME(AASMonsterDamageableTargetProbe, bDestroyedByMonster);
}

bool AASMonsterDamageableTargetProbe::CanReceiveMonsterDamage_Implementation() const
{
	return !bDestroyedByMonster && CurrentHealth > 0.0f;
}

bool AASMonsterDamageableTargetProbe::IsAliveForMonsterTargeting_Implementation() const
{
	return CanReceiveMonsterDamage_Implementation();
}

void AASMonsterDamageableTargetProbe::OnRep_DestroyedByMonster()
{
	ApplyDestroyedState();
}

void AASMonsterDamageableTargetProbe::ApplyDestroyedState()
{
	if (!bDestroyedByMonster)
	{
		return;
	}

	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetHiddenInGame(true);
}

void AASMonsterDamageableTargetProbe::ApplyMonsterDamage_Implementation(const FASMonsterDamageContext& Context)
{
	if (!HasAuthority() || !CanReceiveMonsterDamage_Implementation() || Context.Damage <= 0.0f)
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Context.Damage);
	if (CurrentHealth <= 0.0f)
	{
		bDestroyedByMonster = true;
		MonsterTargetableComponent->SetTargetingEnabled(false);
		ApplyDestroyedState();
	}
}
