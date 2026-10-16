
#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class IPropertyUtilities;
struct FInstancedPropertyBag;

/**
 * Shared plumbing for the resource struct customizations: both keep an embedded property bag in sync with
 * the schema declared for a sibling Key, and both rebuild their rows when that schema changes.
 */
class FGameplayResourceCustomizationBase : public IPropertyTypeCustomization
{
public:
	//~ Begin IPropertyTypeCustomization Interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization Interface

protected:
	// The handle's FName value, or NAME_None when it is unset or holds several values across a multi-select.
	static FName GetNameValue(const TSharedPtr<IPropertyHandle>& Handle);

	// Applies Reshape to every bag behind BagHandle, bracketed by the pre/post change notifications the
	// details panel needs to pick the new layout up.
	static void ReshapeBags(const TSharedPtr<IPropertyHandle>& BagHandle, TFunctionRef<void(FInstancedPropertyBag&)> Reshape);

	// Rebuilds the detail rows, so the bag's new layout is actually drawn.
	void RequestRefresh() const;

	TWeakPtr<IPropertyUtilities> PropertyUtilities;
};

/**
 * Detail customization for FGameplayResource: re-shapes the embedded property bag to the schema
 * declared for the selected Key whenever the Key changes, keeping the bag's editable values in sync
 * with the DataTable.
 */
class FGameplayResourceCustomization : public FGameplayResourceCustomizationBase
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	//~ Begin IPropertyTypeCustomization Interface
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization Interface

private:
	void OnKeyChanged();

	TSharedPtr<IPropertyHandle> KeyHandle;
	TSharedPtr<IPropertyHandle> BagHandle;
};
