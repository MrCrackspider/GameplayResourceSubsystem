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

UScriptStruct* UGameplayResourcesDeveloperSettings::FindValueStruct(FName Key)
{
	if (const UDataTable* DT = GetDataTable())
	{
		if (const FGameplayResourceEntry* Row = DT->FindRow<FGameplayResourceEntry>(Key, TEXT("Temp")))
		{
			return Row->ResourceType;
		}
	}
	return nullptr;
}

#if WITH_EDITOR
void UGameplayResourcesDeveloperSettings::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	
	const FName MemberPropertyName = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetFName();
	
	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UGameplayResourcesDeveloperSettings, DefaultValues))
	{
		for (TTuple<FName, FInstancedStruct>& Value : DefaultValues)
		{
			if (const UScriptStruct* TargetStructType = FindValueStruct(Value.Key))
			{
				if (TargetStructType != Value.Value.GetScriptStruct())
				{
					Value.Value.InitializeAs(TargetStructType);
				}
			}
		}
	}
}

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
