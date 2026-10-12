
#pragma once

#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"
#include "GameplayResource.generated.h"

/**
 * A self-contained, editable copy of a gameplay resource.
 *
 * Holds a Key (restricted to the keys declared in the Global Resources DataTable) and a property bag
 * whose layout mirrors the schema declared for that Key.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYRESOURCES_API FGameplayResource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay Resource", meta = (GetOptions = "GameplayResources.GameplayResourcesDeveloperSettings.GetKeys"))
	FName Key;

	// Editable in the details panel; Blueprint access goes through the Get/Set Gameplay Resource nodes.
	UPROPERTY(EditAnywhere, Category = "Gameplay Resource", meta = (FixedLayout))
	FInstancedPropertyBag Bag;

	// Re-shapes Bag to match the schema declared for Key in the DataTable, preserving matching values.
	// Clears the bag when Key has no declared schema.
	void SyncBagToSchema();

	// Copies the current runtime value for Key from the subsystem into this struct's Bag.
	// Returns false when the subsystem is unavailable or the key is currently unset.
	bool ImportFromSubsystem(const UObject* WorldContext);

	// Pushes this struct's Bag values into the subsystem for Key. Returns false when unavailable.
	bool ExportToSubsystem(const UObject* WorldContext) const;

	// True when Bag and OtherBag hold the same values. Properties are matched by ID, the same way
	// SetResourceBag copies them, so a bag built against an older schema still compares by value. False when
	// either side has a property the other lacks or stores as a different type.
	bool HasEqualValues(const FInstancedPropertyBag& OtherBag) const;

	// Same Key and HasEqualValues. Deliberately looser than reflection's Identical (no WithIdenticalViaEquality
	// trait): that one also requires the same bag struct, and serialization relies on it for delta checks.
	bool operator==(const FGameplayResource& Other) const;
};
