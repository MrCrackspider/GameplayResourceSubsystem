#include "Async/AsyncAction_ListenForGlobalResourceChange.h"

UAsyncAction_ListenForGlobalResourceChange* UAsyncAction_ListenForGlobalResourceChange::ListenForGlobalResourceChange(UObject* WorldContext, FName Key, int32 ChangeMask, UAsyncAction_ListenForGlobalResourceChange* PreviousListener)
{
	if (PreviousListener != nullptr)
	{
		PreviousListener->Cancel();
	}

	UAsyncAction_ListenForGlobalResourceChange* Action = NewObject<UAsyncAction_ListenForGlobalResourceChange>();
	Action->WorldContext = WorldContext;
	Action->Key = Key;
	Action->ChangeMask = static_cast<EGameplayResourceChangeType>(ChangeMask) & EGameplayResourceChangeType::All;
	return Action;
}

void UAsyncAction_ListenForGlobalResourceChange::Activate()
{
	Super::Activate();

	// No subsystem (e.g. an editor preview world): the action simply never fires.
	if (UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext.Get()))
	{
		Subsystem = GRS;
		ListenerHandle = GRS->AddResourceChangeListener(Key, ChangeMask,
			FGameplayResourceChangeListener::CreateUObject(this, &UAsyncAction_ListenForGlobalResourceChange::HandleResourceChanged));
	}
}

void UAsyncAction_ListenForGlobalResourceChange::Cancel()
{
	StopListening();
	OnResourceChanged.Clear();

	// Clears RF_StrongRefOnFrame, so the ubergraph frame no longer keeps this action alive.
	Super::Cancel();
}

bool UAsyncAction_ListenForGlobalResourceChange::ShouldBroadcastDelegates() const
{
	// The base answers "registered with a game instance", which this action never is (see class comment).
	return ListenerHandle.IsValid();
}

void UAsyncAction_ListenForGlobalResourceChange::BeginDestroy()
{
	StopListening();

	Super::BeginDestroy();
}

void UAsyncAction_ListenForGlobalResourceChange::StopListening()
{
	if (UGameplayResourceSubsystem* GRS = Subsystem.Get())
	{
		GRS->RemoveResourceChangeListener(ListenerHandle);
	}

	Subsystem.Reset();
	ListenerHandle.Reset();
}

void UAsyncAction_ListenForGlobalResourceChange::HandleResourceChanged(FName ChangedKey, EGameplayResourceChangeType ChangeType, const FInstancedPropertyBag& Resource)
{
	if (!ShouldBroadcastDelegates())
	{
		return;
	}

	FGameplayResource Snapshot;
	Snapshot.Key = ChangedKey;
	Snapshot.Bag = Resource;

	OnResourceChanged.Broadcast(ChangeType, Snapshot);
}
