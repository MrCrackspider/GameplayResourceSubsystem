#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FGameplayResourceGraphPanelPinFactory;

class FGameplayResourcesEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    TSharedPtr<FGameplayResourceGraphPanelPinFactory> GameplayResourceGraphPanelPinFactory;

private:
    void RegisterPropertyCustomizations();
    void UnregisterPropertyCustomizations();
};
