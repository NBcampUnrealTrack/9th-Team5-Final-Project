#include "Characters/Enemy/Data/ASMonsterPatternSet.h"

#include "Misc/DataValidation.h"

const FASMonsterPatternEntry* FASResolvedMonsterPatternSet::FindPattern(const FGameplayTag PatternId) const
{
    return PatternEntries.FindByPredicate([PatternId](const FASMonsterPatternEntry& Entry)
    {
        return Entry.bEnabled && Entry.PatternId.MatchesTagExact(PatternId);
    });
}

bool UASMonsterPatternSet::ResolvePatternSet(FASResolvedMonsterPatternSet& OutResolved) const
{
    OutResolved = FASResolvedMonsterPatternSet();
    TSet<const UASMonsterPatternSet*> VisitingSets;
    return ResolvePatternSetInternal(OutResolved, VisitingSets);
}

bool UASMonsterPatternSet::ResolvePatternConfig(const FGameplayTag PatternId, FASMonsterPatternEntry& OutConfig) const
{
    FASResolvedMonsterPatternSet ResolvedSet;
    const FASMonsterPatternEntry* ResolvedEntry = ResolvePatternSet(ResolvedSet)
        ? ResolvedSet.FindPattern(PatternId)
        : nullptr;
    if (!ResolvedEntry)
    {
        return false;
    }

    OutConfig = *ResolvedEntry;
    return true;
}

bool UASMonsterPatternSet::ResolvePatternSetInternal(FASResolvedMonsterPatternSet& OutResolved,
    TSet<const UASMonsterPatternSet*>& VisitingSets) const
{
    if (VisitingSets.Contains(this))
    {
        UE_LOG(LogTemp, Error, TEXT("[MonsterPatternSet] ParentPatternSet cycle detected at %s."), *GetNameSafe(this));
        return false;
    }

    VisitingSets.Add(this);
    if (ParentPatternSet && !ParentPatternSet->ResolvePatternSetInternal(OutResolved, VisitingSets))
    {
        VisitingSets.Remove(this);
        return false;
    }

    if (bOverrideInterPatternDelay || !ParentPatternSet)
    {
        OutResolved.InterPatternDelay = FMath::Max(0.0f, InterPatternDelay);
    }

    if (bOverrideSelectionPolicy || !ParentPatternSet)
    {
        OutResolved.SelectionPolicy = SelectionPolicy;
    }

    if (bOverrideSelectionPolicyConfig || !ParentPatternSet)
    {
        OutResolved.SelectionPolicyConfig = SelectionPolicyConfig;
    }

    TMap<FGameplayTag, int32> ResolvedIndices;
    for (int32 Index = 0; Index < OutResolved.PatternEntries.Num(); ++Index)
    {
        ResolvedIndices.Add(OutResolved.PatternEntries[Index].PatternId, Index);
    }

    TSet<FGameplayTag> LocalPatternIds;
    for (const FASMonsterPatternEntry& LocalEntry : PatternEntries)
    {
        if (!LocalEntry.PatternId.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("[MonsterPatternSet] %s has an entry without PatternId."), *GetNameSafe(this));
            VisitingSets.Remove(this);
            return false;
        }

        if (LocalPatternIds.Contains(LocalEntry.PatternId))
        {
            UE_LOG(LogTemp, Error, TEXT("[MonsterPatternSet] %s has duplicate PatternId %s."),
                *GetNameSafe(this), *LocalEntry.PatternId.ToString());
            VisitingSets.Remove(this);
            return false;
        }
        LocalPatternIds.Add(LocalEntry.PatternId);

        if (int32* ExistingIndex = ResolvedIndices.Find(LocalEntry.PatternId))
        {
            OutResolved.PatternEntries[*ExistingIndex] = LocalEntry;
        }
        else
        {
            ResolvedIndices.Add(LocalEntry.PatternId, OutResolved.PatternEntries.Add(LocalEntry));
        }
    }

    VisitingSets.Remove(this);
    return true;
}

#if WITH_EDITOR
EDataValidationResult UASMonsterPatternSet::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    TSet<FGameplayTag> SeenPatternIds;
    for (const FASMonsterPatternEntry& Entry : PatternEntries)
    {
        if (!Entry.PatternId.IsValid())
        {
            Context.AddError(FText::FromString(TEXT("PatternEntries에 PatternId가 비어 있는 항목이 있습니다.")));
            Result = EDataValidationResult::Invalid;
            continue;
        }

        if (SeenPatternIds.Contains(Entry.PatternId))
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("PatternEntries에 PatternId '%s'가 중복됩니다."), *Entry.PatternId.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        SeenPatternIds.Add(Entry.PatternId);

        if (Entry.bEnabled && !Entry.AbilityClass)
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("Pattern '%s'는 활성화되었지만 AbilityClass가 없습니다."), *Entry.PatternId.ToString())));
            Result = EDataValidationResult::Invalid;
        }
    }

    FASResolvedMonsterPatternSet ResolvedSet;
    if (!ResolvePatternSet(ResolvedSet))
    {
        Context.AddError(FText::FromString(TEXT("ParentPatternSet을 해석할 수 없습니다. 순환 참조와 PatternId를 확인하세요.")));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}
#endif
