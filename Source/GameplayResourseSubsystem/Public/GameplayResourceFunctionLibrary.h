// © Volga Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayResource.h"
#include "GameplayResourceFunctionLibrary.generated.h"

/**
 * Blueprint entry points for working with an FGameplayResource copy: importing/exporting it against the
 * subsystem, and the per-property read/write thunks the Get/Set Gameplay Resource nodes expand into.
 */
UCLASS()
class GAMEPLAYRESOURCES_API UGameplayResourceFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// Copies the current runtime value for Resource.Key from the subsystem into Resource.Bag.
	UFUNCTION(BlueprintCallable, Category = "Gameplay Resource", meta = (DefaultToSelf = "WorldContext"))
	static bool ImportGameplayResource(const UObject* WorldContext, UPARAM(ref) FGameplayResource& Resource);

	// Pushes Resource.Bag values into the subsystem for Resource.Key.
	UFUNCTION(BlueprintCallable, Category = "Gameplay Resource", meta = (DefaultToSelf = "WorldContext"))
	static bool ExportGameplayResource(const UObject* WorldContext, const FGameplayResource& Resource);

	// Reads a single named property from Resource.Bag into the wildcard Value pin.
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = "Value", BlueprintInternalUseOnly = "true"))
	static void BPGetResourceProperty(const FGameplayResource& Resource, FName PropertyName, int32& Value);

	DECLARE_FUNCTION(execBPGetResourceProperty);

	// Writes the wildcard Value pin into a single named property of Resource.Bag.
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = "Value", BlueprintInternalUseOnly = "true"))
	static void BPSetResourceProperty(UPARAM(ref) FGameplayResource& Resource, FName PropertyName, const int32& Value);

	DECLARE_FUNCTION(execBPSetResourceProperty);
};
