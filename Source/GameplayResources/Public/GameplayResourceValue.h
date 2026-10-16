
#pragma once

#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"
#include "Templates/ValueOrError.h"
#include "GameplayResourceValue.generated.h"

/** Comparison applied as `Current <op> Value`: the subsystem's runtime value on the left, the authored value on the right. */
UENUM(BlueprintType)
enum class EGameplayResourceComparison : uint8
{
	Equal			UMETA(DisplayName = "=="),
	NotEqual		UMETA(DisplayName = "!="),
	Less			UMETA(DisplayName = "<"),
	LessOrEqual		UMETA(DisplayName = "<="),
	Greater			UMETA(DisplayName = ">"),
	GreaterOrEqual	UMETA(DisplayName = ">="),
};

/** Why a comparison could not produce an answer. Distinct from the comparison simply being false. */
UENUM(BlueprintType)
enum class EGameplayResourceComparisonError : uint8
{
	// No subsystem reachable from the world context (e.g. editor preview).
	NoSubsystem,
	// The key currently has no runtime value.
	KeyUnset,
	// The runtime bag has no property with this value's ID (it predates the schema change that added it).
	PropertyMissing,
	// This struct's Value does not hold the property named by ValueName; call SyncValueToSchema.
	InvalidValue,
	// The two values' types cannot be compared with the requested operation.
	Incomparable,
};

/**
 * A self-contained, editable copy of a single property of a gameplay resource.
 *
 * The narrow counterpart to FGameplayResource: instead of mirroring a key's whole schema, this holds one
 * named property of it. Key picks the resource, ValueName picks the property inside that key's schema, and
 * Value is a property bag holding exactly that one property.
 *
 * Value keeps the schema's own property desc, ID included, which is what makes the single-property bag
 * worth its cost: CopyMatchingValuesByID matches it against the key's full bag in both directions, so
 * ExportToSubsystem writes this one property and leaves the key's other properties untouched.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYRESOURCES_API FGameplayResourceValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay Resource", meta = (GetOptions = "GameplayResources.GameplayResourcesDeveloperSettings.GetKeys"))
	FName Key;

	// The property of Key's schema this struct mirrors. The dropdown comes from the details customization,
	// not from GetOptions: GetOptions resolves to a static no-argument function, which cannot see the
	// sibling Key and so cannot narrow the list to one schema.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay Resource")
	FName ValueName;

	// Holds exactly one property: a copy of Key's schema desc for ValueName. Editable in the details panel,
	// which renders the schema's own widget for the property's type.
	UPROPERTY(EditAnywhere, Category = "Gameplay Resource", meta = (FixedLayout))
	FInstancedPropertyBag Value;

	// Re-shapes Value to the single schema property named by ValueName, preserving the current value when the
	// layout is unchanged. Clears it when Key or ValueName names nothing in the DataTable.
	void SyncValueToSchema();

	// The desc for ValueName inside Value, or null when Value is empty or out of sync with ValueName.
	const FPropertyBagPropertyDesc* FindValueDesc() const;

	// True when Value currently holds the property named by ValueName.
	bool IsValid() const;

	// Copies the current runtime value of this one property from the subsystem, re-shaping Value to the
	// schema first. False when the subsystem is unavailable, the key is unset, or the runtime bag has no
	// property with this ID.
	bool ImportFromSubsystem(const UObject* WorldContext);

	// Writes this one property into the subsystem's bag for Key. The key's other properties keep their
	// values; as with any other write, the subsystem broadcasts Changed (or Added when it had to create the
	// bag from the schema). False when the subsystem is unavailable.
	bool ExportToSubsystem(const UObject* WorldContext) const;

	// True when OtherBag holds a property with the same ID and an equal value. Matching by ID, the same way
	// writes copy, so a bag built against an older schema still compares by value.
	bool HasEqualValue(const FInstancedPropertyBag& OtherBag) const;

	// Whether Comparison can be applied to a property of Desc's type. Equal/NotEqual work on every type.
	// Ordering works on numbers (ints, floats, enums by underlying value), String (case-sensitive), Name
	// (lexical, case-insensitive) and Text (culture-aware); not on bool, struct, object, class, soft path or
	// containers. Meant for editor-time validation of conditions built on this struct.
	static bool SupportsComparison(const FPropertyBagPropertyDesc& Desc, EGameplayResourceComparison Comparison);

	// SupportsComparison for this struct's own property. False when Value is not in sync with ValueName.
	bool SupportsComparison(EGameplayResourceComparison Comparison) const;

	// Evaluates `Current <Comparison> Value`, where Current is Key's runtime value in the subsystem. Returns
	// an error rather than false when there is nothing to compare or the types do not support the operation,
	// so callers can tell "condition failed" from "condition cannot be evaluated".
	TValueOrError<bool, EGameplayResourceComparisonError> CompareWithSubsystem(const UObject* WorldContext, EGameplayResourceComparison Comparison) const;

	// Same as CompareWithSubsystem against an arbitrary bag, e.g. the snapshot handed to a change listener.
	// The property is found in CurrentBag by ID. Numbers of different types compare by value (int vs float
	// after a schema change still works); NaN is unordered, so only Equal/NotEqual can answer for it.
	TValueOrError<bool, EGameplayResourceComparisonError> CompareWithBag(const FInstancedPropertyBag& CurrentBag, EGameplayResourceComparison Comparison) const;

	// Same Key, same ValueName and HasEqualValue. Deliberately looser than reflection's Identical (no
	// WithIdenticalViaEquality trait), for the same reason as FGameplayResource: serialization's delta
	// checks need the strict, bag-struct-aware comparison.
	bool operator==(const FGameplayResourceValue& Other) const;
};
