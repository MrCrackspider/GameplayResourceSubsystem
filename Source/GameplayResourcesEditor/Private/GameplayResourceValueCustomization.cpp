
#include "GameplayResourceValueCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "GameplayResourceValue.h"
#include "GameplayResourcesDeveloperSettings.h"
#include "StructUtils/PropertyBag.h"

TSharedRef<IPropertyTypeCustomization> FGameplayResourceValueCustomization::MakeInstance()
{
	return MakeShared<FGameplayResourceValueCustomization>();
}

void FGameplayResourceValueCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	KeyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGameplayResourceValue, Key));
	ValueNameHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGameplayResourceValue, ValueName));
	ValueHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGameplayResourceValue, Value));

	if (KeyHandle.IsValid())
	{
		KeyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FGameplayResourceValueCustomization::OnKeyChanged));
		ChildBuilder.AddProperty(KeyHandle.ToSharedRef());
	}

	if (ValueNameHandle.IsValid())
	{
		// Bound rather than handled in the combo's selection delegate, so a reset to default or an edit from
		// anywhere else re-shapes the bag too.
		ValueNameHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FGameplayResourceValueCustomization::OnValueNameChanged));

		ChildBuilder.AddProperty(ValueNameHandle.ToSharedRef())
			.CustomWidget()
			.NameContent()
			[
				ValueNameHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MinDesiredWidth(150.0f)
			[
				PropertyCustomizationHelpers::MakePropertyComboBox(
					ValueNameHandle,
					FOnGetPropertyComboBoxStrings::CreateSP(this, &FGameplayResourceValueCustomization::GetValueNameOptions))
			];
	}

	if (ValueHandle.IsValid())
	{
		ChildBuilder.AddProperty(ValueHandle.ToSharedRef());
	}
}

void FGameplayResourceValueCustomization::OnKeyChanged()
{
	// A value name from the previous key is meaningless under the new one.
	const FName ValueName = GetNameValue(ValueNameHandle);
	if (!ValueName.IsNone() && !UGameplayResourcesDeveloperSettings::GetValueNames(GetNameValue(KeyHandle)).Contains(ValueName))
	{
		ValueNameHandle->SetValue(NAME_None);
	}

	SyncValueToSchema();
	RequestRefresh();
}

void FGameplayResourceValueCustomization::OnValueNameChanged()
{
	SyncValueToSchema();
	RequestRefresh();
}

void FGameplayResourceValueCustomization::SyncValueToSchema()
{
	const UPropertyBag* Schema = UGameplayResourcesDeveloperSettings::FindResourceSchema(GetNameValue(KeyHandle));
	const FPropertyBagPropertyDesc* SchemaDesc = Schema ? Schema->FindPropertyDescByName(GetNameValue(ValueNameHandle)) : nullptr;

	// Mirrors FGameplayResourceValue::SyncValueToSchema: the desc is copied whole so the bag keeps the
	// schema's property ID and stays interchangeable with the key's full bag.
	const FPropertyBagPropertyDesc SingleDesc = SchemaDesc ? *SchemaDesc : FPropertyBagPropertyDesc();
	const UPropertyBag* SingleBagStruct = SchemaDesc ? UPropertyBag::GetOrCreateFromDescs(MakeArrayView(&SingleDesc, 1)) : nullptr;

	ReshapeBags(ValueHandle, [SingleBagStruct](FInstancedPropertyBag& Bag)
	{
		if (SingleBagStruct)
		{
			Bag.MigrateToNewBagStruct(SingleBagStruct);
		}
		else
		{
			Bag.Reset();
		}
	});
}

void FGameplayResourceValueCustomization::GetValueNameOptions(TArray<TSharedPtr<FString>>& OutStrings, TArray<TSharedPtr<SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems) const
{
	for (const FName ValueName : UGameplayResourcesDeveloperSettings::GetValueNames(GetNameValue(KeyHandle)))
	{
		OutStrings.Add(MakeShared<FString>(ValueName.ToString()));
		OutToolTips.Add(nullptr);
		OutRestrictedItems.Add(false);
	}
}
