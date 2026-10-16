#include "GameplayResourcesEditor.h"

#include "EdGraphUtilities.h"
#include "GameplayResourceCustomization.h"
#include "GameplayResourceGraphPanelPinFactory.h"
#include "GameplayResourceValueCustomization.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FGameplayResourcesEditorModule"

static const FName GameplayResourceStructName(TEXT("GameplayResource"));
static const FName GameplayResourceValueStructName(TEXT("GameplayResourceValue"));

void FGameplayResourcesEditorModule::StartupModule()
{
	GameplayResourceGraphPanelPinFactory = MakeShareable(new FGameplayResourceGraphPanelPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(GameplayResourceGraphPanelPinFactory);

	RegisterPropertyCustomizations();
}

void FGameplayResourcesEditorModule::ShutdownModule()
{
	if (GameplayResourceGraphPanelPinFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(GameplayResourceGraphPanelPinFactory);
	}

	UnregisterPropertyCustomizations();
}

void FGameplayResourcesEditorModule::RegisterPropertyCustomizations()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(
		GameplayResourceStructName,
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayResourceCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(
		GameplayResourceValueStructName,
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayResourceValueCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FGameplayResourcesEditorModule::UnregisterPropertyCustomizations()
{
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->UnregisterCustomPropertyTypeLayout(GameplayResourceStructName);
		PropertyModule->UnregisterCustomPropertyTypeLayout(GameplayResourceValueStructName);
		PropertyModule->NotifyCustomizationModuleChanged();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGameplayResourcesEditorModule, GameplayResourcesEditor)
