
#include "GameplayResourceCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "GameplayResource.h"
#include "GameplayResourcesDeveloperSettings.h"
#include "StructUtils/PropertyBag.h"

void FGameplayResourceCustomizationBase::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		];
}

FName FGameplayResourceCustomizationBase::GetNameValue(const TSharedPtr<IPropertyHandle>& Handle)
{
	FName Result = NAME_None;
	if (Handle.IsValid() && Handle->GetValue(Result) != FPropertyAccess::Success)
	{
		return NAME_None;
	}
	return Result;
}

void FGameplayResourceCustomizationBase::ReshapeBags(const TSharedPtr<IPropertyHandle>& BagHandle, TFunctionRef<void(FInstancedPropertyBag&)> Reshape)
{
	if (!BagHandle.IsValid())
	{
		return;
	}

	TArray<void*> RawData;
	BagHandle->AccessRawData(RawData);

	BagHandle->NotifyPreChange();
	for (void* Raw : RawData)
	{
		if (FInstancedPropertyBag* Bag = static_cast<FInstancedPropertyBag*>(Raw))
		{
			Reshape(*Bag);
		}
	}
	BagHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
}

void FGameplayResourceCustomizationBase::RequestRefresh() const
{
	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities.Pin()->ForceRefresh();
	}
}

TSharedRef<IPropertyTypeCustomization> FGameplayResourceCustomization::MakeInstance()
{
	return MakeShared<FGameplayResourceCustomization>();
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
	const UPropertyBag* Schema = UGameplayResourcesDeveloperSettings::FindResourceSchema(GetNameValue(KeyHandle));

	ReshapeBags(BagHandle, [Schema](FInstancedPropertyBag& Bag)
	{
		if (Schema)
		{
			Bag.MigrateToNewBagStruct(Schema);
		}
		else
		{
			Bag.Reset();
		}
	});

	RequestRefresh();
}
