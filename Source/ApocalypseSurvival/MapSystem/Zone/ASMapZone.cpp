#include "MapSystem/Zone/ASMapZone.h"

#include "Components/SceneComponent.h"
#include "MapSystem/Shared/ASMapPolygonAreaComponent.h"
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

	MainPolygon = CreateDefaultSubobject<UASMapPolygonAreaComponent>(TEXT("MainPolygon"));
	MainPolygon->SetupAttachment(SceneRoot);

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

void AASMapZone::GetAreaOutlines(TArray<TArray<FVector2D>>& OutOutlines) const
{
	RefreshAreaShapes();
	ASMapShapeUtils::GetAreaOutlines(CachedAreaShapes, OutOutlines);
}

FVector AASMapZone::GetZoneCenter() const
{
	TArray<TArray<FVector2D>> Outlines;
	GetAreaOutlines(Outlines);
	FBox2D Combined(ForceInit);
	for (const TArray<FVector2D>& Outline : Outlines)
	{
		for (const FVector2D& Point : Outline)
		{
			Combined += Point;
		}
	}
	if (!Combined.bIsValid)
	{
		return GetActorLocation();
	}

	const FVector2D Center = Combined.GetCenter();
	return FVector(Center.X, Center.Y, GetActorLocation().Z);
}

bool AASMapZone::ValidateArea(TArray<FString>& OutErrors) const
{
	RefreshAreaShapes();
	const int32 InitialErrorCount = OutErrors.Num();
	for (const TWeakObjectPtr<UPrimitiveComponent>& Component : CachedAreaShapes)
	{
		if (const UASMapPolygonAreaComponent* Polygon = Cast<UASMapPolygonAreaComponent>(Component.Get()))
		{
			if (Polygon->ComponentHasTag(TEXT("NotArea")))
			{
				continue;
			}
			FString Error;
			if (!Polygon->ValidateArea(Error))
			{
				OutErrors.Add(FString::Printf(TEXT("Zone '%s', component '%s': %s"),
					*ZoneId.ToString(), *Polygon->GetName(), *Error));
			}
		}
	}
	return OutErrors.Num() == InitialErrorCount;
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
	if (bAreaShapesCached && !CachedAreaShapes.ContainsByPredicate([](const TWeakObjectPtr<UPrimitiveComponent>& Shape) { return !Shape.IsValid(); }))
	{
		return;
	}

	ASMapShapeUtils::GatherAreaComponents(this, CachedAreaShapes);

	// 플레이 시작 후에만 캐시한다. 에디터 인스턴스는 아직 컴포넌트를 추가 중일 수 있다.
	bAreaShapesCached = HasActorBegunPlay();
}
