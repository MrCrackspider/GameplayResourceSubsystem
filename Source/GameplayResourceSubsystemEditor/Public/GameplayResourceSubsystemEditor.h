#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FGameplayResourceGraphPanelPinFactory;

class FGameplayResourceSubsystemEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    
    TSharedPtr<FGameplayResourceGraphPanelPinFactory> GameplayResourceGraphPanelPinFactory;
};
