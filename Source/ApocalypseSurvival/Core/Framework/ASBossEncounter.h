#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ASBossEncounter.generated.h"

class AASMonsterCharacter;
class AASPlayerController;
class APlayerState;
class SWidget;
class SProgressBar;
class STextBlock;
class UASFogDisplayComponent;

/** Replicates the boss introduction before pausing, then advances it using real time. */
UCLASS()
class APOCALYPSESURVIVAL_API AASBossEncounter : public AActor
{
	GENERATED_BODY()

public:
	AASBossEncounter();
	void InitializeBoss(AASMonsterCharacter* InBoss);
	void AcknowledgePlayer(AASPlayerController* Player);
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static float GetHealthFill(float Elapsed);
	static float GetCameraBlend(float Elapsed);
	static constexpr float IntroDuration = 4.5f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class FASBossIntroPauseTest;
	friend class FASBossIntroRenderingTest;
	UFUNCTION()
	void OnRep_Boss();
	UFUNCTION()
	void HandleMonsterDisplayNameChanged(FText NewDisplayName);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBeginIntro();
	UFUNCTION(NetMulticast, Reliable)
	void MulticastEndIntro();

	void FinishLocalIntro();
	void RemoveHealthBar();
	void BuildHealthBar();
	void UpdateLocalIntro(float Elapsed);
	bool ArePlayersReady() const;

	UPROPERTY(ReplicatedUsing = OnRep_Boss)
	TObjectPtr<AASMonsterCharacter> Boss;
	TSet<TWeakObjectPtr<AASPlayerController>> ReadyPlayers;
	TWeakObjectPtr<AASPlayerController> LocalPlayer;
	TWeakObjectPtr<APlayerState> PreviousPauser;
	TWeakObjectPtr<APlayerState> IntroPauser;
	TArray<TWeakObjectPtr<UASFogDisplayComponent>> FogDisplays;
	TSharedPtr<SWidget> HealthBar;
	TSharedPtr<SProgressBar> HealthProgress;
	TSharedPtr<STextBlock> BossNameText;
	TWeakPtr<SWidget> PreviousKeyboardFocus;
	double PreparationStartTime = 0.0;
	double IntroStartTime = 0.0;
	bool bIntroStarted = false;
	bool bIntroFinished = false;
	bool bLocalIntroActive = false;
	bool bAcknowledged = false;
	bool bDeathReported = false;
	bool bBossWasAlwaysRelevant = false;
	bool bPreviousCameraMoveableWhenPaused = false;
	bool bCameraPauseStateSaved = false;
#if !UE_BUILD_SHIPPING
	float LastLoggedHealth = -1.0f;
	float LastLoggedMaxHealth = -1.0f;
#endif
};
