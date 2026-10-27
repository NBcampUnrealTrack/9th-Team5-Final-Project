#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/Framework/ASBossEncounter.h"
#include "Core/Framework/ASPlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Slate/WidgetRenderer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "TimerManager.h"
#include "MapSystem/Vision/ASFogDisplayComponent.h"
#include "Engine/PostProcessVolume.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASBossIntroPauseTest,
	"ApocalypseSurvival.BossIntro.PauseAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASBossIntroPauseTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(FName(TEXT("BossIntroPauseTest")));
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	World->InitializeActorsForPlay(FURL());
	APlayerController* Player = World->SpawnActor<APlayerController>();
	Player->PlayerState = World->SpawnActor<APlayerState>();
	AASBossEncounter* Encounter = World->SpawnActor<AASBossEncounter>();
	AWorldSettings* Settings = World->GetWorldSettings();
	Settings->NotifyBeginPlay();
	bool bGameplayTimerFired = false;
	FTimerHandle GameplayTimer;
	World->GetTimerManager().SetTimer(GameplayTimer, [&bGameplayTimerFired]() { bGameplayTimerFired = true; }, 0.01f, false);
	TestTrue(TEXT("The introduction ticks while gameplay is paused"), Encounter->PrimaryActorTick.bTickEvenWhenPaused);
	Encounter->MulticastBeginIntro_Implementation();
	TestTrue(TEXT("The introduction pauses the gameplay world"), World->IsPaused());
	TestTrue(TEXT("The renderer allows camera movement during the pause"), World->bIsCameraMoveableWhenPaused);
	Encounter->IntroStartTime = FPlatformTime::Seconds() - AASBossEncounter::IntroDuration - 0.1;
	const double PreviousGameTime = World->GetTimeSeconds();
	World->Tick(LEVELTICK_All, 0.1f);
	TestTrue(TEXT("Real time finishes the introduction even with zero gameplay delta"), Encounter->bIntroFinished);
	TestEqual(TEXT("Gameplay time does not advance during the introduction"), World->GetTimeSeconds(), PreviousGameTime);
	TestFalse(TEXT("Gameplay timers remain stopped"), bGameplayTimerFired);
	World->GetTimerManager().ClearTimer(GameplayTimer);
	TestFalse(TEXT("Finishing the introduction resumes gameplay"), World->IsPaused());
	TestFalse(TEXT("The paused-camera rendering flag is restored"), World->bIsCameraMoveableWhenPaused);
	Encounter = World->SpawnActor<AASBossEncounter>();

	APlayerState* PreviousPauser = World->SpawnActor<APlayerState>();
	Settings->SetPauserPlayerState(PreviousPauser);
	World->bIsCameraMoveableWhenPaused = true;
	Encounter->MulticastBeginIntro_Implementation();
	Encounter->FinishLocalIntro();
	TestTrue(TEXT("An earlier pause is preserved"), Settings->GetPauserPlayerState() == PreviousPauser);
	TestTrue(TEXT("An existing paused-camera setting is preserved"), World->bIsCameraMoveableWhenPaused);
	Encounter->FinishLocalIntro();
	TestTrue(TEXT("Repeated cleanup preserves the previous pause"), Settings->GetPauserPlayerState() == PreviousPauser);
	Settings->SetPauserPlayerState(nullptr);

	TestEqual(TEXT("Health starts empty"), AASBossEncounter::GetHealthFill(0.0f), 0.0f);
	TestEqual(TEXT("Health waits for the camera"), AASBossEncounter::GetHealthFill(0.75f), 0.0f);
	TestEqual(TEXT("Health reaches halfway after one fill second"), AASBossEncounter::GetHealthFill(1.75f), 0.5f);
	TestEqual(TEXT("Health is full before the hold"), AASBossEncounter::GetHealthFill(2.75f), 1.0f);
	TestEqual(TEXT("Camera remains on the boss during the hold"), AASBossEncounter::GetCameraBlend(3.5f), 1.0f);
	TestEqual(TEXT("Camera returns exactly to its previous view"), AASBossEncounter::GetCameraBlend(AASBossEncounter::IntroDuration), 0.0f);
	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASBossCameraRestoreTest,
	"ApocalypseSurvival.BossIntro.RestoresFreeCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FASBossCameraRestoreTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(FName(TEXT("BossIntroCameraTest")));
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	World->InitializeActorsForPlay(FURL());
	APlayerController* Player = World->SpawnActor<APlayerController>();
	AActor* Target = World->SpawnActor<AActor>();
	Target->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
	AASPlayerCameraManager* Camera = World->SpawnActor<AASPlayerCameraManager>();
	Camera->InitializeFor(Player);
	Camera->SetViewTarget(Target);
	Camera->FocusLocation = FVector(1500.0f, -800.0f, 100.0f);
	Camera->ViewFocusLocation = FVector(1400.0f, -750.0f, 100.0f);
	Camera->TargetCameraDistance = 3000.0f;
	Camera->ViewCameraDistance = 2500.0f;
	Camera->LastViewTarget = Target;
	FTViewTarget View;
	View.Target = Target;
	Camera->UpdateViewTargetInternal(View, 0.0f);
	const FVector PreviousFocus = Camera->FocusLocation;
	const FMinimalViewInfo PreviousView = View.POV;
	Camera->FillCameraCache(PreviousView);
	Camera->BeginBossIntro(FVector(-4000.0f, 4000.0f, 100.0f));
	Camera->SetBossIntroBlend(1.0f);
	Camera->UpdateViewTargetInternal(View, 0.0f);
	TestFalse(TEXT("Introduction moves the camera away from the previous view"), View.POV.Location.Equals(PreviousView.Location));
	Camera->EndBossIntro();
	Camera->UpdateViewTargetInternal(View, 0.0f);
	TestTrue(TEXT("Camera returns to the exact previous position, not the pawn"), View.POV.Location.Equals(PreviousView.Location));
	TestTrue(TEXT("Camera rotation is preserved"), View.POV.Rotation.Equals(PreviousView.Rotation));
	TestEqual(TEXT("Camera field of view is preserved"), View.POV.FOV, PreviousView.FOV);
	TestTrue(TEXT("Free camera destination is preserved"), Camera->FocusLocation.Equals(PreviousFocus));
	TestEqual(TEXT("Requested zoom is preserved"), Camera->TargetCameraDistance, 3000.0f);
	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASBossIntroRenderingTest,
	"ApocalypseSurvival.BossIntro.HealthBarPixelsWhilePaused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FASBossIntroRenderingTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(FName(TEXT("BossIntroRenderingTest")));
	UWorld* World = Instance->GetWorld();
	World->InitializeActorsForPlay(FURL());
	AASBossEncounter* Encounter = World->SpawnActor<AASBossEncounter>();
	World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
	Encounter->BuildHealthBar();
	FWidgetRenderer Renderer(false, true);
	TArray<int32> FilledPixelCounts;
	const float ElapsedTimes[] = {0.75f, 1.75f, 2.75f};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Encounter->UpdateLocalIntro(ElapsedTimes[Index]);
		UTextureRenderTarget2D* Target = Renderer.DrawWidget(Encounter->HealthBar.ToSharedRef(), FVector2D(800.0f, 150.0f));
		TArray<FColor> Pixels;
		if (!Target || !Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels))
		{
			AddError(TEXT("Could not read the rendered boss health bar"));
			break;
		}
		int32 FilledPixels = 0;
		for (const FColor& Pixel : Pixels)
		{
			if (Pixel.R > 40 && Pixel.R > Pixel.G * 2 && Pixel.R > Pixel.B * 2) ++FilledPixels;
		}
		FilledPixelCounts.Add(FilledPixels);
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(800, 150, Pixels, Png);
		const FString Path = FPaths::ProjectSavedDir() / FString::Printf(TEXT("Automation/BossHealth-%d.png"), Index * 50);
		FFileHelper::SaveArrayToFile(Png, *Path);
		AddInfo(FString::Printf(TEXT("Health %d%%: %d filled pixels"), Index * 50, FilledPixels));
	}
	if (FilledPixelCounts.Num() == 3)
	{
		TestEqual(TEXT("Empty health bar contains no red fill"), FilledPixelCounts[0], 0);
		TestTrue(TEXT("Half health visibly fills the bar"), FilledPixelCounts[1] > 1000);
		TestTrue(TEXT("Full health visibly fills twice the area of half health"),
			FMath::Abs(FilledPixelCounts[2] - FilledPixelCounts[1] * 2) < 200);
	}
	TestTrue(TEXT("Rendering the UI does not unpause gameplay"), World->IsPaused());
	World->GetWorldSettings()->SetPauserPlayerState(nullptr);
	Encounter->RemoveHealthBar();
	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FASBossIntroFogTest,
	"ApocalypseSurvival.BossIntro.LocalFogRevealAndRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FASBossIntroFogTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	AActor* Owner = World->SpawnActor<AActor>();
	UASFogDisplayComponent* Fog = NewObject<UASFogDisplayComponent>(Owner);
	Fog->WorldFogVolume = World->SpawnActor<APostProcessVolume>();
	Fog->WorldFogVolume->BlendWeight = 0.7f;
	Fog->ExploredCells.Init(0, 64);
	Fog->VisibleCells.Init(0, 64);
	Fog->ShownAlpha.Init(235, 64);
	const TArray<uint8> PreviousExplored = Fog->ExploredCells;
	const TArray<uint8> PreviousVisible = Fog->VisibleCells;
	const TArray<uint8> PreviousAlpha = Fog->ShownAlpha;
	const FVector Center(2500.0f, -1000.0f, 100.0f);
	Fog->SetBossIntroActive(true, Center);
	const FVector2D Center2D(Center.X, Center.Y);
	TestEqual(TEXT("The whole-map fog remains enabled"), Fog->WorldFogVolume->BlendWeight, 0.7f);
	TestEqual(TEXT("Boss center is clear"), Fog->GetBossIntroFogOpacityScale(Center2D), 0.0f);
	TestEqual(TEXT("The inner 500cm is clear"), Fog->GetBossIntroFogOpacityScale(Center2D + FVector2D(500.0f, 0.0f)), 0.0f);
	const float SoftEdge = Fog->GetBossIntroFogOpacityScale(Center2D + FVector2D(550.0f, 0.0f));
	TestTrue(TEXT("The outer edge fades smoothly"), SoftEdge > 0.0f && SoftEdge < 1.0f);
	TestEqual(TEXT("Fog beyond 600cm stays unchanged"), Fog->GetBossIntroFogOpacityScale(Center2D + FVector2D(601.0f, 0.0f)), 1.0f);
	TestTrue(TEXT("Introduction does not explore the map"), Fog->ExploredCells == PreviousExplored);
	TestTrue(TEXT("Introduction does not change logical enemy visibility"), Fog->VisibleCells == PreviousVisible);
	TestTrue(TEXT("Introduction preserves the original fog mask"), Fog->ShownAlpha == PreviousAlpha);
	Fog->SetBossIntroActive(false);
	TestEqual(TEXT("Original fog returns at the boss center"), Fog->GetBossIntroFogOpacityScale(Center2D), 1.0f);
	TestEqual(TEXT("Restoring does not change the world fog weight"), Fog->WorldFogVolume->BlendWeight, 0.7f);

	// Render the uploaded texture too, so swapped world/grid axes cannot pass the radius checks alone.
	Fog->ResolutionX = 64;
	Fog->ResolutionY = 64;
	Fog->FogWorldMin = FVector2D(-3200.0f, -3200.0f);
	Fog->FogWorldMax = FVector2D(3200.0f, 3200.0f);
	Fog->ShownAlpha.Init(235, 64 * 64);
	Fog->VisibleCells.Init(0, 64 * 64);
	Fog->FogTexture = UTexture2D::CreateTransient(64, 64, PF_B8G8R8A8);
	Fog->FogTexture->SRGB = false;
	Fog->FogTexture->UpdateResource();
	Fog->SetBossIntroActive(true, FVector(1000.0f, 1500.0f, 0.0f));
	FSlateBrush Brush;
	Brush.SetResourceObject(Fog->FogTexture);
	Brush.ImageSize = FVector2D(64.0f, 64.0f);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	const TSharedRef<SImage> Image = SNew(SImage).Image(&Brush);
	FWidgetRenderer Renderer(false, true);
	auto ReadPixels = [&]()
	{
		TArray<FColor> Pixels;
		UTextureRenderTarget2D* Target = Renderer.DrawWidget(Image, FVector2D(64.0f, 64.0f));
		if (Target) Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		return Pixels;
	};
	TArray<FColor> Pixels = ReadPixels();
	if (TestEqual(TEXT("Fog texture rendered"), Pixels.Num(), 64 * 64))
	{
		TestTrue(TEXT("Uploaded pixels clear the boss location"), Pixels[22 * 64 + 47].A < 20);
		TestTrue(TEXT("Uploaded pixels preserve distant fog"), Pixels[0].A > 200);
		TestTrue(TEXT("World axes are mapped to the correct texture coordinates"), Pixels[47 * 64 + 22].A > 200);
	}
	Fog->SetBossIntroActive(false);
	Pixels = ReadPixels();
	if (Pixels.Num() == 64 * 64)
	{
		TestTrue(TEXT("Uploaded pixels restore fog after the introduction"), Pixels[22 * 64 + 47].A > 200);
	}
	World->DestroyWorld(false);
	return true;
}

#endif
