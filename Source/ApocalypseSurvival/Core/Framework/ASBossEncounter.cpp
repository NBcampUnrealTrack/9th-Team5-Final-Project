#include "Core/Framework/ASBossEncounter.h"

#include "Characters/Enemy/ASMonsterCharacter.h"
#include "Core/Framework/ASGameMode.h"
#include "Core/Framework/ASGameState.h"
#include "Core/Framework/ASPlayerCameraManager.h"
#include "Core/Framework/ASPlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Brushes/SlateColorBrush.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "GAS/ASAbilitySystemComponent.h"
#include "GAS/Attributes/ASHealthAttributeSet.h"
#include "MapSystem/Vision/ASFogDisplayComponent.h"
#include "MapSystem/Vision/ASFogOfWarManager.h"
#include "MapSystem/Vision/ASVisibilityComponent.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	class SASBossIntroOverlay : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SASBossIntroOverlay) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&, const TSharedRef<SWidget>& Content) { ChildSlot[Content]; }
		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override { return FReply::Handled(); }
		virtual FReply OnKeyUp(const FGeometry&, const FKeyEvent&) override { return FReply::Handled(); }
		virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
		virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
	};
}

AASBossEncounter::AASBossEncounter()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AASBossEncounter::InitializeBoss(AASMonsterCharacter* InBoss)
{
	check(HasAuthority());
	Boss = InBoss;
	PreparationStartTime = FPlatformTime::Seconds();
	bBossWasAlwaysRelevant = Boss->bAlwaysRelevant;
	Boss->bAlwaysRelevant = true;
	Boss->ForceNetUpdate();
#if !UE_BUILD_SHIPPING
	if (const UASAbilitySystemComponent* Abilities = Boss->GetASAbilitySystemComponent())
	{
		UE_LOG(LogTemp, Log, TEXT("[BossDebug][Spawn] Boss=%s Class=%s Health=%.2f Max=%.2f"),
			*Boss->GetName(), *Boss->GetClass()->GetName(),
			Abilities->GetNumericAttribute(UASHealthAttributeSet::GetHealthAttribute()),
			Abilities->GetNumericAttribute(UASHealthAttributeSet::GetMaxHealthAttribute()));
	}
#endif
	ForceNetUpdate();
	OnRep_Boss();
}

void AASBossEncounter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AASBossEncounter, Boss);
}

void AASBossEncounter::OnRep_Boss()
{
	AASPlayerController* Player = Cast<AASPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!IsValid(Boss) || !Player || !Player->IsLocalController() || !Player->PlayerState || bAcknowledged) return;
	LocalPlayer = Player;
	bAcknowledged = true;
	// Acknowledging the actor reference ensures the boss exists on this client before the pause.
	Player->ServerAcknowledgeBossIntro(this);
}

void AASBossEncounter::AcknowledgePlayer(AASPlayerController* Player)
{
	if (HasAuthority() && !bIntroStarted && IsValid(Player) && Player->GetWorld() == GetWorld())
	{
		ReadyPlayers.Add(Player);
	}
}

bool AASBossEncounter::ArePlayersReady() const
{
	bool bHasPlayers = false;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AASPlayerController* Player = Cast<AASPlayerController>(It->Get());
		if (!Player) continue;
		bHasPlayers = true;
		if (!ReadyPlayers.Contains(Player)) return false;
	}
	return bHasPlayers;
}

float AASBossEncounter::GetHealthFill(float Elapsed)
{
	return FMath::Clamp((Elapsed - 0.75f) / 2.0f, 0.0f, 1.0f);
}

float AASBossEncounter::GetCameraBlend(float Elapsed)
{
	return Elapsed < 3.75f ? FMath::Clamp(Elapsed / 0.75f, 0.0f, 1.0f)
		: FMath::Clamp((IntroDuration - Elapsed) / 0.75f, 0.0f, 1.0f);
}

void AASBossEncounter::MulticastBeginIntro_Implementation()
{
	bIntroStarted = true;
	IntroStartTime = FPlatformTime::Seconds();
	AWorldSettings* Settings = GetWorld()->GetWorldSettings();
	bPreviousCameraMoveableWhenPaused = GetWorld()->bIsCameraMoveableWhenPaused;
	bCameraPauseStateSaved = true;
	GetWorld()->bIsCameraMoveableWhenPaused = true;
	PreviousPauser = Settings->GetPauserPlayerState();
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	IntroPauser = Player ? Player->PlayerState : nullptr;
	Settings->SetPauserPlayerState(IntroPauser.Get());
	if (!LocalPlayer.IsValid()) return;

	bLocalIntroActive = true;
	LocalPlayer->SetBossIntroInputBlocked(true);
	if (AASPlayerCameraManager* Camera = LocalPlayer->GetASPlayerCameraManager())
	{
		Camera->BeginBossIntro(Boss->GetActorLocation());
	}
	if (UASVisibilityComponent* Visibility = Boss->FindComponentByClass<UASVisibilityComponent>())
	{
		Visibility->SetBossIntroVisible(true);
	}
	for (TActorIterator<AASFogOfWarManager> It(GetWorld()); It; ++It)
	{
		if (UASFogDisplayComponent* Fog = It->FindComponentByClass<UASFogDisplayComponent>())
		{
			Fog->SetBossIntroActive(true, Boss->GetActorLocation());
			FogDisplays.Add(Fog);
		}
	}

	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!Viewport) return;
	BuildHealthBar();
	Viewport->AddViewportWidgetContent(HealthBar.ToSharedRef(), 50);
	PreviousKeyboardFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
	FSlateApplication::Get().SetKeyboardFocus(HealthBar);
	UE_LOG(LogTemp, Log, TEXT("[Boss] Introduction started; world paused."));
}

void AASBossEncounter::BuildHealthBar()
{
	// Use runtime brushes instead of the application's editor-dependent progress bar style.
	static const FProgressBarStyle BossHealthStyle = FProgressBarStyle()
		.SetBackgroundImage(FSlateColorBrush(FLinearColor(0.08f, 0.08f, 0.08f, 1.0f)))
		.SetFillImage(FSlateColorBrush(FLinearColor::White));
	HealthBar = SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 35.0f)
	[
		SNew(SBox).WidthOverride(720.0f)
		[
			SNew(SBorder).Padding(12.0f).BorderBackgroundColor(FLinearColor(0.03f, 0.03f, 0.03f, 0.95f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[ SAssignNew(BossNameText, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 22)).ColorAndOpacity(FLinearColor::White) ]
				+ SVerticalBox::Slot().AutoHeight()
				[ SNew(SBox).HeightOverride(22.0f)
					[ SAssignNew(HealthProgress, SProgressBar).Style(&BossHealthStyle)
						.BarFillStyle(EProgressBarFillStyle::Scale).Percent(0.0f)
						.FillColorAndOpacity(FLinearColor(0.65f, 0.015f, 0.02f)) ] ]
			]
		]
	];
	HealthBar = SNew(SASBossIntroOverlay, HealthBar.ToSharedRef());
	if (IsValid(Boss))
	{
		Boss->OnMonsterDisplayNameChanged.AddUniqueDynamic(this, &AASBossEncounter::HandleMonsterDisplayNameChanged);
	}
	HandleMonsterDisplayNameChanged(IsValid(Boss) ? Boss->GetMonsterDisplayName() : FText::GetEmpty());
}

void AASBossEncounter::HandleMonsterDisplayNameChanged(FText NewDisplayName)
{
	if (BossNameText) BossNameText->SetText(NewDisplayName.IsEmpty() ? FText::FromString(TEXT("BOSS")) : NewDisplayName);
}

void AASBossEncounter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bAcknowledged) OnRep_Boss();
	if (HasAuthority() && !bIntroStarted)
	{
		if (ArePlayersReady()) MulticastBeginIntro();
		else if (FPlatformTime::Seconds() - PreparationStartTime > 15.0)
		{
			UE_LOG(LogTemp, Error, TEXT("[Boss] Introduction cancelled: a client did not acknowledge the boss."));
			Destroy();
			return;
		}
	}
	if (bIntroStarted && !bIntroFinished)
	{
		const float Elapsed = static_cast<float>(FPlatformTime::Seconds() - IntroStartTime);
		if (bLocalIntroActive) UpdateLocalIntro(Elapsed);
		if (HasAuthority() && Elapsed >= IntroDuration) MulticastEndIntro();
	}
	else if (bIntroFinished && IsValid(Boss))
	{
		if (const UASAbilitySystemComponent* Abilities = Boss->GetASAbilitySystemComponent())
		{
			const float MaxHealth = Abilities->GetNumericAttribute(UASHealthAttributeSet::GetMaxHealthAttribute());
			const float Health = Abilities->GetNumericAttribute(UASHealthAttributeSet::GetHealthAttribute());
			if (HealthProgress) HealthProgress->SetPercent(MaxHealth > 0.0f ? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) : 0.0f);
#if !UE_BUILD_SHIPPING
			if (!FMath::IsNearlyEqual(Health, LastLoggedHealth) || !FMath::IsNearlyEqual(MaxHealth, LastLoggedMaxHealth))
			{
				UE_LOG(LogTemp, Log, TEXT("[BossDebug][UIHealth] Boss=%s NetMode=%d Health=%.2f Max=%.2f Percent=%.2f WidgetValid=%d"),
					*Boss->GetName(), static_cast<int32>(GetNetMode()), Health, MaxHealth,
					MaxHealth > 0.0f ? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) * 100.0f : 0.0f, HealthProgress.IsValid());
				LastLoggedHealth = Health;
				LastLoggedMaxHealth = MaxHealth;
			}
#endif
		}
	}
	if (HasAuthority())
	{
		const AASGameState* State = GetWorld()->GetGameState<AASGameState>();
		if (IsValid(Boss) && Boss->IsDead() && !bDeathReported)
		{
			bDeathReported = true;
			if (AASGameMode* Mode = GetWorld()->GetAuthGameMode<AASGameMode>()) Mode->OnBossDefeated();
		}
		if (!IsValid(Boss) || Boss->IsDead() || (State && State->GetCurrentPhase() != EGamePhase::Night)) Destroy();
	}
}

void AASBossEncounter::UpdateLocalIntro(float Elapsed)
{
	if (AASPlayerCameraManager* Camera = LocalPlayer.IsValid() ? LocalPlayer->GetASPlayerCameraManager() : nullptr)
	{
		Camera->SetBossIntroBlend(GetCameraBlend(Elapsed));
	}
	if (HealthProgress) HealthProgress->SetPercent(GetHealthFill(Elapsed));
}

void AASBossEncounter::MulticastEndIntro_Implementation()
{
	bIntroFinished = true;
	if (HealthProgress) HealthProgress->SetPercent(1.0f);
	FinishLocalIntro();
	UE_LOG(LogTemp, Log, TEXT("[Boss] Introduction finished; camera and world restored."));
}

void AASBossEncounter::FinishLocalIntro()
{
	if (bLocalIntroActive)
	{
		if (LocalPlayer.IsValid())
		{
			if (AASPlayerCameraManager* Camera = LocalPlayer->GetASPlayerCameraManager()) Camera->EndBossIntro();
			LocalPlayer->SetBossIntroInputBlocked(false);
		}
		if (IsValid(Boss))
		{
			if (UASVisibilityComponent* Visibility = Boss->FindComponentByClass<UASVisibilityComponent>()) Visibility->SetBossIntroVisible(false);
		}
		for (const TWeakObjectPtr<UASFogDisplayComponent>& Fog : FogDisplays)
		{
			if (Fog.IsValid()) Fog->SetBossIntroActive(false);
		}
		FogDisplays.Reset();
		if (HealthBar) HealthBar->SetVisibility(EVisibility::HitTestInvisible);
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetKeyboardFocus(PreviousKeyboardFocus.Pin());
		}
		bLocalIntroActive = false;
	}
	if (bCameraPauseStateSaved && GetWorld())
	{
		GetWorld()->bIsCameraMoveableWhenPaused = bPreviousCameraMoveableWhenPaused;
		bCameraPauseStateSaved = false;
	}
	if (bIntroStarted && GetWorld())
	{
		AWorldSettings* Settings = GetWorld()->GetWorldSettings();
		if (Settings->GetPauserPlayerState() == IntroPauser.Get()) Settings->SetPauserPlayerState(PreviousPauser.Get());
	}
}

void AASBossEncounter::RemoveHealthBar()
{
	if (IsValid(Boss))
	{
		Boss->OnMonsterDisplayNameChanged.RemoveDynamic(this, &AASBossEncounter::HandleMonsterDisplayNameChanged);
	}
	if (HealthBar && GetWorld() && GetWorld()->GetGameViewport())
	{
		GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(HealthBar.ToSharedRef());
	}
	HealthBar.Reset();
	HealthProgress.Reset();
	BossNameText.Reset();
}

void AASBossEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !UE_BUILD_SHIPPING
	const AASGameState* State = GetWorld() ? GetWorld()->GetGameState<AASGameState>() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("[BossDebug][UIRemoved] Boss=%s NetMode=%d BossValid=%d BossDead=%d Phase=%d EndPlayReason=%d"),
		*GetNameSafe(Boss), static_cast<int32>(GetNetMode()), IsValid(Boss), IsValid(Boss) && Boss->IsDead(),
		State ? static_cast<int32>(State->GetCurrentPhase()) : -1, static_cast<int32>(EndPlayReason));
#endif
	FinishLocalIntro();
	RemoveHealthBar();
	if (HasAuthority() && IsValid(Boss)) Boss->bAlwaysRelevant = bBossWasAlwaysRelevant;
	Super::EndPlay(EndPlayReason);
}
