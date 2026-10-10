
#include "GameplayResourceFunctionLibrary.h"

#include "GameplayResourceWildcardValue.h"

using UE::GameplayResources::ArePropertiesLayoutCompatible;
using UE::GameplayResources::UnresolvedLiteralStorageSize;

bool UGameplayResourceFunctionLibrary::ImportGameplayResource(const UObject* WorldContext, FGameplayResource& Resource)
{
	return Resource.ImportFromSubsystem(WorldContext);
}

bool UGameplayResourceFunctionLibrary::ExportGameplayResource(const UObject* WorldContext, const FGameplayResource& Resource)
{
	return Resource.ExportToSubsystem(WorldContext);
}

void UGameplayResourceFunctionLibrary::BPGetResourceProperty(const FGameplayResource& Resource, FName PropertyName, int32& Value)
{
	check(0);
}

void UGameplayResourceFunctionLibrary::BPSetResourceProperty(FGameplayResource& Resource, FName PropertyName, const int32& Value)
{
	check(0);
}

DEFINE_FUNCTION(UGameplayResourceFunctionLibrary::execBPGetResourceProperty)
{
	P_GET_STRUCT_REF(FGameplayResource, Resource);
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
	if (const FPropertyBagPropertyDesc* Desc = Resource.Bag.FindPropertyDescByName(PropertyName))
	{
		const FProperty* SrcProp = Desc->CachedProperty;
		const uint8* Base = Resource.Bag.GetValue().GetMemory();
		if (SrcProp != nullptr && Base != nullptr && ArePropertiesLayoutCompatible(DestProp, SrcProp))
		{
			DestProp->CopyCompleteValue(DestPtr, SrcProp->ContainerPtrToValuePtr<void>(Base));
		}
	}
	P_NATIVE_END;
}

DEFINE_FUNCTION(UGameplayResourceFunctionLibrary::execBPSetResourceProperty)
{
	P_GET_STRUCT_REF(FGameplayResource, Resource);
	P_GET_PROPERTY(FNameProperty, PropertyName);

	// Read the wildcard Value input (any type the bag can hold). A connected pin compiles to a variable
	// reference, which only sets MostRecentPropertyAddress, but a pin left at its default compiles to an
	// inline literal (EX_IntConst and friends) that writes straight through the address handed to the
	// step. Stepping with nullptr therefore faults on every unconnected Value pin, so give the step
	// scratch storage laid out like the property this call targets.
	const FProperty* SchemaProp = nullptr;
	if (const FPropertyBagPropertyDesc* Desc = Resource.Bag.FindPropertyDescByName(PropertyName))
	{
		SchemaProp = Desc->CachedProperty;
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
		// Copied by hand rather than through FInstancedPropertyBag::SetValue: that overload treats the
		// source pointer as a *container* and re-applies the source property's offset, which is wrong for
		// both a stack variable's value address and the scratch storage above.
		if (const FPropertyBagPropertyDesc* Desc = Resource.Bag.FindPropertyDescByName(PropertyName))
		{
			const FProperty* DestProp = Desc->CachedProperty;
			uint8* Base = Resource.Bag.GetMutableValue().GetMemory();
			if (DestProp != nullptr && Base != nullptr && ArePropertiesLayoutCompatible(DestProp, SrcProp))
			{
				DestProp->CopyCompleteValue(DestProp->ContainerPtrToValuePtr<void>(Base), SrcPtr);
			}
		}
		P_NATIVE_END;
	}

	if (SchemaProp != nullptr)
	{
		SchemaProp->DestroyValue(Storage);
	}
}
