#pragma once

#include "CoreMinimal.h"
#include "GameplayResource.h"
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

// Kinds of change a resource listener can subscribe to. Combine as a mask when registering;
// a broadcast always carries exactly one flag.
enum class EGameplayResourceChangeType : uint8
{
	None    = 0,
	Added   = 1 << 0, // Key went from unset to set. Resource is the new value.
	Changed = 1 << 1, // Key was already set and was written again. Resource is the new value.
	Removed = 1 << 2, // Key went from set to unset. Resource is the value that was removed.
	All     = Added | Changed | Removed
};
ENUM_CLASS_FLAGS(EGameplayResourceChangeType)

// Resource is a snapshot owned by the broadcast and is only valid for the duration of the call.
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnGameplayResourceChanged, FName /*Key*/, EGameplayResourceChangeType /*ChangeType*/, const FInstancedPropertyBag& /*Resource*/);

// Single-cast callback passed to UGameplayResourceSubsystem::RegisterResourceChangeListener.
using FGameplayResourceChangeListener = FOnGameplayResourceChanged::FDelegate;


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

	// Removes a key's runtime value and leaves it unset, even when the row is a default.
	void ClearValue(FName Key);

	// True when Resource.Key is currently set and its runtime value matches Resource.Bag. Properties are matched
	// by ID, the same way SetResourceBag copies them, so a bag built against an older schema still compares by
	// value. False when the key is unset, or when either side has a property the other lacks or stores as a
	// different type.
	bool IsResourceEqual(const FGameplayResource& Resource) const;

	// BlueprintPure accessor used by the Get node's "Found" output pin.
	UFUNCTION(BlueprintPure, meta = (DefaultToSelf = "WorldContext"))
	static bool HasGlobalResource(const UObject* WorldContext, UPARAM(meta = (GetOptions = "GameplayResources.GameplayResourcesDeveloperSettings.GetKeys")) FName Key);

	UFUNCTION(BlueprintCallable, meta = (DefaultToSelf = "WorldContext", WorldContext = "WorldContext"))
	static void ResetGlobalResource(const UObject* WorldContext, UPARAM(meta = (GetOptions = "GameplayResources.GameplayResourcesDeveloperSettings.GetKeys")) FName Key);

	UFUNCTION(BlueprintPure, meta = (DefaultToSelf = "WorldContext"))
	static bool IsGlobalResourceEqual(const UObject* WorldContext, const FGameplayResource& Resource);

	UFUNCTION(BlueprintCallable, meta = (DefaultToSelf = "WorldContext"))
	static void ClearGlobalResource(const UObject* WorldContext, UPARAM(meta = (GetOptions = "GameplayResources.GameplayResourcesDeveloperSettings.GetKeys")) FName Key);

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

	// Subscribes Listener to the change types set in ChangeMask, for Key only, or for every key when Key is
	// NAME_None. Returns an invalid handle when the subsystem can't be resolved from WorldContext, the mask is empty,
	// or Key is not a row in the resources DataTable (which also raises an ensure).
	static FDelegateHandle RegisterResourceChangeListener(const UObject* WorldContext, FName Key, EGameplayResourceChangeType ChangeMask, const FGameplayResourceChangeListener& Listener);

	// Removes a listener previously returned by RegisterResourceChangeListener, whatever its key and mask were.
	static void UnregisterResourceChangeListener(const UObject* WorldContext, FDelegateHandle Handle);

	// Removes every listener bound to UserObject, across all keys.
	static void UnregisterAllResourceChangeListeners(const UObject* WorldContext, FDelegateUserObjectConst UserObject);

	FDelegateHandle AddResourceChangeListener(FName Key, EGameplayResourceChangeType ChangeMask, const FGameplayResourceChangeListener& Listener);
	void RemoveResourceChangeListener(FDelegateHandle Handle);
	void RemoveAllResourceChangeListeners(FDelegateUserObjectConst UserObject);

private:

	static constexpr int32 NumChangeTypes = 3;

	// One invocation list per change type, indexed by the flag's bit position. A listener registered for
	// several types is added to each list as a copy of the same delegate, so all copies share one handle.
	struct FResourceChangeListeners
	{
		FDelegateHandle Add(EGameplayResourceChangeType ChangeMask, const FGameplayResourceChangeListener& Listener);
		bool Remove(FDelegateHandle Handle);
		void RemoveAll(FDelegateUserObjectConst UserObject);
		bool IsBound() const;

		FOnGameplayResourceChanged ByChangeType[NumChangeTypes];
	};

	// Ensures a runtime bag exists for Key, creating a default-initialized one from the schema.
	// bOutCreated reports whether the bag was created by this call. Returns nullptr if the key has no declared schema.
	FInstancedPropertyBag* FindOrCreateRuntimeBag(FName Key, bool& bOutCreated);

	void BroadcastResourceChange(FName Key, EGameplayResourceChangeType ChangeType, const FInstancedPropertyBag& Resource);

	// Listeners registered with NAME_None.
	FResourceChangeListeners AnyKeyListeners;

	// Per-key listeners. Held by shared reference so a broadcast can keep its set alive and at a stable address
	// while a listener registers for another key (reallocating the map) or unregisters itself (pruning the set).
	TMap<FName, TSharedRef<FResourceChangeListeners>> KeyListeners;

	TMap<FName, FInstancedPropertyBag> Values;
};
