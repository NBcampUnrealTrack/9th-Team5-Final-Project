#include "MapSystem/UI/ASMapViewModelSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MVVMGameSubsystem.h"
#include "MapSystem/Zone/Core/ASMapFunctionLibrary.h"
#include "MapSystem/UI/ASMapViewModel.h"
#include "MapSystem/UI/ASZoneHudViewModel.h"

namespace ASMapViewModelSubsystemPrivate
{
	constexpr double ZoneHudHoldSeconds = 1.0;

	UMVVMViewModelCollectionObject* GetCollection(const UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const UMVVMGameSubsystem* MVVMSubsystem = GameInstance ? GameInstance->GetSubsystem<UMVVMGameSubsystem>() : nullptr;
		return MVVMSubsystem ? MVVMSubsystem->GetViewModelCollection() : nullptr;
	}

	FMVVMViewModelContext MakeContext(const UObject* ViewModel, FName Name)
	{
		FMVVMViewModelContext Context;
		Context.ContextClass = ViewModel->GetClass();
		Context.ContextName = Name;
		return Context;
	}
}

UASMapViewModelSubsystem* UASMapViewModelSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UASMapViewModelSubsystem>() : nullptr;
}

bool UASMapViewModelSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UASMapViewModelSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.GetNetMode() == NM_DedicatedServer || !UASMapFunctionLibrary::LevelHasMapZones(&InWorld))
	{
		return;
	}

	using namespace ASMapViewModelSubsystemPrivate;

	ZoneHudViewModel = NewObject<UASZoneHudViewModel>(this);
	MapViewModel = NewObject<UASMapViewModel>(this);

	// 위젯 바인딩 전에 스냅샷을 읽어 늦게 참가해도 현재 상태로 시작하게 한다.
	RefreshZoneHud();

	if (UMVVMViewModelCollectionObject* Collection = GetCollection(&InWorld))
	{
		Collection->AddViewModelInstance(MakeContext(ZoneHudViewModel, UASZoneHudViewModel::GlobalName), ZoneHudViewModel);
		Collection->AddViewModelInstance(MakeContext(MapViewModel, UASMapViewModel::GlobalName), MapViewModel);
	}
}

void UASMapViewModelSubsystem::Deinitialize()
{
	UnregisterViewModels();
	Super::Deinitialize();
}

void UASMapViewModelSubsystem::UnregisterViewModels()
{
	UMVVMViewModelCollectionObject* Collection = ASMapViewModelSubsystemPrivate::GetCollection(GetWorld());
	if (!Collection)
	{
		return;
	}

	if (ZoneHudViewModel)
	{
		Collection->RemoveAllViewModelInstance(ZoneHudViewModel);
	}
	if (MapViewModel)
	{
		Collection->RemoveAllViewModelInstance(MapViewModel);
	}
}

bool UASMapViewModelSubsystem::IsTickable() const
{
	return ZoneHudViewModel != nullptr;
}

void UASMapViewModelSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RefreshZoneHud();
}

void UASMapViewModelSubsystem::RefreshZoneHud()
{
	if (!ZoneHudViewModel)
	{
		return;
	}

	FASZoneHudInfo Info;
	if (const APawn* Pawn = UASMapFunctionLibrary::GetLocalPlayerPawn(this))
	{
		Info = UASMapFunctionLibrary::GetZoneHudInfoAtLocation(this, Pawn->GetActorLocation());
	}
	const double Now = GetWorld()->GetTimeSeconds();

	// 구역 경계 틈에서 HUD가 깜빡이지 않게 한다.
	if (Info.bInZone)
	{
		HeldZoneId = Info.ZoneId;
		HeldZoneEndTime = Now + ASMapViewModelSubsystemPrivate::ZoneHudHoldSeconds;
	}
	else if (!HeldZoneId.IsNone() && Now < HeldZoneEndTime)
	{
		Info = UASMapFunctionLibrary::GetZoneHudInfo(this, HeldZoneId);
	}
	else
	{
		HeldZoneId = NAME_None;
	}
	ZoneHudViewModel->SetInfo(Info);
}

TStatId UASMapViewModelSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UASMapViewModelSubsystem, STATGROUP_Tickables);
}
