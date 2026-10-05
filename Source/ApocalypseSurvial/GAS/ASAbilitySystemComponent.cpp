#include "GAS/ASAbilitySystemComponent.h"

UASAbilitySystemComponent::UASAbilitySystemComponent()
{
	// 복제를 켜야 클라이언트가 어트리뷰트와 태그를 받는다.
	SetIsReplicatedByDefault(true);
}
