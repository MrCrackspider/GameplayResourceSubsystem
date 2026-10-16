
#include "GameplayResourcesDeveloperSettings.h"

UDataTable* UGameplayResourcesDeveloperSettings::GetDataTable()
{
	return GetDefault<UGameplayResourcesDeveloperSettings>()->GlobalResourcesDataTable.LoadSynchronous();
}

TArray<FName> UGameplayResourcesDeveloperSettings::GetKeys()
{
	const UDataTable* Table = GetDataTable();
	
	if (Table == nullptr)
	{
		return {};
	}
	
	return Table->GetRowNames();
}

TArray<FName> UGameplayResourcesDeveloperSettings::GetValueNames(FName Key)
{
	TArray<FName> ValueNames;

	if (const UPropertyBag* Schema = FindResourceSchema(Key))
	{
		const TConstArrayView<FPropertyBagPropertyDesc> Descs = Schema->GetPropertyDescs();
		ValueNames.Reserve(Descs.Num());
		for (const FPropertyBagPropertyDesc& Desc : Descs)
		{
			ValueNames.Add(Desc.Name);
		}
	}

	return ValueNames;
}

const FInstancedPropertyBag* UGameplayResourcesDeveloperSettings::FindResourceBag(FName Key)
{
	if (const UDataTable* DT = GetDataTable())
	{
		if (const FGameplayResourceEntry* Row = DT->FindRow<FGameplayResourceEntry>(Key, TEXT("Temp")))
		{
			return &Row->Resources;
		}
	}
	return nullptr;
}

const UPropertyBag* UGameplayResourcesDeveloperSettings::FindResourceSchema(FName Key)
{
	if (const FInstancedPropertyBag* Bag = FindResourceBag(Key))
	{
		return Bag->GetPropertyBagStruct();
	}
	return nullptr;
}

#if WITH_EDITOR
FString UGameplayResourcesDeveloperSettings::GetDescription(FName Key)
{
	FString OutDescription{TEXT("No Description")};

	if (!GetDefault<UGameplayResourcesDeveloperSettings>()->bDisplayNodeDescription)
	{
		return OutDescription;
	}
	
	UDataTable* DT = GetDataTable();
	
	if (DT == nullptr)
	{
		return {TEXT("Unknown Resource Key")};
	}

	if (FGameplayResourceEntry* Found = DT->FindRow<FGameplayResourceEntry>(Key, TEXT("Temp")))
	{
		if (Found->Description.IsEmpty())
		{
			return OutDescription;
		}
		return Found->Description.ToString();
	}
	
	return {TEXT("Unknown Resource Key")};
}
#endif
