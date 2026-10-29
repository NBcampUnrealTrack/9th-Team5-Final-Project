#include "Skills/ASSkillFX.h"

#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

UFXSystemComponent* ASSkillFX::SpawnAtLocation(const UObject* WorldContext, UFXSystemAsset* Asset,
	const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	if (!WorldContext || !Asset)
	{
		return nullptr;
	}

	if (UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(Asset))
	{
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, NiagaraSystem, Location, Rotation, Scale);
	}

	//Cascade (구형 파티클)
	if (UParticleSystem* ParticleSystem = Cast<UParticleSystem>(Asset))
	{
		return UGameplayStatics::SpawnEmitterAtLocation(WorldContext, ParticleSystem, Location, Rotation, Scale);
	}

	return nullptr;
}
