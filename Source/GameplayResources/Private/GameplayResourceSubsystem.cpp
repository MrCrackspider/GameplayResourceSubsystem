#include "GameplayResourceSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayResourceWildcardValue.h"

using UE::GameplayResources::ArePropertiesLayoutCompatible;
using UE::GameplayResources::UnresolvedLiteralStorageSize;

namespace
{
	// Compares two bags value by value, matching properties by ID rather than by layout. Both bags must hold
	// exactly the same set of properties, each with a layout-compatible type.
	bool AreBagValuesIdentical(const FInstancedPropertyBag& BagA, const FInstancedPropertyBag& BagB)
	{
		const UPropertyBag* StructA = BagA.GetPropertyBagStruct();
		const UPropertyBag* StructB = BagB.GetPropertyBagStruct();

		// Same layout, including both empty: compare the whole struct in one go.
		if (StructA == StructB)
		{
			return BagA.Identical(&BagB, PPF_None);
		}

		if (StructA == nullptr || StructB == nullptr || BagA.GetNumPropertiesInBag() != BagB.GetNumPropertiesInBag())
		{
			return false;
		}

		const uint8* BaseA = BagA.GetValue().GetMemory();
		const uint8* BaseB = BagB.GetValue().GetMemory();

		// IDs are unique within a bag, so equal counts plus every property of A found in B means the sets match.
		for (const FPropertyBagPropertyDesc& DescA : StructA->GetPropertyDescs())
		{
			const FPropertyBagPropertyDesc* DescB = StructB->FindPropertyDescByID(DescA.ID);
			if (DescB == nullptr || !ArePropertiesLayoutCompatible(DescA.CachedProperty, DescB->CachedProperty))
			{
				return false;
			}

			// Layout-compatible, so A's property can read both values.
			const FProperty* Property = DescA.CachedProperty;
			if (!Property->Identical(Property->ContainerPtrToValuePtr<void>(BaseA), DescB->CachedProperty->ContainerPtrToValuePtr<void>(BaseB), PPF_None))
			{
				return false;
			}
		}

		return true;
	}
}

UGameplayResourceSubsystem* UGameplayResourceSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
	if (World == nullptr || World->GetGameInstance() == nullptr)
	{
		return nullptr;
	}

	return World->GetGameInstance()->GetSubsystem<UGameplayResourceSubsystem>();
}

void UGameplayResourceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (const UDataTable* DT = UGameplayResourcesDeveloperSettings::GetDataTable())
	{
		DT->ForeachRow<FGameplayResourceEntry>(TEXT("GameplayResourceSubsystem::Initialize"),
			[this](const FName& Key, const FGameplayResourceEntry& Row)
			{
				if (Row.bSetAsDefault && Row.Resources.IsValid())
				{
					Values.Add(Key, Row.Resources);
				}
			});
	}
}

bool UGameplayResourceSubsystem::HasResource(FName Key) const
{
	return Values.Contains(Key);
}

const FInstancedPropertyBag* UGameplayResourceSubsystem::FindRuntimeBag(FName Key) const
{
	return Values.Find(Key);
}

FInstancedPropertyBag* UGameplayResourceSubsystem::FindOrCreateRuntimeBag(FName Key, bool& bOutCreated)
{
	bOutCreated = false;

	if (FInstancedPropertyBag* Existing = Values.Find(Key))
	{
		return Existing;
	}

	// Create a default-initialized runtime bag from the declared schema. Authored default values are
	// intentionally not copied here (they only apply when the row is flagged bSetAsDefault at startup).
	const UPropertyBag* Schema = UGameplayResourcesDeveloperSettings::FindResourceSchema(Key);
	if (Schema == nullptr)
	{
		return nullptr;
	}

	FInstancedPropertyBag& NewBag = Values.Add(Key);
	NewBag.InitializeFromBagStruct(Schema);
	bOutCreated = true;
	return &NewBag;
}

void UGameplayResourceSubsystem::SetResourceBag(FName Key, const FInstancedPropertyBag& InBag)
{
	if (!Key.IsValid())
	{
		return;
	}

	bool bCreated = false;
	if (FInstancedPropertyBag* Bag = FindOrCreateRuntimeBag(Key, bCreated))
	{
		Bag->CopyMatchingValuesByID(InBag);
		BroadcastResourceChange(Key, bCreated ? EGameplayResourceChangeType::Added : EGameplayResourceChangeType::Changed, *Bag);
	}
}

void UGameplayResourceSubsystem::ResetValue(FName Key)
{
	if (!Key.IsValid())
	{
		return;
	}

	FInstancedPropertyBag RemovedBag;
	const bool bWasSet = Values.RemoveAndCopyValue(Key, RemovedBag);

	// Re-seed the default value if this key is flagged as a startup default.
	const FInstancedPropertyBag* ReseededBag = nullptr;
	if (const UDataTable* DT = UGameplayResourcesDeveloperSettings::GetDataTable())
	{
		if (const FGameplayResourceEntry* Row = DT->FindRow<FGameplayResourceEntry>(Key, TEXT("GameplayResourceSubsystem::ResetValue")))
		{
			if (Row->bSetAsDefault && Row->Resources.IsValid())
			{
				ReseededBag = &Values.Add(Key, Row->Resources);
			}
		}
	}

	// A reset that lands back on a default is a value change, not a removal: the key stays set.
	if (ReseededBag != nullptr)
	{
		BroadcastResourceChange(Key, bWasSet ? EGameplayResourceChangeType::Changed : EGameplayResourceChangeType::Added, *ReseededBag);
	}
	else if (bWasSet)
	{
		BroadcastResourceChange(Key, EGameplayResourceChangeType::Removed, RemovedBag);
	}
}

void UGameplayResourceSubsystem::ClearValue(FName Key)
{
	FInstancedPropertyBag RemovedBag;
	if (Values.RemoveAndCopyValue(Key, RemovedBag))
	{
		BroadcastResourceChange(Key, EGameplayResourceChangeType::Removed, RemovedBag);
	}
}

bool UGameplayResourceSubsystem::IsResourceEqual(const FGameplayResource& Resource) const
{
	const FInstancedPropertyBag* RuntimeBag = FindRuntimeBag(Resource.Key);
	return RuntimeBag != nullptr && AreBagValuesIdentical(Resource.Bag, *RuntimeBag);
}

FDelegateHandle UGameplayResourceSubsystem::RegisterResourceChangeListener(const UObject* WorldContext, FName Key, EGameplayResourceChangeType ChangeMask, const FGameplayResourceChangeListener& Listener)
{
	UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext);
	return GRS ? GRS->AddResourceChangeListener(Key, ChangeMask, Listener) : FDelegateHandle();
}

void UGameplayResourceSubsystem::UnregisterResourceChangeListener(const UObject* WorldContext, FDelegateHandle Handle)
{
	if (UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext))
	{
		GRS->RemoveResourceChangeListener(Handle);
	}
}

void UGameplayResourceSubsystem::UnregisterAllResourceChangeListeners(const UObject* WorldContext, FDelegateUserObjectConst UserObject)
{
	if (UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext))
	{
		GRS->RemoveAllResourceChangeListeners(UserObject);
	}
}

FDelegateHandle UGameplayResourceSubsystem::AddResourceChangeListener(FName Key, EGameplayResourceChangeType ChangeMask, const FGameplayResourceChangeListener& Listener)
{
	if (!ensureMsgf(Listener.IsBound(), TEXT("Registering an unbound gameplay resource change listener.")))
	{
		return FDelegateHandle();
	}

	if (Key.IsNone())
	{
		return AnyKeyListeners.Add(ChangeMask, Listener);
	}

	// Only keys declared in the resources DataTable can ever hold a value, so a listener on any other key
	// would never fire. Almost always a typo or a renamed row.
	if (!ensureMsgf(UGameplayResourcesDeveloperSettings::FindResourceSchema(Key) != nullptr,
		TEXT("Registering a gameplay resource change listener for unknown key '%s'."), *Key.ToString()))
	{
		return FDelegateHandle();
	}

	// Don't leave an empty set behind for a mask that subscribes to nothing.
	if (!EnumHasAnyFlags(ChangeMask, EGameplayResourceChangeType::All))
	{
		return FDelegateHandle();
	}

	TSharedRef<FResourceChangeListeners>* Listeners = KeyListeners.Find(Key);
	if (Listeners == nullptr)
	{
		Listeners = &KeyListeners.Add(Key, MakeShared<FResourceChangeListeners>());
	}

	return (*Listeners)->Add(ChangeMask, Listener);
}

void UGameplayResourceSubsystem::RemoveResourceChangeListener(FDelegateHandle Handle)
{
	if (!Handle.IsValid() || AnyKeyListeners.Remove(Handle))
	{
		return;
	}

	for (auto It = KeyListeners.CreateIterator(); It; ++It)
	{
		if (It->Value->Remove(Handle))
		{
			if (!It->Value->IsBound())
			{
				It.RemoveCurrent();
			}
			return;
		}
	}
}

void UGameplayResourceSubsystem::RemoveAllResourceChangeListeners(FDelegateUserObjectConst UserObject)
{
	AnyKeyListeners.RemoveAll(UserObject);

	for (auto It = KeyListeners.CreateIterator(); It; ++It)
	{
		It->Value->RemoveAll(UserObject);
		if (!It->Value->IsBound())
		{
			It.RemoveCurrent();
		}
	}
}

void UGameplayResourceSubsystem::BroadcastResourceChange(FName Key, EGameplayResourceChangeType ChangeType, const FInstancedPropertyBag& Resource)
{
	const uint32 ChangeBits = static_cast<uint32>(ChangeType);
	check(FMath::IsPowerOfTwo(ChangeBits) && ChangeBits < (1u << NumChangeTypes));
	const int32 Index = FMath::CountTrailingZeros(ChangeBits);

	// Held locally so the set outlives this broadcast even if one of its listeners unregisters and prunes it.
	TSharedPtr<FResourceChangeListeners> KeyedListeners;
	if (const TSharedRef<FResourceChangeListeners>* Found = KeyListeners.Find(Key))
	{
		KeyedListeners = *Found;
	}

	const bool bKeyedBound = KeyedListeners.IsValid() && KeyedListeners->ByChangeType[Index].IsBound();
	if (!bKeyedBound && !AnyKeyListeners.ByChangeType[Index].IsBound())
	{
		return;
	}

	// Resource usually points into Values. A listener that sets or resets another key can reallocate the
	// map mid-broadcast and leave later listeners with a dangling reference, so hand out a snapshot instead.
	const FInstancedPropertyBag Snapshot = Resource;

	if (bKeyedBound)
	{
		KeyedListeners->ByChangeType[Index].Broadcast(Key, ChangeType, Snapshot);
	}
	AnyKeyListeners.ByChangeType[Index].Broadcast(Key, ChangeType, Snapshot);
}

FDelegateHandle UGameplayResourceSubsystem::FResourceChangeListeners::Add(EGameplayResourceChangeType ChangeMask, const FGameplayResourceChangeListener& Listener)
{
	static_assert(static_cast<uint32>(EGameplayResourceChangeType::All) == (1u << NumChangeTypes) - 1u,
		"ByChangeType must have one entry per EGameplayResourceChangeType flag.");

	// Each list stores a copy of Listener; delegate copies keep the original's handle, so this one handle
	// identifies the listener in every list it was added to.
	FDelegateHandle Handle;
	for (int32 Index = 0; Index < NumChangeTypes; ++Index)
	{
		if (EnumHasAnyFlags(ChangeMask, static_cast<EGameplayResourceChangeType>(1 << Index)))
		{
			const FDelegateHandle AddedHandle = ByChangeType[Index].Add(Listener);
			check(!Handle.IsValid() || Handle == AddedHandle);
			Handle = AddedHandle;
		}
	}

	return Handle;
}

bool UGameplayResourceSubsystem::FResourceChangeListeners::Remove(FDelegateHandle Handle)
{
	bool bRemoved = false;
	for (FOnGameplayResourceChanged& Listeners : ByChangeType)
	{
		bRemoved |= Listeners.Remove(Handle);
	}
	return bRemoved;
}

void UGameplayResourceSubsystem::FResourceChangeListeners::RemoveAll(FDelegateUserObjectConst UserObject)
{
	for (FOnGameplayResourceChanged& Listeners : ByChangeType)
	{
		Listeners.RemoveAll(UserObject);
	}
}

bool UGameplayResourceSubsystem::FResourceChangeListeners::IsBound() const
{
	for (const FOnGameplayResourceChanged& Listeners : ByChangeType)
	{
		if (Listeners.IsBound())
		{
			return true;
		}
	}
	return false;
}

bool UGameplayResourceSubsystem::HasGlobalResource(const UObject* WorldContext, FName Key)
{
	const UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext);
	return GRS ? GRS->HasResource(Key) : false;
}

void UGameplayResourceSubsystem::ResetGlobalResource(const UObject* WorldContext, FName Key)
{
	if (UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext))
	{
		GRS->ResetValue(Key);
	}
}

bool UGameplayResourceSubsystem::IsGlobalResourceEqual(const UObject* WorldContext, const FGameplayResource& Resource)
{
	const UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext);
	return GRS ? GRS->IsResourceEqual(Resource) : false;
}

void UGameplayResourceSubsystem::ClearGlobalResource(const UObject* WorldContext, FName Key)
{
	if (UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext))
	{
		GRS->ClearValue(Key);
	}
}

void UGameplayResourceSubsystem::BPGetProperty(const UObject* WorldContext, FName Key, FName PropertyName, int32& Value)
{
	check(0);
}

void UGameplayResourceSubsystem::BPSetProperty(const UObject* WorldContext, FName Key, FName PropertyName, const int32& Value)
{
	check(0);
}

DEFINE_FUNCTION(UGameplayResourceSubsystem::execBPGetProperty)
{
	P_GET_OBJECT(UObject, WorldContext);
	P_GET_PROPERTY(FNameProperty, Key);
	P_GET_PROPERTY(FNameProperty, PropertyName);

	// Read the wildcard Value output. This is an out parameter, so the term is always a variable and the
	// step only has to hand back its address.
	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);

	FProperty* DestProp = Stack.MostRecentProperty;
	void* DestPtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	if (DestProp == nullptr || DestPtr == nullptr)
	{
		return;
	}

	P_NATIVE_BEGIN;
	const UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext);
	if (GRS)
	{
		if (const FInstancedPropertyBag* Bag = GRS->FindRuntimeBag(Key))
		{
			if (const FPropertyBagPropertyDesc* Desc = Bag->FindPropertyDescByName(PropertyName))
			{
				const FProperty* SrcProp = Desc->CachedProperty;

				// CachedProperty lives inside the bag's backing struct; resolve its address there.
				const uint8* Base = Bag->GetValue().GetMemory();

				// The K2 node builds the Value pin from the same desc, so the layouts normally match;
				// guard anyway to stay safe against a schema edited under a compiled Blueprint.
				if (SrcProp != nullptr && Base != nullptr && ArePropertiesLayoutCompatible(DestProp, SrcProp))
				{
					DestProp->CopyCompleteValue(DestPtr, SrcProp->ContainerPtrToValuePtr<void>(Base));
				}
			}
		}
	}
	P_NATIVE_END;
}

DEFINE_FUNCTION(UGameplayResourceSubsystem::execBPSetProperty)
{
	P_GET_OBJECT(UObject, WorldContext);
	P_GET_PROPERTY(FNameProperty, Key);
	P_GET_PROPERTY(FNameProperty, PropertyName);

	// Read the wildcard Value input (any type the bag can hold). A connected pin compiles to a variable
	// reference, which only sets MostRecentPropertyAddress, but a pin left at its default compiles to an
	// inline literal (EX_IntConst and friends) that writes straight through the address handed to the
	// step. Stepping with nullptr therefore faults on every unconnected Value pin, so give the step
	// scratch storage laid out like the property this call targets.
	const FProperty* SchemaProp = nullptr;
	if (const UPropertyBag* Schema = UGameplayResourcesDeveloperSettings::FindResourceSchema(Key))
	{
		if (const FPropertyBagPropertyDesc* Desc = Schema->FindPropertyDescByName(PropertyName))
		{
			SchemaProp = Desc->CachedProperty;
		}
	}

	void* Storage = nullptr;
	if (SchemaProp)
	{
		Storage = FMemory_Alloca_Aligned(SchemaProp->GetSize(), SchemaProp->GetMinAlignment());
		SchemaProp->InitializeValue(Storage);
	}
	else
	{
		// Unknown layout: the value can't be written anywhere useful, but the step still has to happen to
		// keep the bytecode stream aligned. Zeroed memory is a valid empty state for the constructed types
		// the literal opcodes assign into.
		Storage = FMemory_Alloca_Aligned(UnresolvedLiteralStorageSize, 16);
		FMemory::Memzero(Storage, UnresolvedLiteralStorageSize);
	}

	Stack.MostRecentProperty = nullptr;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FProperty>(Storage);

	const FProperty* SrcProp = Stack.MostRecentProperty;
	const void* SrcPtr = Stack.MostRecentPropertyAddress;
	if (SrcPtr == nullptr)
	{
		// Literal term: the value landed in the scratch storage instead of a variable.
		SrcProp = SchemaProp;
		SrcPtr = Storage;
	}

	P_FINISH;

	if (SrcProp != nullptr && SrcPtr != nullptr)
	{
		P_NATIVE_BEGIN;
		UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext);
		if (GRS)
		{
			bool bCreated = false;
			if (FInstancedPropertyBag* Bag = GRS->FindOrCreateRuntimeBag(Key, bCreated))
			{
				// Copied by hand rather than through FInstancedPropertyBag::SetValue: that overload treats
				// the source pointer as a *container* and re-applies the source property's offset, which is
				// wrong for both a stack variable's value address and the scratch storage above.
				bool bCopied = false;
				if (const FPropertyBagPropertyDesc* Desc = Bag->FindPropertyDescByName(PropertyName))
				{
					const FProperty* DestProp = Desc->CachedProperty;
					uint8* Base = Bag->GetMutableValue().GetMemory();
					if (DestProp != nullptr && Base != nullptr && ArePropertiesLayoutCompatible(DestProp, SrcProp))
					{
						DestProp->CopyCompleteValue(DestProp->ContainerPtrToValuePtr<void>(Base), SrcPtr);
						bCopied = true;
					}
				}

				// The bag exists from here on even if the copy was rejected, so creation is still reported.
				if (bCreated)
				{
					GRS->BroadcastResourceChange(Key, EGameplayResourceChangeType::Added, *Bag);
				}
				else if (bCopied)
				{
					GRS->BroadcastResourceChange(Key, EGameplayResourceChangeType::Changed, *Bag);
				}
			}
		}
		P_NATIVE_END;
	}

	if (SchemaProp != nullptr)
	{
		SchemaProp->DestroyValue(Storage);
	}
}
