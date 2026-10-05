#include "Map/World/ASZoneLinkDebugComponent.h"

#include "Map/World/ASMapZone.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "EngineUtils.h"
#endif

#if WITH_EDITOR
namespace ASZoneLinkDebugPrivate
{
	TArray<const AASMapZone*> GetLinkedZones(const AASMapZone* Zone)
	{
		TArray<const AASMapZone*> Result;
		for (const AASMapZone* AdjacentZone : Zone->GetAdjacentZones())
		{
			if (IsValid(AdjacentZone) && AdjacentZone != Zone)
			{
				Result.AddUnique(AdjacentZone);
			}
		}
		if (UWorld* World = Zone->GetWorld())
		{
			for (TActorIterator<AASMapZone> It(World); It; ++It)
			{
				if (*It != Zone && It->GetAdjacentZones().Contains(Zone))
				{
					Result.AddUnique(*It);
				}
			}
		}
		return Result;
	}

	class FZoneLinkSceneProxy : public FDebugRenderSceneProxy
	{
	public:
		FZoneLinkSceneProxy(const UASZoneLinkDebugComponent* Component, const AASMapZone* Zone)
			: FDebugRenderSceneProxy(Component)
		{
			const FVector Center = Zone->GetZoneCenter();
			for (const AASMapZone* AdjacentZone : ASZoneLinkDebugPrivate::GetLinkedZones(Zone))
			{
				if (IsValid(AdjacentZone))
				{
					Lines.Emplace(Center, AdjacentZone->GetZoneCenter(), FColor::Cyan, 2.0f);
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			// 엔진의 PushSelectionToProxy가 액터 선택 변경을 렌더 스레드에 전달한다.
			Result.bDrawRelevance = IsShown(View) && IsParentSelected() && !View->Family->EngineShowFlags.Game;
			Result.bDynamicRelevance = true;
			Result.bShadowRelevance = false;
			Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
			return Result;
		}
	};
}
#endif

UASZoneLinkDebugComponent::UASZoneLinkDebugComponent()
{
	bIsEditorOnly = true;
	SetHiddenInGame(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	CastShadow = false;
	bSelectable = true;
	bUseEditorCompositing = true;
}

FBoxSphereBounds UASZoneLinkDebugComponent::CalcBounds(const FTransform& LocalToWorld) const
{
#if WITH_EDITOR
	if (const AASMapZone* Zone = Cast<AASMapZone>(GetOwner()))
	{
		// 끝점 전체를 포함해야 소유 구역이 화면 밖에 있어도 연결선이 보인다.
		FBox LinkBounds(ForceInit);
		LinkBounds += Zone->GetZoneCenter();
		for (const AASMapZone* AdjacentZone : ASZoneLinkDebugPrivate::GetLinkedZones(Zone))
		{
			if (IsValid(AdjacentZone))
			{
				LinkBounds += AdjacentZone->GetZoneCenter();
			}
		}
		return FBoxSphereBounds(LinkBounds.ExpandBy(2.0));
	}
#endif
	return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector(2.0), 2.0);
}

FDebugRenderSceneProxy* UASZoneLinkDebugComponent::CreateDebugSceneProxy()
{
#if WITH_EDITOR
	if (const AASMapZone* Zone = Cast<AASMapZone>(GetOwner()))
	{
		return new ASZoneLinkDebugPrivate::FZoneLinkSceneProxy(this, Zone);
	}
#endif
	return nullptr;
}
