
#pragma once

#include "CoreMinimal.h"
#include "GameplayResourceCustomization.h"

class SToolTip;

/**
 * Detail customization for FGameplayResourceValue. Adds a ValueName dropdown listing the properties of the
 * selected Key's schema - the list depends on a sibling property, which GetOptions cannot express - and
 * re-shapes the single-property bag whenever Key or ValueName changes.
 */
class FGameplayResourceValueCustomization : public FGameplayResourceCustomizationBase
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	//~ Begin IPropertyTypeCustomization Interface
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization Interface

private:
	void OnKeyChanged();
	void OnValueNameChanged();

	// Re-shapes the bag to the schema property named by ValueName, or empties it when there is none.
	void SyncValueToSchema();

	// Fills the ValueName dropdown from the current Key's schema.
	void GetValueNameOptions(TArray<TSharedPtr<FString>>& OutStrings, TArray<TSharedPtr<SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems) const;

	TSharedPtr<IPropertyHandle> KeyHandle;
	TSharedPtr<IPropertyHandle> ValueNameHandle;
	TSharedPtr<IPropertyHandle> ValueHandle;
};
