#include "GameplayResourceSubsystemEditor.h"

#include "EdGraphUtilities.h"
#include "GameplayResourceCustomization.h"
#include "GameplayResourceGraphPanelPinFactory.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FGameplayResourceSubsystemEditorModule"

static const FName GameplayResourceStructName(TEXT("GameplayResource"));

void FGameplayResourceSubsystemEditorModule::StartupModule()
{
	GameplayResourceGraphPanelPinFactory = MakeShareable(new FGameplayResourceGraphPanelPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(GameplayResourceGraphPanelPinFactory);

	RegisterPropertyCustomizations();
}

void FGameplayResourceSubsystemEditorModule::ShutdownModule()
{
	if (GameplayResourceGraphPanelPinFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(GameplayResourceGraphPanelPinFactory);
	}

	UnregisterPropertyCustomizations();
}

void FGameplayResourceSubsystemEditorModule::RegisterPropertyCustomizations()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(
		GameplayResourceStructName,
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayResourceCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FGameplayResourceSubsystemEditorModule::UnregisterPropertyCustomizations()
{
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->UnregisterCustomPropertyTypeLayout(GameplayResourceStructName);
		PropertyModule->NotifyCustomizationModuleChanged();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGameplayResourceSubsystemEditorModule, GameplayResourceSubsystemEditor)
