
#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node_GlobalResourceAccessor.generated.h"

class UK2Node_CallFunction;
class FCompilerResultsLog;

/**
 * Shared base for resource accessor nodes. Expands one value pin per property declared in a resource
 * key's property bag schema. Subclasses provide how that schema key is obtained (a Key dropdown for
 * the subsystem nodes, or the wired FGameplayResource for the struct nodes), the fixed input pins,
 * and the per-property accessor function the node expands into.
 */
UCLASS(Abstract)
class GAMEPLAYRESOURCESEDITOR_API UK2Node_ResourceAccessorBase : public UK2Node
{
	GENERATED_BODY()

protected:

	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node Interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	//~ End UK2Node Interface

	UEdGraphPin* GetKeyPin() const;
	UEdGraphPin* GetFoundPin() const;

	// Collects the current per-property value pins (excludes the fixed exec/self/key/found/resource pins).
	void GetResourcePins(TArray<UEdGraphPin*>& OutPins) const;

	// Rebuilds the per-property value pins from whichever schema key is currently resolvable.
	// Returns true when the set of property pins actually changed.
	bool SyncResourcePins();
	// Rebuilds the per-property value pins from an explicit key. Returns true when pins changed.
	bool RebuildResourcePins(FName Key);

	//~ Subclass hooks.
	// Creates the subclass-specific fixed input pins (e.g. WorldContext or Resource).
	virtual void AllocateExtraFixedPins() {}
	virtual bool HasKeyPin() const { return true; }
	virtual bool ValuePinAsOutput() const { return true; }
	virtual bool HasFoundPin() const { return false; }
	// Resolves the schema key from the node's live state (pins/connections). Returns false when unknown.
	virtual bool TryGetSchemaKey(FName& OutKey) const;
	// Resolves the schema key from the pre-reconstruction pins (so property pins can be rebuilt before
	// the base class restores connections by name).
	virtual bool TryGetSchemaKeyFromOldPins(const TArray<UEdGraphPin*>& OldPins, FName& OutKey) const;
	// Whether a change on Pin should trigger a resync of the property pins.
	virtual bool IsResyncTriggerPin(const UEdGraphPin* Pin) const;

	virtual FName GetAccessFunctionName() const { return FName(); }
	virtual UClass* GetAccessFunctionClass() const { return nullptr; }
	// Verifies required connections at expansion time (while links are still intact). Report errors to
	// the compiler and return false to abort expansion.
	virtual bool VerifyRequiredConnections(class FKismetCompilerContext& CompilerContext) const { return true; }
	// Wires the shared (non value/property-name) inputs of a per-property call.
	virtual void WirePropertyCallSharedInputs(class FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* Call) {}
	// Wires any extra pins (e.g. the Get node's Found output) during expansion.
	virtual void ExpandExtraPins(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) {}
};

// ---------------------------------------------------------------------------------------------------
// Subsystem accessors: operate on the global runtime values held by UGameplayResourceSubsystem.
// The schema key comes from a Key dropdown pin.
// ---------------------------------------------------------------------------------------------------

UCLASS(Abstract)
class UK2Node_GlobalResourceAccessor : public UK2Node_ResourceAccessorBase
{
	GENERATED_BODY()

protected:

	virtual void AllocateExtraFixedPins() override;
	virtual UClass* GetAccessFunctionClass() const override;
	virtual void WirePropertyCallSharedInputs(class FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* Call) override;

	UEdGraphPin* GetContextPin() const;
};

UCLASS()
class UK2Node_GetGlobalResource : public UK2Node_GlobalResourceAccessor
{
	GENERATED_BODY()

public:

	virtual FText GetNodeTitle(ENodeTitleType::Type) const override;
	virtual FName GetAccessFunctionName() const override;
	virtual bool HasFoundPin() const override { return true; }

protected:

	virtual void ExpandExtraPins(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
};

UCLASS()
class UK2Node_SetGlobalResource : public UK2Node_GlobalResourceAccessor
{
	GENERATED_BODY()

public:

	virtual FText GetNodeTitle(ENodeTitleType::Type) const override;
	virtual bool ValuePinAsOutput() const override { return false; }
	virtual FName GetAccessFunctionName() const override;
};

// ---------------------------------------------------------------------------------------------------
// Struct accessors: operate on an FGameplayResource copy passed in on the (required) Resource pin.
// The schema key is read from the wired FGameplayResource variable's default Key.
// ---------------------------------------------------------------------------------------------------

UCLASS(Abstract)
class UK2Node_GameplayResourceAccessor : public UK2Node_ResourceAccessorBase
{
	GENERATED_BODY()

protected:

	virtual void AllocateExtraFixedPins() override;
	virtual bool HasKeyPin() const override { return false; }
	virtual bool TryGetSchemaKey(FName& OutKey) const override;
	virtual bool TryGetSchemaKeyFromOldPins(const TArray<UEdGraphPin*>& OldPins, FName& OutKey) const override;
	virtual bool IsResyncTriggerPin(const UEdGraphPin* Pin) const override;
	virtual bool VerifyRequiredConnections(class FKismetCompilerContext& CompilerContext) const override;
	virtual UClass* GetAccessFunctionClass() const override;
	virtual void WirePropertyCallSharedInputs(class FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* Call) override;

	UEdGraphPin* GetResourcePin() const;
};

UCLASS()
class UK2Node_GetGameplayResource : public UK2Node_GameplayResourceAccessor
{
	GENERATED_BODY()

public:

	virtual FText GetNodeTitle(ENodeTitleType::Type) const override;
	virtual FName GetAccessFunctionName() const override;
};

UCLASS()
class UK2Node_SetGameplayResource : public UK2Node_GameplayResourceAccessor
{
	GENERATED_BODY()

public:

	virtual FText GetNodeTitle(ENodeTitleType::Type) const override;
	virtual bool ValuePinAsOutput() const override { return false; }
	virtual FName GetAccessFunctionName() const override;
};
