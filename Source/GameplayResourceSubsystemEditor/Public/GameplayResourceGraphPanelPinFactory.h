#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"
#include "Templates/SharedPointer.h"

class SGraphPin;

class FGameplayResourceGraphPanelPinFactory: public FGraphPanelPinFactory
{
	GAMEPLAYRESOURCESUBSYSTEMEDITOR_API virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override;
};
