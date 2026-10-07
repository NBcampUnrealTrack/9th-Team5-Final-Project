#include "MapSystem/Zone/ASMapZone.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "MapSystem/Shared/ASMapShapeUtils.h"
#include "MapSystem/Zone/Core/ASMapWorldSubsystem.h"

#if WITH_EDITORONLY_DATA
#include "MapSystem/Zone/ASZoneLinkDebugComponent.h"
#endif

#if WITH_EDITOR
#include "Engine/World.h"
#include "EngineUtils.h"
#endif

AASMapZone::AASMapZone()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MainArea = CreateDefaultSubobject<UBoxComponent>(TEXT("MainArea"));
	MainArea->SetupAttachment(SceneRoot);
	MainArea->SetBoxExtent(FVector(1000.0, 1000.0, 300.0));
	ASMapShapeUtils::ConfigureAreaShape(MainArea, FColor(80, 200, 255));

#if WITH_EDITORONLY_DATA
	// 상호 참조하는 구역과 레지스트리를 위해 모든 구역을 항상 로드한다.
	bIsSpatiallyLoaded = false;
	LinkDebugComponent = CreateEditorOnlyDefaultSubobject<UASZoneLinkDebugComponent>(TEXT("LinkDebugComponent"));
	if (LinkDebugComponent)
	{
		LinkDebugComponent->SetupAttachment(SceneRoot);
	}
#endif
}

bool AASMapZone::ContainsLocation(const FVector& Location) const
{
	RefreshAreaShapes();
	return ASMapShapeUtils::AnyShapeContainsLocation(CachedAreaShapes, Location, bIgnoreHeight);
}

void AASMapZone::GetFootprints(TArray<FBox2D>& OutFootprints) const
{
	RefreshAreaShapes();
	ASMapShapeUtils::GetFootprints(CachedAreaShapes, OutFootprints);
}

FVector AASMapZone::GetZoneCenter() const
{
	TArray<FBox2D> Footprints;
	GetFootprints(Footprints);
	if (Footprints.Num() == 0)
	{
		return GetActorLocation();
	}

	FBox2D Combined(ForceInit);
	for (const FBox2D& Footprint : Footprints)
	{
		Combined += Footprint;
	}

	const FVector2D Center = Combined.GetCenter();
	return FVector(Center.X, Center.Y, GetActorLocation().Z);
}

TArray<FTransform> AASMapZone::GetSpawnTransforms() const
{
	TArray<FTransform> Result;
	for (const FTransform& Point : SpawnPoints)
	{
		Result.Add(Point * GetActorTransform());
	}
	return Result;
}

TArray<FTransform> AASMapZone::GetRespawnTransforms() const
{
	TArray<FTransform> Result;
	for (const FTransform& Point : RespawnPoints)
	{
		Result.Add(Point * GetActorTransform());
	}
	return Result;
}

bool AASMapZone::HasAreaShapes() const
{
	RefreshAreaShapes();
	return CachedAreaShapes.Num() > 0;
}

void AASMapZone::BeginPlay()
{
	Super::BeginPlay();

	bAreaShapesCached = false;
	RefreshAreaShapes();

	if (UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(this))
	{
		Subsystem->RegisterZone(this);
	}
}

void AASMapZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(this))
	{
		Subsystem->UnregisterZone(this);
	}

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void AASMapZone::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	bAreaShapesCached = false;
	RefreshLinkDebugComponents();
}

void AASMapZone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	bAreaShapesCached = false;
	RefreshLinkDebugComponents();
}

void AASMapZone::RefreshLinkDebugComponents()
{
#if WITH_EDITORONLY_DATA
	auto RefreshZone = [](AASMapZone* Zone)
	{
		if (IsValid(Zone) && Zone->LinkDebugComponent)
		{
			Zone->LinkDebugComponent->UpdateBounds();
			Zone->LinkDebugComponent->MarkRenderStateDirty();
		}
	};

	RefreshZone(this);
	// 연결 삭제 시 이전 이웃의 역방향 선도 사라지도록 모든 구역의 프록시를 갱신한다.
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AASMapZone> It(World); It; ++It)
		{
			RefreshZone(*It);
		}
	}
#endif
}
#endif

void AASMapZone::RefreshAreaShapes() const
{
	if (bAreaShapesCached && !CachedAreaShapes.ContainsByPredicate([](const TWeakObjectPtr<UShapeComponent>& Shape) { return !Shape.IsValid(); }))
	{
		return;
	}

	ASMapShapeUtils::GatherAreaShapes(this, CachedAreaShapes);

	// 플레이 시작 후에만 캐시한다. 에디터 인스턴스는 아직 컴포넌트를 추가 중일 수 있다.
	bAreaShapesCached = HasActorBegunPlay();
}
