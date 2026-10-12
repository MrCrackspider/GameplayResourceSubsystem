
#include "GameplayResource.h"

#include "GameplayResourcesDeveloperSettings.h"
#include "GameplayResourceSubsystem.h"
#include "GameplayResourceWildcardValue.h"

void FGameplayResource::SyncBagToSchema()
{
	if (const UPropertyBag* Schema = UGameplayResourcesDeveloperSettings::FindResourceSchema(Key))
	{
		Bag.MigrateToNewBagStruct(Schema);
	}
	else
	{
		Bag.Reset();
	}
}

bool FGameplayResource::ImportFromSubsystem(const UObject* WorldContext)
{
	if (const UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext))
	{
		if (const FInstancedPropertyBag* Runtime = GRS->FindRuntimeBag(Key))
		{
			Bag = *Runtime;
			return true;
		}
	}
	return false;
}

bool FGameplayResource::ExportToSubsystem(const UObject* WorldContext) const
{
	if (UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext))
	{
		GRS->SetResourceBag(Key, Bag);
		return true;
	}
	return false;
}

bool FGameplayResource::HasEqualValues(const FInstancedPropertyBag& OtherBag) const
{
	const UPropertyBag* StructA = Bag.GetPropertyBagStruct();
	const UPropertyBag* StructB = OtherBag.GetPropertyBagStruct();

	// Same layout, including both empty: compare the whole struct in one go.
	if (StructA == StructB)
	{
		return Bag.Identical(&OtherBag, PPF_None);
	}

	if (StructA == nullptr || StructB == nullptr || Bag.GetNumPropertiesInBag() != OtherBag.GetNumPropertiesInBag())
	{
		return false;
	}

	const uint8* BaseA = Bag.GetValue().GetMemory();
	const uint8* BaseB = OtherBag.GetValue().GetMemory();

	// IDs are unique within a bag, so equal counts plus every property of A found in B means the sets match.
	for (const FPropertyBagPropertyDesc& DescA : StructA->GetPropertyDescs())
	{
		const FPropertyBagPropertyDesc* DescB = StructB->FindPropertyDescByID(DescA.ID);
		if (DescB == nullptr || !UE::GameplayResources::ArePropertiesLayoutCompatible(DescA.CachedProperty, DescB->CachedProperty))
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

bool FGameplayResource::operator==(const FGameplayResource& Other) const
{
	return Key == Other.Key && HasEqualValues(Other.Bag);
}
