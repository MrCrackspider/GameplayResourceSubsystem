// © Volga Studios. All rights reserved.


#include "GameplayResource.h"

#include "GameplayResourcesDeveloperSettings.h"
#include "GameplayResourceSubsystem.h"

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
