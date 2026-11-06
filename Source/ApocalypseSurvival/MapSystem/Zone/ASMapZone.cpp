#include "MapSystem/Zone/ASMapZone.h"

#include "ApocalypseSurvival.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MapSystem/Shared/ASMapPolygonAreaComponent.h"
#include "MapSystem/Shared/ASMapShapeUtils.h"
#include "MapSystem/Zone/Core/ASMapWorldSubsystem.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITORONLY_DATA
#include "MapSystem/Zone/ASZoneLinkDebugComponent.h"
#endif

#if WITH_EDITOR
#include "Engine/World.h"
#include "EngineUtils.h"
#endif

namespace
{
	// 경계선 폭 (cm). 높이와 불투명도는 구역 액터에서 설정한다.
	constexpr double BoundaryWidth = 30.0;
}

AASMapZone::AASMapZone()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MainPolygon = CreateDefaultSubobject<UASMapPolygonAreaComponent>(TEXT("MainPolygon"));
	MainPolygon->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	BoundaryPlaneMesh = PlaneMeshFinder.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	BoundaryCylinderMesh = CylinderMeshFinder.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BoundaryMaterialFinder(
		TEXT("/Game/ApocalypseSurvival/MapSystem/Zone/M_ZoneBoundary.M_ZoneBoundary"));
	BoundaryMaterial = BoundaryMaterialFinder.Object;

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

const TArray<TArray<FVector2D>>& AASMapZone::GetAreaOutlines() const
{
	RefreshAreaShapes();
	return CachedOutlines;
}

FVector AASMapZone::GetZoneCenter() const
{
	RefreshAreaShapes();
	return CachedCenter;
}

bool AASMapZone::ValidateArea(TArray<FString>& OutErrors) const
{
	RefreshAreaShapes();
	const int32 InitialErrorCount = OutErrors.Num();
	for (const TWeakObjectPtr<UPrimitiveComponent>& Component : CachedAreaShapes)
	{
		const UPrimitiveComponent* AreaComponent = Component.Get();
		if (!AreaComponent || AreaComponent->ComponentHasTag(TEXT("NotArea")))
		{
			continue;
		}
		const FVector Scale = AreaComponent->GetComponentTransform().GetScale3D();
		if (FMath::IsNearlyZero(FMath::Abs(Scale.X)) || FMath::IsNearlyZero(FMath::Abs(Scale.Y)))
		{
			OutErrors.Add(FString::Printf(TEXT("Zone '%s', component '%s': World X and Y scale must not be nearly zero."),
				*ZoneId.ToString(), *AreaComponent->GetName()));
		}
		bool bInvalidSize = false;
		if (const UBoxComponent* Box = Cast<UBoxComponent>(AreaComponent))
		{
			const FVector Extent = Box->GetUnscaledBoxExtent();
			bInvalidSize = Extent.X <= 0.0 || Extent.Y <= 0.0;
		}
		else if (const USphereComponent* Sphere = Cast<USphereComponent>(AreaComponent))
		{
			bInvalidSize = Sphere->GetUnscaledSphereRadius() <= 0.0f;
		}
		else if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(AreaComponent))
		{
			bInvalidSize = Capsule->GetUnscaledCapsuleRadius() <= 0.0f;
		}
		if (bInvalidSize)
		{
			OutErrors.Add(FString::Printf(TEXT("Zone '%s', component '%s': Unscaled box X/Y extent or sphere/capsule radius must be greater than zero."),
				*ZoneId.ToString(), *AreaComponent->GetName()));
		}
		const FRotator Rotation = AreaComponent->GetComponentRotation();
		if (FMath::Abs(Rotation.Pitch) > 0.1 || FMath::Abs(Rotation.Roll) > 0.1)
		{
			OutErrors.Add(FString::Printf(TEXT("Zone '%s', component '%s': World Pitch and Roll must be within 0.1 degrees of zero."),
				*ZoneId.ToString(), *AreaComponent->GetName()));
		}
		if (const UASMapPolygonAreaComponent* Polygon = Cast<UASMapPolygonAreaComponent>(AreaComponent))
		{
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
	ASMapShapeUtils::DisableAreaShapeCollision(CachedAreaShapes);
	BuildBoundary();

	if (UASMapWorldSubsystem* Subsystem = UASMapWorldSubsystem::Get(this))
	{
		Subsystem->RegisterZone(this);
	}
}

void AASMapZone::BuildBoundary()
{
	if (!bShowBoundary || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!IsValid(BoundaryMaterial))
	{
		UE_LOG(LogApocalypseSurvival, Warning, TEXT("Zone '%s': M_ZoneBoundary is missing; boundary is disabled."), *ZoneId.ToString());
		return;
	}
	if (!IsValid(BoundaryPlaneMesh) || !IsValid(BoundaryCylinderMesh))
	{
		return;
	}

	const auto CreateBoundaryComponent = [this](const FName Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, Name, RF_Transient);
		Component->SetupAttachment(RootComponent);
		// 액터의 위치, 회전, 스케일과 무관하게 인스턴스 변환을 월드 좌표로 사용한다.
		Component->SetAbsolute(true, true, true);
		Component->SetWorldTransform(FTransform::Identity);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCastShadow(false);
		Component->bReceivesDecals = false;
		Component->SetCanEverAffectNavigation(false);
		Component->SetStaticMesh(Mesh);
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BoundaryMaterial, Component);
		Material->SetVectorParameterValue(TEXT("Color"), BoundaryColor);
		Material->SetScalarParameterValue(TEXT("Opacity"), BoundaryOpacity);
		Component->SetMaterial(0, Material);
		Component->RegisterComponent();
		return Component;
	};
	BoundaryComponent = CreateBoundaryComponent(TEXT("BoundaryComponent"), BoundaryPlaneMesh);
	BoundaryJointComponent = CreateBoundaryComponent(TEXT("BoundaryJointComponent"), BoundaryCylinderMesh);

	const TArray<TArray<FVector2D>>& Outlines = GetAreaOutlines();
	const double BoundaryZ = GetActorLocation().Z + BoundaryHeight;
	const double HalfWidth = BoundaryWidth * 0.5;
	const FVector JointScale(BoundaryWidth / 100.0, BoundaryWidth / 100.0, 0.002);
	// 피벗 위치와 무관하게 원판 윗면을 리본과 같은 액터 기준 높이에 맞춘다.
	const FBoxSphereBounds CylinderBounds = BoundaryCylinderMesh->GetBounds();
	const double JointTopOffset = (CylinderBounds.Origin.Z + CylinderBounds.BoxExtent.Z) * JointScale.Z;
	for (const TArray<FVector2D>& Outline : Outlines)
	{
		// 길이가 없는 변과 중복된 마지막 점을 제거한 뒤 안쪽 중심선을 계산한다.
		TArray<FVector2D> Vertices;
		Vertices.Reserve(Outline.Num());
		for (const FVector2D& Vertex : Outline)
		{
			if (Vertices.IsEmpty() || (Vertex - Vertices.Last()).SizeSquared() > FMath::Square(UE_KINDA_SMALL_NUMBER))
			{
				Vertices.Add(Vertex);
			}
		}
		if (Vertices.Num() > 1 && (Vertices.Last() - Vertices[0]).SizeSquared() <= FMath::Square(UE_KINDA_SMALL_NUMBER))
		{
			Vertices.Pop();
		}
		if (Vertices.Num() < 3)
		{
			continue;
		}
		// 부호 있는 면적으로 안쪽 법선 방향을 판단한다.
		double TwiceArea = 0.0;
		for (int32 Index = 1; Index + 1 < Vertices.Num(); ++Index)
		{
			TwiceArea += (Vertices[Index] - Vertices[0]) ^ (Vertices[Index + 1] - Vertices[0]);
		}
		if (FMath::IsNearlyZero(TwiceArea))
		{
			continue;
		}
		TArray<FVector2D> InwardNormals;
		InwardNormals.Reserve(Vertices.Num());
		for (int32 Index = 0; Index < Vertices.Num(); ++Index)
		{
			const FVector2D Edge = Vertices[(Index + 1) % Vertices.Num()] - Vertices[Index];
			const FVector2D Direction = Edge / Edge.Size();
			InwardNormals.Add(FVector2D(-Direction.Y, Direction.X) * (TwiceArea > 0.0 ? 1.0 : -1.0));
		}
		TArray<FVector2D> InsetVertices;
		InsetVertices.Reserve(Vertices.Num());
		for (int32 Index = 0; Index < Vertices.Num(); ++Index)
		{
			const FVector2D& IncomingNormal = InwardNormals[(Index + Vertices.Num() - 1) % Vertices.Num()];
			const FVector2D& OutgoingNormal = InwardNormals[Index];
			const FVector2D NormalSum = IncomingNormal + OutgoingNormal;
			const double Denominator = 1.0 + FVector2D::DotProduct(IncomingNormal, OutgoingNormal);
			FVector2D Offset;
			// 예각의 긴 돌출과 반대 방향 법선의 영벡터를 피한다.
			if (NormalSum.IsNearlyZero())
			{
				Offset = IncomingNormal * HalfWidth;
			}
			else if (Denominator < 0.25)
			{
				Offset = NormalSum.GetSafeNormal() * HalfWidth;
			}
			else
			{
				Offset = NormalSum * (HalfWidth / Denominator);
			}
			const FVector2D InsetVertex = Vertices[Index] + Offset;
			InsetVertices.Add(InsetVertex);
			const FVector JointLocation(InsetVertex.X, InsetVertex.Y, BoundaryZ - JointTopOffset);
			BoundaryJointComponent->AddInstance(FTransform(FRotator::ZeroRotator, JointLocation, JointScale));
		}
		// 안쪽 다각형의 변마다 분할이나 길이 겹침 없이 리본 하나를 배치한다.
		for (int32 Index = 0; Index < InsetVertices.Num(); ++Index)
		{
			const FVector2D& Start = InsetVertices[Index];
			const FVector2D Edge = InsetVertices[(Index + 1) % InsetVertices.Num()] - Start;
			const double EdgeLength = Edge.Size();
			if (EdgeLength <= UE_KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const FVector2D Direction = Edge / EdgeLength;
			const FRotator Rotation(0.0, FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)), 0.0);
			const FVector2D Center = Start + Edge * 0.5;
			const FVector Location(Center.X, Center.Y, BoundaryZ);
			const FVector Scale(EdgeLength / 100.0, BoundaryWidth / 100.0, 1.0);
			BoundaryComponent->AddInstance(FTransform(Rotation, Location, Scale));
		}
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
	ASMapShapeUtils::GetAreaOutlines(CachedAreaShapes, CachedOutlines);
	FBox2D OutlineBounds(ForceInit);
	for (const TArray<FVector2D>& Outline : CachedOutlines)
	{
		for (const FVector2D& Point : Outline)
		{
			OutlineBounds += Point;
		}
	}
	CachedCenter = GetActorLocation();
	if (OutlineBounds.bIsValid)
	{
		const FVector2D Center = OutlineBounds.GetCenter();
		CachedCenter = FVector(Center.X, Center.Y, CachedCenter.Z);
	}

	// 플레이 시작 후에만 캐시한다. 에디터 인스턴스는 아직 컴포넌트를 추가 중일 수 있다.
	bAreaShapesCached = HasActorBegunPlay();
}
