#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "GameplayResource.h"
#include "GameplayResourceSubsystem.h"
#include "AsyncAction_ListenForGlobalResourceChange.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameplayResourceChangedDynamic, EGameplayResourceChangeType, ChangeType, const FGameplayResource&, Resource);

/**
 * Blueprint-facing bridge to UGameplayResourceSubsystem's native change listeners. Not placed directly
 * (HasDedicatedAsyncNode): the "On Global Resource Changed" K2 node expands into a UK2Node_AsyncAction for this
 * class and adds the per-property pins on top.
 *
 * Lifetime: deliberately not registered with the game instance. UBlueprintAsyncActionBase sets
 * RF_StrongRefOnFrame, so the Blueprint instance that ran the node keeps this action alive through its ubergraph
 * frame; when that instance goes away the action is collected and BeginDestroy unregisters the listener.
 * Cancel() (also callable from Blueprint through the exposed Async Action pin) stops it early, and running the
 * node again cancels the previous action.
 */
UCLASS(meta = (HasDedicatedAsyncNode))
class GAMEPLAYRESOURCES_API UAsyncAction_ListenForGlobalResourceChange : public UCancellableAsyncAction
{
	GENERATED_BODY()

public:

	// Prepares a listener for Key (every key when Key is None) on the change types in ChangeMask; Activate starts
	// it. Cancels PreviousListener first.
	UFUNCTION(BlueprintCallable, Category = "Gameplay Resource|Async", meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContext"))
	static UAsyncAction_ListenForGlobalResourceChange* ListenForGlobalResourceChange(
		UObject* WorldContext,
		FName Key,
		UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/GameplayResources.EGameplayResourceChangeType")) int32 ChangeMask,
		UAsyncAction_ListenForGlobalResourceChange* PreviousListener);

	UPROPERTY(BlueprintAssignable)
	FOnGameplayResourceChangedDynamic OnResourceChanged;

	//~ Begin UCancellableAsyncAction Interface
	virtual void Activate() override;
	virtual void Cancel() override;
	virtual bool ShouldBroadcastDelegates() const override;
	//~ End UCancellableAsyncAction Interface

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

private:

	void HandleResourceChanged(FName ChangedKey, EGameplayResourceChangeType ChangeType, const FInstancedPropertyBag& Resource);

	// Removes the native listener, if any. Leaves the Blueprint bindings alone.
	void StopListening();

	TWeakObjectPtr<UObject> WorldContext;

	FName Key;

	EGameplayResourceChangeType ChangeMask = EGameplayResourceChangeType::None;

	TWeakObjectPtr<UGameplayResourceSubsystem> Subsystem;

	FDelegateHandle ListenerHandle;
};
