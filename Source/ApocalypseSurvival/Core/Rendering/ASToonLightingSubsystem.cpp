#include "Core/Rendering/ASToonLightingSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

namespace
{
	const TCHAR* CollectionPath = TEXT("/Game/ApocalypseSurvival/Art/ToonStyle/MPC_ToonLighting.MPC_ToonLighting");
}

bool UASToonLightingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// 게임 월드와 에디터 월드에서만. 프리뷰·인액티브 월드에서는 돌 필요가 없다.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->IsGameWorld() || World->WorldType == EWorldType::Editor);
}

void UASToonLightingSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!Collection.IsValid())
	{
		Collection = LoadObject<UMaterialParameterCollection>(nullptr, CollectionPath);
		if (!Collection.IsValid())
		{
			return;
		}
	}

	if (!CachedSun.IsValid())
	{
		// 매 프레임 월드를 뒤지면 낭비라 반 초에 한 번만 다시 찾는다.
		RetryTimer -= DeltaTime;
		if (RetryTimer > 0.0f)
		{
			return;
		}
		RetryTimer = 0.5f;
		CachedSun = FindSunLight();
		if (!CachedSun.IsValid())
		{
			return;
		}
	}

	const UDirectionalLightComponent* Sun = CachedSun.Get();

	// 라이트의 전방이 빛이 "가는" 방향이다. 머티리얼은 이 값을 그대로 받아 -방향으로 뒤집어 쓴다.
	const FVector Direction = Sun->GetForwardVector();
	UKismetMaterialLibrary::SetVectorParameterValue(World, Collection.Get(), TEXT("SunDirection"),
		FLinearColor(Direction.X, Direction.Y, Direction.Z, 0.0f));

	// 세기는 룩스 단위라 그대로 곱하면 너무 밝다. 낮 기준(대략 4~10룩스)을 1 로 보고 눌러서 넘긴다.
	const float Intensity = FMath::Clamp(Sun->Intensity / 6.0f, 0.05f, 1.5f);
	const FLinearColor SunColor = Sun->GetLightColor() * Intensity;
	UKismetMaterialLibrary::SetVectorParameterValue(World, Collection.Get(), TEXT("SunColor"), SunColor);

	// 밤에는 그늘도 같이 어두워져야 한다. 태양 세기에 따라 주변광을 낮춘다.
	const FLinearColor Ambient = FLinearColor(0.75f, 0.78f, 0.9f) * FMath::Lerp(0.35f, 1.0f, Intensity);
	UKismetMaterialLibrary::SetVectorParameterValue(World, Collection.Get(), TEXT("AmbientColor"), Ambient);
}

UDirectionalLightComponent* UASToonLightingSubsystem::FindSunLight()
{
	UWorld* World = GetWorld();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UDirectionalLightComponent* Light = It->FindComponentByClass<UDirectionalLightComponent>())
		{
			if (Light->IsVisible() && Light->Intensity > 0.0f)
			{
				return Light;
			}
		}
	}
	return nullptr;
}
