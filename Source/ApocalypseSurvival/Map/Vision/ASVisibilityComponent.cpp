#include "Map/Vision/ASVisibilityComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Map/Vision/ASVisionDefinition.h"
#include "Map/Vision/ASFogOfWarManager.h"
#include "Map/Vision/ASVisionFunctionLibrary.h"
#include "Map/Vision/ASVisionWorldSubsystem.h"
#include "Net/UnrealNetwork.h"

UASVisibilityComponent::UASVisibilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UASVisibilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UASVisibilityComponent, VisibleTeamMask);
}

void UASVisibilityComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this))
	{
		Subsystem->RegisterVisibilityTarget(this);
	}

	if (AActor* Owner = GetOwner(); Owner && Owner->HasAuthority())
	{
		bOwnerAlwaysRelevantByDefault = Owner->bAlwaysRelevant;
		if (bAlwaysVisible)
		{
			VisibleTeamMask = 0xFF;
			if (bRelevantWhileTeamVisible)
			{
				Owner->bAlwaysRelevant = true;
			}
		}
	}

	UpdateLocalPresentation(true);
}

void UASVisibilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this))
	{
		Subsystem->UnregisterVisibilityTarget(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UASVisibilityComponent::ServerSetVisibleTeamMask(uint8 NewMask)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (bAlwaysVisible)
	{
		NewMask = 0xFF;
	}

	if (VisibleTeamMask != NewMask)
	{
		VisibleTeamMask = NewMask;

		// 팀 시야는 공유된다. 팀원 한 명이 보는 대상은 모든 팀원에게 복제되어야 한다.
		if (bRelevantWhileTeamVisible)
		{
			GetOwner()->bAlwaysRelevant = bOwnerAlwaysRelevantByDefault || NewMask != 0;
		}

		// 서버에서는 OnRep가 실행되지 않는다. 리슨 서버 호스트도 갱신이 필요하다.
		UpdateLocalPresentation(false);
	}
}

void UASVisibilityComponent::NotifyAttackStarted(float RevealSeconds)
{
	const UWorld* World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (RevealSeconds < 0.0f)
	{
		const UASVisionWorldSubsystem* Subsystem = UASVisionWorldSubsystem::Get(this);
		const AASFogOfWarManager* Manager = Subsystem ? Subsystem->GetFogOfWarManager() : nullptr;
		const UASVisionDefinition* Definition = Manager ? Manager->GetVisionDefinition() : nullptr;
		RevealSeconds = Definition ? Definition->Settings.AttackRevealSeconds : 3.0f;
	}

	const AGameStateBase* GameState = World->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
	RevealEndServerTime = FMath::Max(RevealEndServerTime, Now + RevealSeconds);
}

bool UASVisibilityComponent::IsVisibleToTeam(int32 TeamId) const
{
	if (bAlwaysVisible)
	{
		return true;
	}

	return TeamId >= 0 && TeamId < 8 && (VisibleTeamMask & (1 << TeamId)) != 0;
}

bool UASVisibilityComponent::IsVisibleToLocalTeam() const
{
	return IsVisibleToTeam(UASVisionFunctionLibrary::GetLocalTeamId(this));
}

void UASVisibilityComponent::RefreshLocalPresentation()
{
	UpdateLocalPresentation(true);
}

void UASVisibilityComponent::OnRep_VisibleTeamMask()
{
	UpdateLocalPresentation(false);
}

void UASVisibilityComponent::UpdateLocalPresentation(bool bForce)
{
	// 전용 서버의 화면을 보는 사람은 없다.
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const bool bVisible = IsVisibleToLocalTeam();
	if (!bForce && bPresentationInitialized && bVisible == bLocallyVisible)
	{
		return;
	}

	const bool bChanged = !bPresentationInitialized || bVisible != bLocallyVisible;
	bLocallyVisible = bVisible;
	bPresentationInitialized = true;

	if (bHideOwnerWhenNotVisible)
	{
		SetOwnerPresentationVisible(bVisible);
	}

	if (bChanged)
	{
		OnLocalVisibilityChanged.Broadcast(bVisible);
	}
}

void UASVisibilityComponent::SetOwnerPresentationVisible(bool bVisible)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (bVisible)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Primitive : HiddenPrimitives)
		{
			if (UPrimitiveComponent* Resolved = Primitive.Get())
			{
				Resolved->SetVisibility(true, false);
			}
		}
		HiddenPrimitives.Reset();
		return;
	}

	TArray<AActor*> Actors;
	Actors.Add(Owner);
	TArray<AActor*> AttachedActors;
	Owner->GetAttachedActors(AttachedActors, true, true);
	Actors.Append(AttachedActors);

	for (AActor* Actor : Actors)
	{
		TArray<UPrimitiveComponent*> Primitives;
		Actor->GetComponents<UPrimitiveComponent>(Primitives);

		for (UPrimitiveComponent* Primitive : Primitives)
		{
			// 현재 표시된 것만 숨겨 복원 시 의도적으로 숨긴 부분이 드러나지 않게 한다.
			if (!Primitive || !Primitive->IsVisible())
			{
				continue;
			}

			if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Primitive))
			{
				// 숨김이 히트 판정에 영향을 주지 않도록 리슨 서버 호스트에서 본을 계속 갱신한다.
				if (Owner->HasAuthority())
				{
					SkeletalMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
				}
			}

			Primitive->SetVisibility(false, false);
			HiddenPrimitives.AddUnique(Primitive);
		}
	}
}
