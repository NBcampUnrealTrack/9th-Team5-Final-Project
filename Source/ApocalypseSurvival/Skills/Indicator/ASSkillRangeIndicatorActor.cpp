#include "Skills/Indicator/ASSkillRangeIndicatorActor.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Core/Framework/ASPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace ASSkillRangeIndicatorPrivate
{
	//데칼은 로컬 X축으로 투영되므로 아래를 보게 눕힘. 이때 로컬 Z축이 Yaw 방향(앞)을 향함
	FRotator MakeDecalRotation(float Yaw)
	{
		return FRotator(-90.0f, Yaw, 0.0f);
	}

	UDecalComponent* CreateDecal(AActor* Owner, USceneComponent* Parent, const FName Name, int32 SortOrder)
	{
		UDecalComponent* Decal = Owner->CreateDefaultSubobject<UDecalComponent>(Name);
		Decal->SetupAttachment(Parent);
		Decal->SetUsingAbsoluteLocation(true);
		Decal->SetUsingAbsoluteRotation(true);
		Decal->SetUsingAbsoluteScale(true);
		Decal->SetVisibility(false);
		Decal->SortOrder = SortOrder;
		//화면에서 작아져도 사라지지 않게
		Decal->FadeScreenSize = 0.0f;
		return Decal;
	}
}

AASSkillRangeIndicatorActor::AASSkillRangeIndicatorActor()
{
	PrimaryActorTick.bCanEverTick = true;
	//시전자 이동이 끝난 뒤 따라가도록
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	bReplicates = false;
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	RangeDecal = ASSkillRangeIndicatorPrivate::CreateDecal(this, SceneRoot, TEXT("RangeDecal"), 0);
	AreaDecal = ASSkillRangeIndicatorPrivate::CreateDecal(this, SceneRoot, TEXT("AreaDecal"), 1);
}

void AASSkillRangeIndicatorActor::InitIndicator(APawn* InCaster, const FASSkillIndicatorSettings& InSettings)
{
	Caster = InCaster;
	Settings = InSettings;

	if (RangeMaterial)
	{
		RangeMID = UMaterialInstanceDynamic::Create(RangeMaterial, this);
		RangeDecal->SetDecalMaterial(RangeMID);
	}

	if (UMaterialInterface* AreaMaterial = GetAreaMaterial())
	{
		AreaMID = UMaterialInstanceDynamic::Create(AreaMaterial, this);
		AreaDecal->SetDecalMaterial(AreaMID);

		if (Settings.Shape == EASSkillIndicatorShape::Cone && !ConeAngleParameterName.IsNone())
		{
			AreaMID->SetScalarParameterValue(ConeAngleParameterName, Settings.Angle * ConeAngleDisplayScale);
		}
	}

	//DecalSize = (투영 깊이, 좌우 절반, 앞뒤 절반)
	const float RingRadius = FMath::Max(0.0f, Settings.RangeRingRadius);
	
	//RangeDecal->DecalSize = FVector(DecalDepth, RingRadius, RingRadius);
	
	if (Settings.Shape == EASSkillIndicatorShape::Cone)
	{
		RangeDecal->DecalSize = FVector(DecalDepth, RingRadius * ConeRadiusDisplayScale, RingRadius * ConeRadiusDisplayScale);
	}
	else if (Settings.Shape == EASSkillIndicatorShape::Line)
	{
		RangeDecal->DecalSize = FVector(DecalDepth, RingRadius * LineRadiusDisplayScale, RingRadius * LineRadiusDisplayScale);
	}
	else
	{
		RangeDecal->DecalSize = FVector(DecalDepth, RingRadius, RingRadius);
	}
	
	RangeDecal->MarkRenderStateDirty();
	RangeDecal->SetVisibility(RingRadius > 0.0f && RangeMID != nullptr);

	if (Settings.Shape == EASSkillIndicatorShape::Line)
	{
		AreaDecal->DecalSize = FVector(DecalDepth, Settings.Width * 0.5f * LineWidthDisplayScale, Settings.Length * 0.5f * LineRadiusDisplayScale);
	}
	else if (Settings.Shape == EASSkillIndicatorShape::Cone)
	{
		AreaDecal->DecalSize = FVector(DecalDepth, Settings.Radius * ConeRadiusDisplayScale, Settings.Radius * ConeRadiusDisplayScale);
	}
	else
	{
		AreaDecal->DecalSize = FVector(DecalDepth, Settings.Radius * CircleRadiusDisplayScale, Settings.Radius * CircleRadiusDisplayScale);
	}
	AreaDecal->MarkRenderStateDirty();
	AreaDecal->SetVisibility(AreaMID != nullptr);

	UpdateIndicator();
	BP_OnIndicatorInitialized();
}

void AASSkillRangeIndicatorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateIndicator();
}

void AASSkillRangeIndicatorActor::UpdateIndicator()
{
	APawn* CasterPawn = Caster.Get();
	if (!IsValid(CasterPawn))
	{
		Destroy();
		return;
	}

	const FVector Origin = CasterPawn->GetActorLocation();
	SetActorLocation(Origin);

	FVector CursorLocation;
	if (GetCursorWorldLocation(CasterPawn, CursorLocation))
	{
		LastCursorLocation = CursorLocation;
		bHasCursorLocation = true;
	}

	const FVector Desired = bHasCursorLocation
		? LastCursorLocation
		: Origin + CasterPawn->GetActorForwardVector() * FMath::Max(Settings.MaxCastRange, 100.0f);

	const FVector ToDesired = (Desired - Origin).GetSafeNormal2D();
	if (!ToDesired.IsNearlyZero())
	{
		AimDirection = ToDesired;
	}

	switch (Settings.TargetingType)
	{
	case EASSkillTargetingType::Location:
		//걸어가서 시전 모드면 사거리 제한 없이 커서 위치 그대로 (바닥 높이만 맞춤)
		TargetLocation = ResolveTargetLocation(CasterPawn, Desired, Settings.bClampToRange ? Settings.MaxCastRange : 0.0f);
		break;
	case EASSkillTargetingType::Self:
		TargetLocation = Origin;
		break;
	default:
		TargetLocation = Origin + AimDirection * Settings.ForwardOffset;
		break;
	}

	const float Yaw = AimDirection.Rotation().Yaw;

	RangeDecal->SetWorldLocationAndRotation(Origin, ASSkillRangeIndicatorPrivate::MakeDecalRotation(0.0f));

	FVector AreaLocation = TargetLocation;
	if (Settings.TargetingType == EASSkillTargetingType::Direction && Settings.Shape == EASSkillIndicatorShape::Line)
	{
		//데칼 위치 수정
		AreaLocation = Origin + AimDirection * (Settings.Length * 0.5f * LineRadiusDisplayScale);
	}

	AreaDecal->SetWorldLocationAndRotation(AreaLocation, ASSkillRangeIndicatorPrivate::MakeDecalRotation(Yaw));
}

UMaterialInterface* AASSkillRangeIndicatorActor::GetAreaMaterial() const
{
	switch (Settings.Shape)
	{
	case EASSkillIndicatorShape::Cone:
		return ConeMaterial ? ConeMaterial.Get() : CircleMaterial.Get();
	case EASSkillIndicatorShape::Line:
		return LineMaterial ? LineMaterial.Get() : CircleMaterial.Get();
	default:
		return CircleMaterial;
	}
}

bool AASSkillRangeIndicatorActor::GetCursorWorldLocation(const APawn* InCaster, FVector& OutLocation)
{
	const APlayerController* PlayerController = InCaster ? Cast<APlayerController>(InCaster->GetController()) : nullptr;
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return false;
	}

	//팀 컨트롤러의 커서 트레이스(클릭 이동과 같은 채널)를 우선 사용
	if (const AASPlayerController* ASPlayerController = Cast<AASPlayerController>(PlayerController))
	{
		return ASPlayerController->GetCursorWorldLocation(OutLocation);
	}

	FHitResult Hit;
	if (PlayerController->GetHitResultUnderCursor(ECC_Visibility, true, Hit))
	{
		OutLocation = Hit.Location;
		return true;
	}

	return false;
}

FVector AASSkillRangeIndicatorActor::ResolveTargetLocation(const AActor* InCaster, const FVector& DesiredLocation, float MaxCastRange)
{
	if (!InCaster)
	{
		return DesiredLocation;
	}

	const FVector Origin = InCaster->GetActorLocation();

	FVector Offset = DesiredLocation - Origin;
	Offset.Z = 0.0f;
	if (MaxCastRange > 0.0f)
	{
		Offset = Offset.GetClampedToMaxSize2D(MaxCastRange);
	}

	FVector Result = Origin + Offset;

	//바닥 높이에 맞춤 (몬스터·캐릭터에 걸리지 않게 지형만 검사)
	if (const UWorld* World = InCaster->GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ASSkillIndicatorGround), false, InCaster);
		const FVector Start = Result + FVector(0.0f, 0.0f, 500.0f);
		const FVector End = Result - FVector(0.0f, 0.0f, 1000.0f);

		if (World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Result.Z = Hit.ImpactPoint.Z;
		}
	}

	return Result;
}
