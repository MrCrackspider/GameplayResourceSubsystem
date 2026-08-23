#include "GameplayResourceGraphPanelPinFactory.h"

#include "GameplayResourcesDeveloperSettings.h"
#include "K2Node_GlobalResourceAccessor.h"
#include "SGraphPinDataTableRowName.h"


TSharedPtr<class SGraphPin> FGameplayResourceGraphPanelPinFactory::CreatePin(class UEdGraphPin* InPin) const
{
	if (InPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Name)
	{
		return nullptr;
	}

	// Only the Key pin gets the DataTable row-name dropdown; bag properties may also be Name-typed.
	if (InPin->PinName != TEXT("Key"))
	{
		return nullptr;
	}

	UObject* Outer = InPin->GetOuter();
	if (!Outer->IsA(UK2Node_ResourceAccessorBase::StaticClass()))
	{
		return nullptr;
	}

	UDataTable* DataTable = UGameplayResourcesDeveloperSettings::GetDataTable();

	if (!DataTable)
	{
		return nullptr;
	}
	
	return SNew(SGraphPinDataTableRowName, InPin, DataTable);
}
