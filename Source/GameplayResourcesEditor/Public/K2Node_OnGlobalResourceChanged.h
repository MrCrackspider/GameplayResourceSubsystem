#pragma once

#include "CoreMinimal.h"
#include "K2Node_GlobalResourceAccessor.h"
#include "K2Node_OnGlobalResourceChanged.generated.h"

/**
 * "On Global Resource Changed": registers a change listener on UGameplayResourceSubsystem and fires its
 * On Changed exec output on every matching change. Like the Get/Set nodes, it grows one output pin per
 * property declared for the selected Key; those pins carry the value delivered with the change (the new
 * value for Added/Changed, the removed value for Removed).
 *
 * Expands into a stock UK2Node_AsyncAction for UAsyncAction_ListenForGlobalResourceChange (which carries
 * HasDedicatedAsyncNode so it doesn't appear on its own), followed by one
 * UGameplayResourceFunctionLibrary::BPGetResourceProperty call per connected property pin. A persistent
 * temporary variable hands the previous action back to the factory so running the node again replaces it.
 * Event graphs only, like every async action node.
 */
UCLASS()
class GAMEPLAYRESOURCESEDITOR_API UK2Node_OnGlobalResourceChanged : public UK2Node_ResourceAccessorBase
{
	GENERATED_BODY()

public:

	//~ Begin UEdGraphNode Interface.
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	//~ End UEdGraphNode Interface.

protected:

	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node_ResourceAccessorBase Interface.
	virtual void AllocateExtraFixedPins() override;
	virtual bool IsFixedPin(const UEdGraphPin& Pin) const override;
	//~ End UK2Node_ResourceAccessorBase Interface.

	UEdGraphPin* GetContextPin() const;
	UEdGraphPin* GetChangeMaskPin() const;
	UEdGraphPin* GetOnChangedPin() const;
	UEdGraphPin* GetChangeTypePin() const;
	UEdGraphPin* GetAsyncActionPin() const;
};
