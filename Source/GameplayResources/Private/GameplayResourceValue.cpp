
#include "GameplayResourceValue.h"

#include "GameplayResourcesDeveloperSettings.h"
#include "GameplayResourceSubsystem.h"
#include "GameplayResourceWildcardValue.h"

namespace UE::GameplayResources::Private
{
	// How a property's value can be ordered. Numeric kinds compare across each other; the rest only with themselves.
	enum class EOrderKind : uint8
	{
		None,
		Signed,
		Unsigned,
		Floating,
		String,
		Name,
		Text,
	};

	EOrderKind GetOrderKind(const FPropertyBagPropertyDesc& Desc)
	{
		if (Desc.ContainerTypes.Num() > 0)
		{
			return EOrderKind::None;
		}

		switch (Desc.ValueType)
		{
		case EPropertyBagPropertyType::Bool:	return EOrderKind::None;	// numeric to the bag, but "less than true" means nothing
		case EPropertyBagPropertyType::Enum:	return EOrderKind::Signed;
		case EPropertyBagPropertyType::String:	return EOrderKind::String;
		case EPropertyBagPropertyType::Name:	return EOrderKind::Name;
		case EPropertyBagPropertyType::Text:	return EOrderKind::Text;
		default:								break;
		}

		if (Desc.IsNumericFloatType())
		{
			return EOrderKind::Floating;
		}
		if (Desc.IsUnsignedNumericType())
		{
			return EOrderKind::Unsigned;
		}
		if (Desc.IsNumericIntegralType())
		{
			return EOrderKind::Signed;
		}
		return EOrderKind::None;
	}

	bool IsNumericKind(const EOrderKind Kind)
	{
		return Kind == EOrderKind::Signed || Kind == EOrderKind::Unsigned || Kind == EOrderKind::Floating;
	}

	bool IsEqualityComparison(const EGameplayResourceComparison Comparison)
	{
		return Comparison == EGameplayResourceComparison::Equal || Comparison == EGameplayResourceComparison::NotEqual;
	}

	template<typename T>
	int32 ThreeWay(const T A, const T B)
	{
		return A < B ? -1 : (B < A ? 1 : 0);
	}

	int32 Sign(const int32 Value)
	{
		return Value < 0 ? -1 : (Value > 0 ? 1 : 0);
	}

	// Orders A against B. Unset when either side is not orderable, the kinds cannot be compared with each
	// other, a read fails, or a float is NaN.
	TOptional<int32> CompareOrdered(const FInstancedPropertyBag& BagA, const FPropertyBagPropertyDesc& DescA, const FInstancedPropertyBag& BagB, const FPropertyBagPropertyDesc& DescB)
	{
		const EOrderKind KindA = GetOrderKind(DescA);
		const EOrderKind KindB = GetOrderKind(DescB);

		if (IsNumericKind(KindA) && IsNumericKind(KindB))
		{
			if (KindA == EOrderKind::Floating || KindB == EOrderKind::Floating)
			{
				const TValueOrError<double, EPropertyBagResult> A = BagA.GetValueDouble(DescA);
				const TValueOrError<double, EPropertyBagResult> B = BagB.GetValueDouble(DescB);
				if (!A.HasValue() || !B.HasValue() || FMath::IsNaN(A.GetValue()) || FMath::IsNaN(B.GetValue()))
				{
					return {};
				}
				return ThreeWay(A.GetValue(), B.GetValue());
			}

			// Integers stay out of double: past 2^53 it would call distinct values equal.
			if (KindA == EOrderKind::Unsigned && KindB == EOrderKind::Unsigned)
			{
				const TValueOrError<uint64, EPropertyBagResult> A = BagA.GetValueUInt64(DescA);
				const TValueOrError<uint64, EPropertyBagResult> B = BagB.GetValueUInt64(DescB);
				return (A.HasValue() && B.HasValue()) ? TOptional<int32>(ThreeWay(A.GetValue(), B.GetValue())) : TOptional<int32>();
			}

			if (KindA == EOrderKind::Signed && KindB == EOrderKind::Signed)
			{
				const TValueOrError<int64, EPropertyBagResult> A = BagA.GetValueInt64(DescA);
				const TValueOrError<int64, EPropertyBagResult> B = BagB.GetValueInt64(DescB);
				return (A.HasValue() && B.HasValue()) ? TOptional<int32>(ThreeWay(A.GetValue(), B.GetValue())) : TOptional<int32>();
			}

			// One signed, one unsigned: a negative signed value is below every unsigned one, otherwise both fit in uint64.
			const bool bASigned = KindA == EOrderKind::Signed;
			const TValueOrError<int64, EPropertyBagResult> SignedValue = bASigned ? BagA.GetValueInt64(DescA) : BagB.GetValueInt64(DescB);
			const TValueOrError<uint64, EPropertyBagResult> UnsignedValue = bASigned ? BagB.GetValueUInt64(DescB) : BagA.GetValueUInt64(DescA);
			if (!SignedValue.HasValue() || !UnsignedValue.HasValue())
			{
				return {};
			}
			const int32 SignedVsUnsigned = SignedValue.GetValue() < 0 ? -1 : ThreeWay(static_cast<uint64>(SignedValue.GetValue()), UnsignedValue.GetValue());
			return bASigned ? SignedVsUnsigned : -SignedVsUnsigned;
		}

		if (KindA != KindB)
		{
			return {};
		}

		switch (KindA)
		{
		case EOrderKind::String:
			{
				const TValueOrError<FString, EPropertyBagResult> A = BagA.GetValueString(DescA);
				const TValueOrError<FString, EPropertyBagResult> B = BagB.GetValueString(DescB);
				return (A.HasValue() && B.HasValue()) ? TOptional<int32>(Sign(A.GetValue().Compare(B.GetValue(), ESearchCase::CaseSensitive))) : TOptional<int32>();
			}
		case EOrderKind::Name:
			{
				const TValueOrError<FName, EPropertyBagResult> A = BagA.GetValueName(DescA);
				const TValueOrError<FName, EPropertyBagResult> B = BagB.GetValueName(DescB);
				return (A.HasValue() && B.HasValue()) ? TOptional<int32>(Sign(A.GetValue().Compare(B.GetValue()))) : TOptional<int32>();
			}
		case EOrderKind::Text:
			{
				const TValueOrError<FText, EPropertyBagResult> A = BagA.GetValueText(DescA);
				const TValueOrError<FText, EPropertyBagResult> B = BagB.GetValueText(DescB);
				return (A.HasValue() && B.HasValue()) ? TOptional<int32>(Sign(A.GetValue().CompareTo(B.GetValue()))) : TOptional<int32>();
			}
		default:
			return {};
		}
	}

	bool ApplyComparison(const EGameplayResourceComparison Comparison, const int32 Order)
	{
		switch (Comparison)
		{
		case EGameplayResourceComparison::Equal:			return Order == 0;
		case EGameplayResourceComparison::NotEqual:			return Order != 0;
		case EGameplayResourceComparison::Less:				return Order < 0;
		case EGameplayResourceComparison::LessOrEqual:		return Order <= 0;
		case EGameplayResourceComparison::Greater:			return Order > 0;
		case EGameplayResourceComparison::GreaterOrEqual:	return Order >= 0;
		}
		checkNoEntry();
		return false;
	}
}

void FGameplayResourceValue::SyncValueToSchema()
{
	const UPropertyBag* Schema = UGameplayResourcesDeveloperSettings::FindResourceSchema(Key);
	const FPropertyBagPropertyDesc* SchemaDesc = Schema ? Schema->FindPropertyDescByName(ValueName) : nullptr;

	if (SchemaDesc == nullptr)
	{
		Value.Reset();
		return;
	}

	// Copy the schema's desc verbatim. Its ID is hashed into the bag layout, so every FGameplayResourceValue
	// naming the same schema property resolves to one shared, cached UPropertyBag rather than a new transient
	// struct per instance - and the ID is what CopyMatchingValuesByID matches on at both ends.
	const FPropertyBagPropertyDesc SingleDesc = *SchemaDesc;
	Value.MigrateToNewBagStruct(UPropertyBag::GetOrCreateFromDescs(MakeArrayView(&SingleDesc, 1)));
}

const FPropertyBagPropertyDesc* FGameplayResourceValue::FindValueDesc() const
{
	return Value.FindPropertyDescByName(ValueName);
}

bool FGameplayResourceValue::IsValid() const
{
	return FindValueDesc() != nullptr;
}

bool FGameplayResourceValue::ImportFromSubsystem(const UObject* WorldContext)
{
	const UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext);
	if (GRS == nullptr)
	{
		return false;
	}

	const FInstancedPropertyBag* Runtime = GRS->FindRuntimeBag(Key);
	if (Runtime == nullptr)
	{
		return false;
	}

	SyncValueToSchema();

	const FPropertyBagPropertyDesc* Desc = FindValueDesc();
	if (Desc == nullptr)
	{
		return false;
	}

	// The runtime bag can predate the current schema, so report whether it actually carries this property
	// rather than whether the copy ran.
	const UPropertyBag* RuntimeStruct = Runtime->GetPropertyBagStruct();
	if (RuntimeStruct == nullptr || RuntimeStruct->FindPropertyDescByID(Desc->ID) == nullptr)
	{
		return false;
	}

	Value.CopyMatchingValuesByID(*Runtime);
	return true;
}

bool FGameplayResourceValue::ExportToSubsystem(const UObject* WorldContext) const
{
	if (UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext))
	{
		// SetResourceBag copies matching properties by ID, so a one-property bag is a one-property write.
		GRS->SetResourceBag(Key, Value);
		return true;
	}
	return false;
}

bool FGameplayResourceValue::HasEqualValue(const FInstancedPropertyBag& OtherBag) const
{
	const FPropertyBagPropertyDesc* Desc = FindValueDesc();
	if (Desc == nullptr)
	{
		return false;
	}

	const UPropertyBag* OtherStruct = OtherBag.GetPropertyBagStruct();
	const FPropertyBagPropertyDesc* OtherDesc = OtherStruct ? OtherStruct->FindPropertyDescByID(Desc->ID) : nullptr;
	if (OtherDesc == nullptr || !UE::GameplayResources::ArePropertiesLayoutCompatible(Desc->CachedProperty, OtherDesc->CachedProperty))
	{
		return false;
	}

	// Layout-compatible, so this property can read both values.
	const FProperty* Property = Desc->CachedProperty;
	return Property->Identical(
		Property->ContainerPtrToValuePtr<void>(Value.GetValue().GetMemory()),
		OtherDesc->CachedProperty->ContainerPtrToValuePtr<void>(OtherBag.GetValue().GetMemory()),
		PPF_None);
}

bool FGameplayResourceValue::SupportsComparison(const FPropertyBagPropertyDesc& Desc, const EGameplayResourceComparison Comparison)
{
	using namespace UE::GameplayResources::Private;
	return IsEqualityComparison(Comparison) || GetOrderKind(Desc) != EOrderKind::None;
}

bool FGameplayResourceValue::SupportsComparison(const EGameplayResourceComparison Comparison) const
{
	const FPropertyBagPropertyDesc* Desc = FindValueDesc();
	return Desc != nullptr && SupportsComparison(*Desc, Comparison);
}

TValueOrError<bool, EGameplayResourceComparisonError> FGameplayResourceValue::CompareWithSubsystem(const UObject* WorldContext, const EGameplayResourceComparison Comparison) const
{
	const UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext);
	if (GRS == nullptr)
	{
		return MakeError(EGameplayResourceComparisonError::NoSubsystem);
	}

	const FInstancedPropertyBag* Current = GRS->FindRuntimeBag(Key);
	if (Current == nullptr)
	{
		return MakeError(EGameplayResourceComparisonError::KeyUnset);
	}

	return CompareWithBag(*Current, Comparison);
}

TValueOrError<bool, EGameplayResourceComparisonError> FGameplayResourceValue::CompareWithBag(const FInstancedPropertyBag& CurrentBag, const EGameplayResourceComparison Comparison) const
{
	using namespace UE::GameplayResources::Private;

	const FPropertyBagPropertyDesc* ValueDesc = FindValueDesc();
	if (ValueDesc == nullptr)
	{
		return MakeError(EGameplayResourceComparisonError::InvalidValue);
	}

	const FPropertyBagPropertyDesc* CurrentDesc = CurrentBag.FindPropertyDescByID(ValueDesc->ID);
	if (CurrentDesc == nullptr)
	{
		return MakeError(EGameplayResourceComparisonError::PropertyMissing);
	}

	// Current on the left, so a condition reads the way it is authored: "Health >= 50".
	if (const TOptional<int32> Order = CompareOrdered(CurrentBag, *CurrentDesc, Value, *ValueDesc))
	{
		return MakeValue(ApplyComparison(Comparison, Order.GetValue()));
	}

	// Not orderable (bool, struct, object, container, NaN...): only equality is left, by the property's own Identical.
	if (!IsEqualityComparison(Comparison)
		|| !UE::GameplayResources::ArePropertiesLayoutCompatible(CurrentDesc->CachedProperty, ValueDesc->CachedProperty))
	{
		return MakeError(EGameplayResourceComparisonError::Incomparable);
	}

	const FProperty* Property = ValueDesc->CachedProperty;
	const bool bEqual = Property->Identical(
		CurrentDesc->CachedProperty->ContainerPtrToValuePtr<void>(CurrentBag.GetValue().GetMemory()),
		Property->ContainerPtrToValuePtr<void>(Value.GetValue().GetMemory()),
		PPF_None);

	return MakeValue(Comparison == EGameplayResourceComparison::Equal ? bEqual : !bEqual);
}

bool FGameplayResourceValue::operator==(const FGameplayResourceValue& Other) const
{
	if (Key != Other.Key || ValueName != Other.ValueName)
	{
		return false;
	}

	// Two unresolved values naming the same thing are equal; neither holds a value to disagree about.
	if (!IsValid() && !Other.IsValid())
	{
		return true;
	}

	return HasEqualValue(Other.Value);
}
