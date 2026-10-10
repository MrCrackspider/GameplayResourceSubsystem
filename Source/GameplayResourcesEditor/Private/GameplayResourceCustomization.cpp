
#include "GameplayResourceCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "GameplayResource.h"
#include "GameplayResourcesDeveloperSettings.h"

TSharedRef<IPropertyTypeCustomization> FGameplayResourceCustomization::MakeInstance()
{
	return MakeShared<FGameplayResourceCustomization>();
}

void FGameplayResourceCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		];
}

void FGameplayResourceCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	KeyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGameplayResource, Key));
	BagHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGameplayResource, Bag));

	if (KeyHandle.IsValid())
	{
		KeyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FGameplayResourceCustomization::OnKeyChanged));
		ChildBuilder.AddProperty(KeyHandle.ToSharedRef());
	}

	if (BagHandle.IsValid())
	{
		ChildBuilder.AddProperty(BagHandle.ToSharedRef());
	}
}

void FGameplayResourceCustomization::OnKeyChanged()
{
	if (!KeyHandle.IsValid() || !BagHandle.IsValid())
	{
		return;
	}

	FString KeyString;
	KeyHandle->GetValueAsFormattedString(KeyString);
	const FName Key(*KeyString);

	const UPropertyBag* Schema = UGameplayResourcesDeveloperSettings::FindResourceSchema(Key);

	TArray<void*> RawData;
	BagHandle->AccessRawData(RawData);

	BagHandle->NotifyPreChange();
	for (void* Raw : RawData)
	{
		if (FInstancedPropertyBag* Bag = static_cast<FInstancedPropertyBag*>(Raw))
		{
			if (Schema)
			{
				Bag->MigrateToNewBagStruct(Schema);
			}
			else
			{
				Bag->Reset();
			}
		}
	}
	BagHandle->NotifyPostChange(EPropertyChangeType::ValueSet);

	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities.Pin()->ForceRefresh();
	}
}
