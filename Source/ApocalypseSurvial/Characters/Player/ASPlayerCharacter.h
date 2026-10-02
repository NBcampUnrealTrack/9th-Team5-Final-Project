#pragma once

#include "CoreMinimal.h"
#include "Characters/ASCharacterBase.h"
#include "ASPlayerCharacter.generated.h"

/**
 * Player-controlled character.
 *
 * Movement is point-and-click: AASPlayerController path-follows on the owning
 * machine and the movement component replicates the result like normal input.
 * The character has no camera of its own; AASPlayerCameraManager provides the
 * view so it can move independently of the pawn.
 */
UCLASS(Abstract)
class APOCALYPSESURVIAL_API AASPlayerCharacter : public AASCharacterBase
{
	GENERATED_BODY()

public:
	AASPlayerCharacter();

	/** Server: the PlayerState is already valid when the pawn is possessed. */
	virtual void PossessedBy(AController* NewController) override;

	/** Clients: the PlayerState arrives through replication. */
	virtual void OnRep_PlayerState() override;

private:
	void InitAbilitySystemFromPlayerState();
};
