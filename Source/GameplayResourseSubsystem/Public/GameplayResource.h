// © Volga Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"
#include "GameplayResource.generated.h"

/**
 * A self-contained, editable copy of a gameplay resource.
 *
 * Holds a Key (restricted to the keys declared in the Global Resources DataTable) and a property bag
 * whose layout mirrors the schema declared for that Key. The bag's values are freely editable, and the
 * bag re-shapes itself to the declared schema whenever the Key changes in the editor.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYRESOURCES_API FGameplayResource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay Resource", meta = (GetOptions = "GameplayResources.GameplayResourcesDeveloperSettings.GetKeys"))
	FName Key;

	// Editable in the details panel; not directly Blueprint-exposed (property bags aren't a BP-native
	// type). Blueprint access goes through the Get/Set Gameplay Resource nodes.
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
};
