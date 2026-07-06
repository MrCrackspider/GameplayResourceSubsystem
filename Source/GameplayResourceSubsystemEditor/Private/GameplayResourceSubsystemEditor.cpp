#include "GameplayResourceSubsystemEditor.h"

#include "EdGraphUtilities.h"
#include "GameplayResourceGraphPanelPinFactory.h"

#define LOCTEXT_NAMESPACE "FGameplayResourceSubsystemEditorModule"

void FGameplayResourceSubsystemEditorModule::StartupModule()
{
	GameplayResourceGraphPanelPinFactory = MakeShareable(new FGameplayResourceGraphPanelPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(GameplayResourceGraphPanelPinFactory);
}

void FGameplayResourceSubsystemEditorModule::ShutdownModule()
{
	if (GameplayResourceGraphPanelPinFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(GameplayResourceGraphPanelPinFactory);
	}
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FGameplayResourceSubsystemEditorModule, GameplayResourceSubsystemEditor)