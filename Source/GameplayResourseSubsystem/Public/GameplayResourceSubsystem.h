#pragma once

#include "CoreMinimal.h"
#include "GameplayResourcesDeveloperSettings.h"
#include "UObject/UnrealType.h"
#include "StructUtils/PropertyBag.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayResourceSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FGlobalResourceReference
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (GetOptions = "GameplayResources.GameplayResourcesDeveloperSettings.GetKeys"))
	FName Key;
};


UCLASS()
class GAMEPLAYRESOURCES_API UGameplayResourceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	
	static UGameplayResourceSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// True when the key currently holds a runtime value (either seeded as default or set at runtime).
	bool HasResource(FName Key) const;

	// Runtime bag for a key, or nullptr when the resource is currently unset.
	const FInstancedPropertyBag* FindRuntimeBag(FName Key) const;

	// Sets a key's runtime value from an external bag, copying matching properties onto the schema.
	void SetResourceBag(FName Key, const FInstancedPropertyBag& InBag);

	// Removes a key's runtime value (back to "unset"). Re-seeds the default if the row is a default.
	void ResetValue(FName Key);

	// BlueprintPure accessor used by the Get node's "Found" output pin.
	UFUNCTION(BlueprintPure, meta = (DefaultToSelf = "WorldContext"))
	static bool HasGlobalResource(const UObject* WorldContext, UPARAM(meta = (GetOptions = "GameplayResources.GameplayResourcesDeveloperSettings.GetKeys")) FName Key);

	UFUNCTION(BlueprintCallable, meta = (DefaultToSelf = "WorldContext", WorldContex = "WorldContext"))
	static void ResetGlobalResource(const UObject* WorldContext, UPARAM(meta = (GetOptions = "GameplayResources.GameplayResourcesDeveloperSettings.GetKeys")) FName Key);

	// Reads a single named property out of a resource key's runtime bag into the wildcard Value pin.
	// Value is left zero-initialized when the key is unset or the property is missing.
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = "Value", BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContext"))
	static void BPGetProperty(const UObject* WorldContext, FName Key, FName PropertyName, int32& Value);

	DECLARE_FUNCTION(execBPGetProperty);

	// Writes the wildcard Value pin into a single named property of a resource key's runtime bag,
	// creating the runtime bag from the key's schema (default-initialized) if it did not exist yet.
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = "Value", BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContext"))
	static void BPSetProperty(const UObject* WorldContext, FName Key, FName PropertyName, const int32& Value);

	DECLARE_FUNCTION(execBPSetProperty);

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnValueUpdated, FName)

	FOnValueUpdated OnValueUpdated;

private:

	// Ensures a runtime bag exists for Key, creating a default-initialized one from the schema.
	// Returns nullptr if the key has no declared schema.
	FInstancedPropertyBag* FindOrCreateRuntimeBag(FName Key);

	TMap<FName, FInstancedPropertyBag> Values;
};
