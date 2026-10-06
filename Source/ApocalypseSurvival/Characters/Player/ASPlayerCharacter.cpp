#include "Characters/Player/ASPlayerCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Core/Framework/ASPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/ASAbilitySystemComponent.h"

AASPlayerCharacter::AASPlayerCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// 카메라가 회전하지 않으므로 캐릭터는 컨트롤러 방향이 아니라 걷는 방향을 본다.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->RotationRate = FRotator(0.0f, 1080.0f, 0.0f);
	MoveComp->bConstrainToPlane = true;
	MoveComp->bSnapToPlaneAtStart = true;

	// 클릭하면 바로 출발하고 바로 멈추게 한다. 롤 계열 캐릭터의 반응에 맞춘 값이다.
	MoveComp->MaxAcceleration = 8192.0f;
	MoveComp->BrakingDecelerationWalking = 8192.0f;

	// 길찾기 이동을 "가속(입력)"으로 굴리게 한다.
	// 기본값은 속도를 직접 넣는 방식인데, 클라이언트가 서버로 보내는 이동 기록에는
	// 그 속도가 들어가지 않는다. 그러면 서버는 캐릭터가 서 있다고 계산해서
	// 본인 클라이언트의 위치를 매 프레임 되돌려 놓는다.
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

	// ASC 의 주인은 PlayerState, 아바타는 이 캐릭터다.
	InitAbilitySystem(ASPlayerState->GetASAbilitySystemComponent(), ASPlayerState);
}
