#include "GameplayResourceSubsystem.h"

#include "GameplayResourceWildcardValue.h"

using UE::GameplayResources::ArePropertiesLayoutCompatible;
using UE::GameplayResources::UnresolvedLiteralStorageSize;

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

FInstancedPropertyBag* UGameplayResourceSubsystem::FindOrCreateRuntimeBag(FName Key)
{
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
	return &NewBag;
}

void UGameplayResourceSubsystem::SetResourceBag(FName Key, const FInstancedPropertyBag& InBag)
{
	if (!Key.IsValid())
	{
		return;
	}

	if (FInstancedPropertyBag* Bag = FindOrCreateRuntimeBag(Key))
	{
		Bag->CopyMatchingValuesByID(InBag);
		OnValueUpdated.Broadcast(Key);
	}
}

void UGameplayResourceSubsystem::ResetValue(FName Key)
{
	if (!Key.IsValid())
	{
		return;
	}

	Values.Remove(Key);

	// Re-seed the default value if this key is flagged as a startup default.
	if (const FInstancedPropertyBag* DefaultBag = UGameplayResourcesDeveloperSettings::FindResourceBag(Key))
	{
		if (const UDataTable* DT = UGameplayResourcesDeveloperSettings::GetDataTable())
		{
			if (const FGameplayResourceEntry* Row = DT->FindRow<FGameplayResourceEntry>(Key, TEXT("GameplayResourceSubsystem::ResetValue")))
			{
				if (Row->bSetAsDefault && Row->Resources.IsValid())
				{
					Values.Add(Key, Row->Resources);
				}
			}
		}
	}

	OnValueUpdated.Broadcast(Key);
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
			if (FInstancedPropertyBag* Bag = GRS->FindOrCreateRuntimeBag(Key))
			{
				// Copied by hand rather than through FInstancedPropertyBag::SetValue: that overload treats
				// the source pointer as a *container* and re-applies the source property's offset, which is
				// wrong for both a stack variable's value address and the scratch storage above.
				if (const FPropertyBagPropertyDesc* Desc = Bag->FindPropertyDescByName(PropertyName))
				{
					const FProperty* DestProp = Desc->CachedProperty;
					uint8* Base = Bag->GetMutableValue().GetMemory();
					if (DestProp != nullptr && Base != nullptr && ArePropertiesLayoutCompatible(DestProp, SrcProp))
					{
						DestProp->CopyCompleteValue(DestProp->ContainerPtrToValuePtr<void>(Base), SrcPtr);
						GRS->OnValueUpdated.Broadcast(Key);
					}
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
