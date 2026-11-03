#include "SheetAssetImport.h"
#include "SheetAssetIdResolver.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/DataTable.h"
#include "StructUtils/UserDefinedStruct.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/DataValidation.h"
#include "Misc/StringOutputDevice.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "Serialization/Csv/CsvParser.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Containers/StringConv.h"
#include <charconv>

namespace
{
	using FPreviewAssets = TMap<FString, TStrongObjectPtr<UObject>>;
	struct FPreparedAsset
	{
		FString PackagePath;
		TStrongObjectPtr<UObject> Candidate;
		TStrongObjectPtr<UObject> Existing;
	};
	struct FResolvedMapping
	{
		FSheetColumnMapping Mapping;
		int32 ColumnIndex = INDEX_NONE;
	};

	bool PropertyPathsOverlap(const FString& Left, const FString& Right)
	{
		return Left.Equals(Right, ESearchCase::IgnoreCase)
			|| Left.StartsWith(Right + TEXT("."), ESearchCase::IgnoreCase)
			|| Right.StartsWith(Left + TEXT("."), ESearchCase::IgnoreCase);
	}

	FString ObjectPath(const FString& PackagePath)
	{
		return PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(PackagePath);
	}

	bool ValidDestination(const FString& Path, FString& Error)
	{
		FText Reason;
		if (!Path.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(Path, false, &Reason)
			|| !FPackageName::IsValidObjectPath(ObjectPath(Path), &Reason))
		{
			Error = FString::Printf(TEXT("Invalid destination '%s'. Use a /Game/... package path without .uasset. %s"), *Path, *Reason.ToString());
			return false;
		}
		return true;
	}

	UObject* FindExisting(const FString& PackagePath, const FPreviewAssets* Preview)
	{
		const FString Path = ObjectPath(PackagePath);
		if (Preview)
			if (const TStrongObjectPtr<UObject>* Staged = Preview->Find(Path)) return Staged->Get();
		if (UObject* Loaded = FindObject<UObject>(nullptr, *Path)) return Loaded;
		return FPackageName::DoesPackageExist(PackagePath) ? LoadObject<UObject>(nullptr, *Path) : nullptr;
	}

	// FCsvParser handles quoted commas/newlines. Reject malformed quoting before its permissive parser runs.
	bool CheckCsv(const FString& Text, FString& Error)
	{
		enum class EState { Start, Plain, Quoted, Closed };
		EState State = EState::Start;
		for (int32 I = 0; I < Text.Len(); ++I)
		{
			const TCHAR C = Text[I];
			if (State == EState::Quoted)
			{
				if (C == TEXT('"'))
				{
					if (I + 1 < Text.Len() && Text[I + 1] == TEXT('"')) ++I;
					else State = EState::Closed;
				}
				continue;
			}
			if (C == TEXT(',') || C == TEXT('\r') || C == TEXT('\n')) { State = EState::Start; continue; }
			if (State == EState::Closed)
			{
				if (!FChar::IsWhitespace(C)) { Error = TEXT("Unexpected character after a CSV closing quote."); return false; }
			}
			else if (C == TEXT('"'))
			{
				if (State != EState::Start) { Error = TEXT("A CSV quote appears inside an unquoted cell."); return false; }
				State = EState::Quoted;
			}
			else State = EState::Plain;
		}
		if (State == EState::Quoted) { Error = TEXT("Unterminated CSV quoted cell."); return false; }
		return true;
	}

	FProperty* FindProperty(UStruct* Schema, const FString& Name)
	{
		if (FProperty* Property = FindFProperty<FProperty>(Schema, *Name)) return Property;
		for (TFieldIterator<FProperty> It(Schema); It; ++It)
			if (It->GetAuthoredName().Equals(Name, ESearchCase::IgnoreCase)) return *It;
		return nullptr;
	}

	bool ResolveProperty(UStruct* Schema, void* Container, const FString& Path,
		FProperty*& OutProperty, void*& OutValue, FString& Error)
	{
		TArray<FString> Parts;
		Path.ParseIntoArray(Parts, TEXT("."), false);
		if (Parts.IsEmpty()) { Error = TEXT("Property path is empty."); return false; }
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			FProperty* Property = FindProperty(Schema, Parts[Index]);
			if (!Property || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated | CPF_EditConst)
				|| (Index == 0 && Schema->IsA<UClass>() && !Property->HasAnyPropertyFlags(CPF_Edit)))
			{
				Error = FString::Printf(TEXT("Unknown or non-importable property '%s'."), *Path);
				return false;
			}
			void* Value = Property->ContainerPtrToValuePtr<void>(Container);
			if (Index == Parts.Num() - 1) { OutProperty = Property; OutValue = Value; return true; }
			FStructProperty* Struct = CastField<FStructProperty>(Property);
			if (!Struct) { Error = FString::Printf(TEXT("'%s' traverses something other than a struct."), *Path); return false; }
			Schema = Struct->Struct;
			Container = Value;
		}
		return false;
	}

	bool IsUnsignedInteger(const FNumericProperty* Number)
	{
		return CastField<FByteProperty>(Number) || CastField<FUInt16Property>(Number)
			|| CastField<FUInt32Property>(Number) || CastField<FUInt64Property>(Number);
	}

	template <typename T>
	bool ParseNumber(FString Text, T& Out)
	{
		Text.TrimStartAndEndInline();
		if (Text.StartsWith(TEXT("+"))) Text.RemoveAt(0);
		FTCHARToUTF8 Utf8(*Text);
		const auto Parsed = std::from_chars(Utf8.Get(), Utf8.Get() + Utf8.Length(), Out);
		return Parsed.ec == std::errc() && Parsed.ptr == Utf8.Get() + Utf8.Length();
	}

	FString ResolvePreviewText(FString Text, const FPreviewAssets* Preview)
	{
		if (Preview)
		{
			for (const auto& Pair : *Preview)
			{
				// Object paths only: do not replace a prefix inside a different asset's name.
				int32 At = Text.Find(Pair.Key);
				while (At != INDEX_NONE)
				{
					const int32 End = At + Pair.Key.Len();
					const bool bBoundary = End == Text.Len() || Text[End] == TEXT('"') || Text[End] == TEXT('\'')
						|| Text[End] == TEXT(',') || Text[End] == TEXT(')') || FChar::IsWhitespace(Text[End]);
					if (bBoundary)
					{
						const FString Replacement = Pair.Value->GetPathName();
						Text = Text.Left(At) + Replacement + Text.Mid(End);
						At += Replacement.Len();
					}
					else At = End;
					At = Text.Find(Pair.Key, ESearchCase::CaseSensitive, ESearchDir::FromStart, At);
				}
			}
		}
		return Text;
	}

	bool ImportValue(FProperty* Property, void* Value, const FString& Cell, ESheetCellFormat Format,
		UObject* Owner, const FPreviewAssets* Preview, FSheetAssetIdResolver& References, FString& Error)
	{
		const FString Text = ResolvePreviewText(Cell, Preview);
		if (Format == ESheetCellFormat::Auto)
		{
			const FString Trimmed = Text.TrimStartAndEnd();
			if (FStructProperty* Struct = CastField<FStructProperty>(Property);
				Struct && !Trimmed.StartsWith(TEXT("{")) && !Trimmed.StartsWith(TEXT("[")) && !Trimmed.StartsWith(TEXT("(")))
			{
				const FString ScalarPath = Struct->Struct->GetMetaData(TEXT("SheetScalarProperty"));
				if (!ScalarPath.IsEmpty())
				{
					FProperty* ScalarProperty = nullptr;
					void* ScalarValue = nullptr;
					if (!ResolveProperty(Struct->Struct, Value, ScalarPath, ScalarProperty, ScalarValue, Error)) return false;
					return ImportValue(ScalarProperty, ScalarValue, Cell, ESheetCellFormat::Auto, Owner, Preview, References, Error);
				}
			}
			const bool bComposite = CastField<FStructProperty>(Property) || CastField<FArrayProperty>(Property)
				|| CastField<FMapProperty>(Property) || CastField<FSetProperty>(Property);
			Format = bComposite && (Trimmed.StartsWith(TEXT("{")) || Trimmed.StartsWith(TEXT("[")))
				? ESheetCellFormat::Json : ESheetCellFormat::UnrealText;
		}
		if (Format == ESheetCellFormat::Json)
		{
			TSharedPtr<FJsonValue> Json;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json))
			{
				Error = TEXT("Cell is not valid JSON.");
				return false;
			}
			FText Reason;
			if (FNumericProperty* Number = CastField<FNumericProperty>(Property); Number && !Number->IsEnum())
			{
				if (Json->Type != EJson::Number && Json->Type != EJson::String) { Error = TEXT("Numeric JSON cell must be a number or numeric string."); return false; }
				return ImportValue(Property, Value, Json->Type == EJson::String ? Json->AsString() : Text,
					ESheetCellFormat::UnrealText, Owner, Preview, References, Error);
			}
			FString ReferenceError;
			const auto ImportReference = FJsonObjectConverter::CustomImportCallback::CreateLambda(
				[&](const TSharedPtr<FJsonValue>& JsonValue, FProperty* Field, void* Destination)
				{
					if (JsonValue->Type != EJson::String) return false;
					FString Failure;
					const bool bHandled = References.TryImport(Field, Destination, JsonValue->AsString(), Failure);
					if (ReferenceError.IsEmpty()) ReferenceError = Failure;
					// A failed ID lookup must not fall back to interpreting the ID as an object path.
					return bHandled;
				});
			const bool bImported = FJsonObjectConverter::JsonValueToUProperty(Json, Property, Value, 0,
				CPF_Transient | CPF_Deprecated, true, &Reason, &ImportReference);
			if (!ReferenceError.IsEmpty()) { Error = ReferenceError; return false; }
			if (!bImported)
			{
				Error = Reason.ToString();
				return false;
			}
			return true;
		}
		if (FStrProperty* String = CastField<FStrProperty>(Property)) { String->SetPropertyValue(Value, Text); return true; }
		if (FTextProperty* String = CastField<FTextProperty>(Property)) { String->SetPropertyValue(Value, FText::FromString(Text)); return true; }
		if (FNameProperty* Name = CastField<FNameProperty>(Property)) { Name->SetPropertyValue(Value, FName(*Text)); return true; }
		if (FBoolProperty* Bool = CastField<FBoolProperty>(Property))
		{
			if (Text.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Text == TEXT("1")) { Bool->SetPropertyValue(Value, true); return true; }
			if (Text.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Text == TEXT("0")) { Bool->SetPropertyValue(Value, false); return true; }
			Error = TEXT("Boolean cells must be true, false, 1, or 0.");
			return false;
		}
		if (FNumericProperty* Number = CastField<FNumericProperty>(Property); Number && !Number->IsEnum())
		{
			if (Number->IsFloatingPoint())
			{
				double Parsed = 0.0;
				if (ParseNumber(Text, Parsed) && FMath::IsFinite(Parsed)
					&& (!CastField<FFloatProperty>(Number) || FMath::Abs(Parsed) <= MAX_flt))
				{
					Number->SetFloatingPointPropertyValue(Value, Parsed);
					return true;
				}
			}
			else if (IsUnsignedInteger(Number))
			{
				uint64 Parsed = 0;
				if (ParseNumber(Text, Parsed) && Number->CanHoldValue(Parsed)) { Number->SetIntPropertyValue(Value, Parsed); return true; }
			}
			else
			{
				int64 Parsed = 0;
				if (ParseNumber(Text, Parsed) && Number->CanHoldValue(Parsed)) { Number->SetIntPropertyValue(Value, Parsed); return true; }
			}
			Error = TEXT("Numeric cell is invalid, non-finite, or outside the property's range.");
			return false;
		}
		if (References.TryImport(Property, Value, Text, Error)) return Error.IsEmpty();
		FStringOutputDevice Problems;
		const TCHAR* End = Property->ImportText_Direct(*Text, Value, Owner, PPF_None, &Problems);
		if (!End || !FString(End).TrimStartAndEnd().IsEmpty() || !Problems.IsEmpty())
		{
			Error = FString::Printf(TEXT("Cannot parse value as %s. %s"), *Property->GetCPPType(), *Problems);
			return false;
		}
		return true;
	}

	bool ValidateValue(FProperty* Property, void* Value, bool bReferences, FString& Error, int32 Depth = 0)
	{
		if (Depth > 64) { Error = TEXT("Property nesting exceeds 64 levels."); return false; }
		if (FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
		{
			const FSoftObjectPtr Ref = Soft->GetPropertyValue(Value);
			if (bReferences && !Ref.IsNull())
			{
				UObject* Object = Ref.LoadSynchronous();
				if (!Object || !Object->IsA(Soft->PropertyClass)) { Error = TEXT("Missing or incompatible soft asset reference."); return false; }
				if (FSoftClassProperty* Class = CastField<FSoftClassProperty>(Property))
					if (!Cast<UClass>(Object) || !Cast<UClass>(Object)->IsChildOf(Class->MetaClass)) { Error = TEXT("Soft class has an incompatible base class."); return false; }
			}
		}
		else if (FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
		{
			UObject* Ref = Object->GetObjectPropertyValue(Value);
			if (Ref && !Ref->IsA(Object->PropertyClass)) { Error = TEXT("Incompatible asset reference."); return false; }
			if (FClassProperty* Class = CastField<FClassProperty>(Property); Class && Ref)
				if (!Cast<UClass>(Ref) || !Cast<UClass>(Ref)->IsChildOf(Class->MetaClass)) { Error = TEXT("Class reference has an incompatible base class."); return false; }
		}
		else if (FNumericProperty* Number = CastField<FNumericProperty>(Property); Number && !Number->IsEnum())
		{
			const double N = Number->IsFloatingPoint() ? Number->GetFloatingPointPropertyValue(Value)
				: IsUnsignedInteger(Number) ? static_cast<double>(Number->GetUnsignedIntPropertyValue(Value))
				: static_cast<double>(Number->GetSignedIntPropertyValue(Value));
			if (!FMath::IsFinite(N)) { Error = TEXT("Non-finite numeric value."); return false; }
			for (const TCHAR* Bound : { TEXT("ClampMin"), TEXT("ClampMax") })
			{
				if (!Property->HasMetaData(Bound)) continue;
				double Limit;
				if (!LexTryParseString(Limit, *Property->GetMetaData(Bound))) continue;
				if (CastField<FFloatProperty>(Number)) Limit = static_cast<float>(Limit);
				if (FString(Bound) == TEXT("ClampMin") ? N < Limit : N > Limit)
				{
					Error = FString::Printf(TEXT("%s violates %s=%g."), *Property->GetName(), Bound, Limit);
					return false;
				}
			}
		}
		else if (FEnumProperty* Enum = CastField<FEnumProperty>(Property))
		{
			if (!Enum->GetEnum()->IsValidEnumValue(Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value)))
			{
				Error = TEXT("Invalid enum value.");
				return false;
			}
		}
		else if (FByteProperty* ByteEnum = CastField<FByteProperty>(Property); ByteEnum && ByteEnum->Enum)
		{
			if (!ByteEnum->Enum->IsValidEnumValue(ByteEnum->GetPropertyValue(Value))) { Error = TEXT("Invalid enum value."); return false; }
		}
		else if (FStructProperty* Struct = CastField<FStructProperty>(Property))
		{
			for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
				if (!It->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)
					&& !ValidateValue(*It, It->ContainerPtrToValuePtr<void>(Value), bReferences, Error, Depth + 1)) return false;
		}
		else if (FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(Array, Value);
			for (int32 I = 0; I < Helper.Num(); ++I)
				if (!ValidateValue(Array->Inner, Helper.GetRawPtr(I), bReferences, Error, Depth + 1)) return false;
		}
		else if (FMapProperty* Map = CastField<FMapProperty>(Property))
		{
			FScriptMapHelper Helper(Map, Value);
			for (int32 I = 0; I < Helper.GetMaxIndex(); ++I)
				if (Helper.IsValidIndex(I) && (!ValidateValue(Map->KeyProp, Helper.GetKeyPtr(I), bReferences, Error, Depth + 1)
					|| !ValidateValue(Map->ValueProp, Helper.GetValuePtr(I), bReferences, Error, Depth + 1))) return false;
		}
		else if (FSetProperty* Set = CastField<FSetProperty>(Property))
		{
			FScriptSetHelper Helper(Set, Value);
			for (int32 I = 0; I < Helper.GetMaxIndex(); ++I)
				if (Helper.IsValidIndex(I) && !ValidateValue(Set->ElementProp, Helper.GetElementPtr(I), bReferences, Error, Depth + 1)) return false;
		}
		return true;
	}

	bool ApplyRow(UStruct* Schema, void* Data, void* Defaults, const TArray<const TCHAR*>& Cells,
		const TArray<FResolvedMapping>& Mappings, UObject* Owner, USheetImportProfile* Profile, const FPreviewAssets* Preview, FSheetAssetIdResolver& References, FString& Error)
	{
		for (const FResolvedMapping& Mapping : Mappings)
		{
			const FString Cell = Mapping.ColumnIndex == INDEX_NONE ? FString() : FString(Cells[Mapping.ColumnIndex]);
			if (Cell.TrimStartAndEnd().IsEmpty())
			{
				if (Mapping.Mapping.bRequired) { Error = FString::Printf(TEXT("Required column '%s' is empty."), *Mapping.Mapping.Column); return false; }
				if (Mapping.Mapping.bSkipEmpty) continue;
			}
			FProperty* Property = nullptr;
			void* Value = nullptr;
			if (!ResolveProperty(Schema, Data, Mapping.Mapping.PropertyPath, Property, Value, Error)) return false;
			if (Cell.TrimStartAndEnd().IsEmpty())
			{
				FProperty* DefaultProperty = nullptr;
				void* DefaultValue = nullptr;
				if (!ResolveProperty(Schema, Defaults, Mapping.Mapping.PropertyPath, DefaultProperty, DefaultValue, Error)) return false;
				Property->CopyCompleteValue(Value, DefaultValue);
			}
			else if (!ImportValue(Property, Value, Cell, Mapping.Mapping.Format, Owner, Preview, References, Error))
			{
				Error = Mapping.Mapping.Column + TEXT(" -> ") + Mapping.Mapping.PropertyPath + TEXT(": ") + Error;
				return false;
			}
			if (!ValidateValue(Property, Value, Profile->bValidateReferences, Error))
			{
				Error = Mapping.Mapping.Column + TEXT(": ") + Error;
				return false;
			}
		}
		return true;
	}

	bool ValidateAsset(UObject* Asset, FSheetImportReport& Report)
	{
		FDataValidationContext Context;
		const EDataValidationResult Result = Asset->IsDataValid(Context);
		for (const auto& Issue : Context.GetIssues())
			Report.Messages.Add((Issue.Severity == EMessageSeverity::Error ? TEXT("Error: ") : TEXT("Validation: ")) + Issue.Message.ToString());
		return Result != EDataValidationResult::Invalid && Context.GetNumErrors() == 0;
	}
}

FSheetImportReport SheetAssetImport::Import(USheetImportProfile* Profile, const FString& Csv, const bool bPreview, FPreviewAssets* PreviewAssets)
{
	FSheetImportReport Report;
	Report.bPreviewOnly = bPreview;
	auto Fail = [&](const FString& Message) { Report.Messages.Add(Message); return Report; };
	if (!IsInGameThread() || !IsValid(Profile)) return Fail(TEXT("Import requires a valid profile on the editor thread."));
	if (Csv.Len() > 10 * 1024 * 1024) return Fail(TEXT("CSV exceeds the 10 Mi-character import limit."));
	FString Text = Csv;
	if (Text.Len() && Text[0] == 0xFEFF) Text.RemoveAt(0);
	if (Text.TrimStart().StartsWith(TEXT("<"))) return Fail(TEXT("Received HTML/XML instead of CSV. Check the export URL and sheet access."));
	FString Error;
	FSheetAssetIdResolver References(Profile, PreviewAssets);
	if (!References.ValidateSettings(Error)) return Fail(Error);
	if (!CheckCsv(Text, Error)) return Fail(Error);
	FCsvParser Parser(MoveTemp(Text));
	const FCsvParser::FRows& Rows = Parser.GetRows();
	if (Rows.Num() < 2 || Rows.Num() > 50001) return Fail(TEXT("CSV must contain a header and 1..50000 data rows."));
	const auto& Header = Rows[0];
	TMap<FName, int32> Columns;
	for (int32 I = 0; I < Header.Num(); ++I)
	{
		const FName Name(*FString(Header[I]).TrimStartAndEnd());
		if (Name.IsNone() || Columns.Contains(Name)) return Fail(TEXT("CSV contains an empty or duplicate column header."));
		Columns.Add(Name, I);
	}
	FString KeyColumn = Profile->KeyColumn.TrimStartAndEnd();
	if (KeyColumn.IsEmpty())
	{
		for (const TCHAR* Common : { TEXT("Name"), TEXT("ID"), TEXT("ItemId"), TEXT("RowName"), TEXT("AssetName") })
			if (Columns.Contains(FName(Common))) { KeyColumn = Common; break; }
		if (KeyColumn.IsEmpty()) KeyColumn = FString(Header[0]).TrimStartAndEnd();
	}
	const int32* KeyIndex = Columns.Find(FName(*KeyColumn));
	if (!KeyIndex) return Fail(TEXT("KeyColumn was not found in the CSV header."));
	Report.Messages.Add(TEXT("식별자 열: ") + KeyColumn);
	UScriptStruct* RowStruct = nullptr;
	UClass* AssetClass = nullptr;
	UStruct* Schema = nullptr;
	FString TargetTablePath = Profile->DataTablePath;
	if (Profile->Target == ESheetImportTarget::DataTable)
	{
		if (!Profile->ExistingDataTable.IsNull())
		{
			UDataTable* Table = Profile->ExistingDataTable.LoadSynchronous();
			if (!Table) return Fail(TEXT("선택한 기존 데이터 테이블을 읽을 수 없습니다."));
			RowStruct = Table->RowStruct;
			TargetTablePath = Table->GetOutermost()->GetName();
		}
		else RowStruct = LoadObject<UScriptStruct>(nullptr, *Profile->RowStructPath);
		if (!RowStruct || (!RowStruct->IsChildOf(FTableRowBase::StaticStruct()) && !RowStruct->IsA<UUserDefinedStruct>()))
			return Fail(TEXT("RowStructPath must resolve to an FTableRowBase-derived native struct or a user-defined struct."));
		if (!ValidDestination(TargetTablePath, Error)) return Fail(Error);
		Schema = RowStruct;
	}
	else
	{
		AssetClass = Profile->DataAssetClass.LoadSynchronous();
		if (!AssetClass || !AssetClass->IsChildOf(UDataAsset::StaticClass()) || AssetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
			return Fail(TEXT("DataAssetClass must be a concrete UDataAsset/UPrimaryDataAsset subclass."));
		Schema = AssetClass;
	}
	for (TFieldIterator<FProperty> It(Schema); It; ++It)
	{
		if (!It->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)
			&& It->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ContainsInstancedReference))
			return Fail(TEXT("Instanced UObject fields are not supported: ") + It->GetName() + TEXT(". Import separate DataAssets and reference them by path instead."));
	}
	TArray<FResolvedMapping> Mappings;
	TSet<FName> ExplicitColumns;
	TArray<FString> PropertyPaths;
	for (const FSheetColumnMapping& Mapping : Profile->ColumnMappings)
	{
		if (Mapping.Column.IsEmpty() || Mapping.PropertyPath.IsEmpty() || ExplicitColumns.Contains(FName(*Mapping.Column)))
			return Fail(TEXT("Explicit mappings require unique, nonempty columns and property paths."));
		const int32* Index = Columns.Find(FName(*Mapping.Column));
		if (!Index && Mapping.bRequired) return Fail(TEXT("Required column is missing: ") + Mapping.Column);
		Mappings.Add({ Mapping, Index ? *Index : INDEX_NONE });
		ExplicitColumns.Add(FName(*Mapping.Column));
		PropertyPaths.Add(Mapping.PropertyPath);
	}
	if (Profile->bAutoMapColumns)
	{
		for (const auto& Pair : Columns)
		{
			if (ExplicitColumns.Contains(Pair.Key)) continue;
			FProperty* Property = FindProperty(Schema, Pair.Key.ToString());
			if (!Property || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated | CPF_EditConst)
				|| (AssetClass && !Property->HasAnyPropertyFlags(CPF_Edit))) continue;
			FSheetColumnMapping Mapping;
			Mapping.Column = Pair.Key.ToString();
			Mapping.PropertyPath = Property->GetName();
			// Explicit mappings own their destination, including a struct's parent/child paths.
			const bool bExplicitDestination = Profile->ColumnMappings.ContainsByPredicate([&Mapping](const FSheetColumnMapping& Explicit)
			{
				return PropertyPathsOverlap(Explicit.PropertyPath, Mapping.PropertyPath);
			});
			if (bExplicitDestination) continue;
			Mappings.Add({ Mapping, Pair.Value });
			PropertyPaths.Add(Mapping.PropertyPath);
		}
	}
	if (Mappings.IsEmpty()) return Fail(TEXT("No importable columns. Add mappings or enable matching property headers."));
	TUniquePtr<FStructOnScope> MappingDefaults = RowStruct ? MakeUnique<FStructOnScope>(RowStruct) : nullptr;
	for (const FResolvedMapping& Mapping : Mappings)
	{
		FProperty* Property = nullptr;
		void* Value = nullptr;
		void* Container = RowStruct ? static_cast<void*>(MappingDefaults->GetStructMemory()) : static_cast<void*>(AssetClass->GetDefaultObject());
		if (!ResolveProperty(Schema, Container, Mapping.Mapping.PropertyPath, Property, Value, Error)) return Fail(Error);
	}
	// Parent and child writes would otherwise depend on map iteration order.
	for (int32 I = 0; I < PropertyPaths.Num(); ++I)
		for (int32 J = I + 1; J < PropertyPaths.Num(); ++J)
			if (PropertyPathsOverlap(PropertyPaths[I], PropertyPaths[J]))
				return Fail(TEXT("Mappings overlap: ") + PropertyPaths[I] + TEXT(" and ") + PropertyPaths[J]);
	TArray<FPreparedAsset> Prepared;
	UDataTable* CandidateTable = nullptr;
	UDataTable* ExistingTable = nullptr;
	TUniquePtr<FStructOnScope> Defaults;
	if (RowStruct)
	{
		UObject* Existing = FindExisting(TargetTablePath, PreviewAssets);
		ExistingTable = Cast<UDataTable>(Existing);
		if ((Existing && !ExistingTable) || (ExistingTable && ExistingTable->RowStruct != RowStruct))
			return Fail(TEXT("Destination already exists with a different asset/row type."));
		CandidateTable = NewObject<UDataTable>(GetTransientPackage());
		CandidateTable->RowStruct = RowStruct;
		Prepared.Add({ TargetTablePath, TStrongObjectPtr<UObject>(CandidateTable), TStrongObjectPtr<UObject>(Existing) });
		if (ExistingTable && !Profile->bRemoveMissingTableRows)
			for (const auto& Pair : ExistingTable->GetRowMap()) CandidateTable->AddRow(Pair.Key, Pair.Value, RowStruct);
		Defaults = MoveTemp(MappingDefaults);
	}
	TSet<FName> Keys;
	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		const auto& Cells = Rows[RowIndex];
		bool bEmpty = true;
		for (const TCHAR* Cell : Cells) bEmpty &= FString(Cell).TrimStartAndEnd().IsEmpty();
		if (bEmpty) continue;
		if (Cells.Num() != Header.Num()) return Fail(FString::Printf(TEXT("CSV record %d has %d cells; expected %d."), RowIndex + 1, Cells.Num(), Header.Num()));
		const FString KeyText = FString(Cells[*KeyIndex]).TrimStartAndEnd();
		const FName Key(*KeyText);
		if (Key.IsNone() || Keys.Contains(Key)) return Fail(FString::Printf(TEXT("CSV record %d has an empty/duplicate key '%s'."), RowIndex + 1, *KeyText));
		Keys.Add(Key);
		if (RowStruct)
		{
			FStructOnScope Row(RowStruct);
			if (ExistingTable && Profile->bPreserveUnmappedValues)
				if (const uint8* const* ExistingRow = ExistingTable->GetRowMap().Find(Key)) RowStruct->CopyScriptStruct(Row.GetStructMemory(), *ExistingRow);
			if (!ApplyRow(Schema, Row.GetStructMemory(), Defaults->GetStructMemory(), Cells, Mappings, CandidateTable, Profile, PreviewAssets, References, Error))
				return Fail(FString::Printf(TEXT("Record %d (%s): %s"), RowIndex + 1, *KeyText, *Error));
			CandidateTable->AddRow(Key, Row.GetStructMemory(), RowStruct);
		}
		else
		{
			const FString Name = Profile->AssetNamePrefix + KeyText;
			if (Name != ObjectTools::SanitizeObjectName(Name)) return Fail(TEXT("Key/prefix contains invalid asset-name characters: ") + Name);
			const FString Path = Profile->OutputDirectory / Name;
			if (!ValidDestination(Path, Error)) return Fail(Error);
			UObject* Existing = FindExisting(Path, PreviewAssets);
			if (Existing && Existing->GetClass() != AssetClass) return Fail(TEXT("Destination asset has a different class: ") + Path);
			UDataAsset* Candidate = Existing && Profile->bPreserveUnmappedValues
				? DuplicateObject<UDataAsset>(CastChecked<UDataAsset>(Existing), GetTransientPackage())
				: NewObject<UDataAsset>(GetTransientPackage(), AssetClass);
			Prepared.Add({ Path, TStrongObjectPtr<UObject>(Candidate), TStrongObjectPtr<UObject>(Existing) });
			if (!ApplyRow(Schema, Candidate, AssetClass->GetDefaultObject(), Cells, Mappings, Candidate, Profile, PreviewAssets, References, Error))
				return Fail(FString::Printf(TEXT("Record %d (%s): %s"), RowIndex + 1, *KeyText, *Error));
		}
		++Report.RowCount;
	}
	if (Report.RowCount == 0) return Fail(TEXT("There are no nonempty data rows. Importing an empty sheet is not supported."));
	for (const FPreparedAsset& Asset : Prepared)
	{
		if (Profile->bValidateAssets && !ValidateAsset(Asset.Candidate.Get(), Report)) return Fail(TEXT("Asset validation failed: ") + Asset.PackagePath);
		if (Profile->bSaveAssets)
		{
			const FString Filename = FPackageName::LongPackageNameToFilename(Asset.PackagePath, FPackageName::GetAssetPackageExtension());
			if (IFileManager::Get().IsReadOnly(*Filename)) return Fail(TEXT("Destination is read-only; check it out before importing: ") + Asset.PackagePath);
		}
	}
	if (bPreview)
	{
		for (const FPreparedAsset& Asset : Prepared)
		{
			Report.AssetPaths.Add(ObjectPath(Asset.PackagePath));
			if (Asset.Existing.IsValid()) ++Report.UpdatedAssetCount; else ++Report.CreatedAssetCount;
			if (PreviewAssets) PreviewAssets->Add(ObjectPath(Asset.PackagePath), TStrongObjectPtr<UObject>(Asset.Candidate.Get()));
		}
		Report.bSuccess = true;
		Report.Messages.Add(TEXT("Preview passed. No destination assets were changed or saved."));
		return Report;
	}
	// Every record is parsed and validated before the first destination is touched.
	const FScopedTransaction Transaction(NSLOCTEXT("SheetAssetImporter", "ImportTransaction", "Import sheet assets"));
	bool bAllSaved = true;
	for (const FPreparedAsset& Asset : Prepared)
	{
		UObject* Target = Asset.Existing.Get();
		const bool bNew = Target == nullptr;
		if (bNew)
		{
			UPackage* Package = CreatePackage(*Asset.PackagePath);
			Target = NewObject<UObject>(Package, Asset.Candidate->GetClass(), *FPackageName::GetLongPackageAssetName(Asset.PackagePath), RF_Public | RF_Standalone | RF_Transactional);
		}
		Target->SetFlags(RF_Transactional);
		Target->Modify();
		if (UDataTable* Table = Cast<UDataTable>(Target))
		{
			Table->RowStruct = RowStruct;
			Table->EmptyTable();
			for (const auto& Pair : CandidateTable->GetRowMap()) Table->AddRow(Pair.Key, Pair.Value, RowStruct);
		}
		else
		{
			for (TFieldIterator<FProperty> It(AssetClass); It; ++It)
				if (It->HasAnyPropertyFlags(CPF_Edit) && !It->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
					It->CopyCompleteValue_InContainer(Target, Asset.Candidate.Get());
		}
		Target->PostEditChange();
		Target->MarkPackageDirty();
		if (bNew) { FAssetRegistryModule::AssetCreated(Target); ++Report.CreatedAssetCount; }
		else ++Report.UpdatedAssetCount;
		Report.AssetPaths.Add(Target->GetPathName());
		if (Profile->bSaveAssets)
		{
			const FString Filename = FPackageName::LongPackageNameToFilename(Asset.PackagePath, FPackageName::GetAssetPackageExtension());
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			Args.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Target->GetOutermost(), Target, *Filename, Args))
			{
				Report.Messages.Add(TEXT("Asset was updated in memory but could not be saved: ") + Target->GetPathName());
				bAllSaved = false;
			}
		}
	}
	Report.bSuccess = bAllSaved;
	Report.Messages.Add(Profile->bSaveAssets ? TEXT("Imported assets and attempted to save their packages.") : TEXT("Imported assets are dirty. Review and save them in the editor."));
	return Report;
}
