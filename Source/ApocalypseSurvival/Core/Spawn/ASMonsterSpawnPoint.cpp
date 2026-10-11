#include "Core/Spawn/ASMonsterSpawnPoint.h"

#include "Components/ArrowComponent.h"

AASMonsterSpawnPoint::AASMonsterSpawnPoint()
{
	ArrowComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	SetRootComponent(ArrowComponent);

	bReplicates = false;
	SetActorEnableCollision(false);
}
