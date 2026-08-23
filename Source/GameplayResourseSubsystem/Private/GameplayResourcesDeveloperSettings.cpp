// © Volga Studios. All rights reserved.


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
