// © Volga Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node_GlobalResourceAccessor.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class GAMEPLAYRESOURCESUBSYSTEMEDITOR_API UK2Node_GlobalResourceAccessor : public UK2Node
{
	GENERATED_BODY()
	
protected:
	
	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void PostReconstructNode() override;
	//~ End UEdGraphNode Interface.
	
	//~ Begin UK2Node Interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	//~ End UK2Node Interface
	
	UEdGraphPin* GetKeyPin() const;
	UEdGraphPin* GetValuePin() const;
	UEdGraphPin* GetContextPin() const;
	virtual FName GetAccessFunctionName() const { return FName(); }
	virtual bool ValuePinAsOutput() const { return true; }
	
	void UpdateValuePin();
};

UCLASS()
class UK2Node_GetGlobalResource : public UK2Node_GlobalResourceAccessor
{
	GENERATED_BODY()

public:
	
	//~ Begin UEdGraphNode Interface.
	virtual FText GetNodeTitle(ENodeTitleType::Type) const override;
	//~ End UEdGraphNode Interface.
	
	virtual FName GetAccessFunctionName() const override;
};

UCLASS()
class UK2Node_SetGlobalResource : public UK2Node_GlobalResourceAccessor
{
	GENERATED_BODY()

public:
	
	//~ Begin UEdGraphNode Interface.
	virtual FText GetNodeTitle(ENodeTitleType::Type) const override;
	//~ End UEdGraphNode Interface.
	
	virtual bool ValuePinAsOutput() const override { return false; }
	
	virtual FName GetAccessFunctionName() const override;
	

};

