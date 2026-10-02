#include "Characters/Player/ASPlayerCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Core/Framework/ASPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ASAbilitySystemComponent.h"

AASPlayerCharacter::AASPlayerCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// The camera never rotates, so the character turns towards where it is walking.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->RotationRate = FRotator(0.0f, 1080.0f, 0.0f);
	MoveComp->bConstrainToPlane = true;
	MoveComp->bSnapToPlaneAtStart = true;

	// Snappy starts and stops, close to how a MOBA character responds to a click.
	MoveComp->MaxAcceleration = 8192.0f;
	MoveComp->BrakingDecelerationWalking = 8192.0f;

	// Path following has to drive the character through acceleration. Velocity
	// driven path moves are not part of the saved moves a client sends to the
	// server, which makes the server correct the owning client every frame.
	MoveComp->GetNavMovementProperties()->bUseAccelerationForPaths = true;
}

void AASPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitAbilitySystemFromPlayerState();
}

void AASPlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitAbilitySystemFromPlayerState();
}

void AASPlayerCharacter::InitAbilitySystemFromPlayerState()
{
	AASPlayerState* ASPlayerState = GetPlayerState<AASPlayerState>();
	if (!IsValid(ASPlayerState))
	{
		return;
	}

	InitAbilitySystem(ASPlayerState->GetASAbilitySystemComponent(), ASPlayerState->GetAttributeSet(), ASPlayerState);
}
