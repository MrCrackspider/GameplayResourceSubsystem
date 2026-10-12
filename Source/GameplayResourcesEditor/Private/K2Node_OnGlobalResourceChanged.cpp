#include "K2Node_OnGlobalResourceChanged.h"

#include "Async/AsyncAction_ListenForGlobalResourceChange.h"
#include "EdGraphSchema_K2.h"
#include "GameplayResource.h"
#include "GameplayResourceFunctionLibrary.h"
#include "GameplayResourceSubsystem.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_TemporaryVariable.h"
#include "KismetCompiler.h"

#define LOCTEXT_NAMESPACE "K2Node_OnGlobalResourceChanged"

namespace
{
	const FName ChangeMaskPinName(TEXT("ChangeMask"));
	const FName OnChangedPinName(TEXT("OnChanged"));
	const FName ChangeTypePinName(TEXT("ChangeType"));
	const FName ResourcePinName(TEXT("Resource"));
	// Same name UK2Node_BaseAsyncTask gives the exposed proxy pin, so the pin reads the same as on other async nodes.
	const FName AsyncActionPinName(TEXT("AsyncTaskProxy"));
	const FName ContextPinFriendlyName(TEXT("WorldContext"));

	// Pins of the intermediate async node: factory parameters, the delegate's exec pin and its parameters.
	const FName AsyncWorldContextPinName(TEXT("WorldContext"));
	const FName AsyncKeyPinName(TEXT("Key"));
	const FName AsyncChangeMaskPinName(TEXT("ChangeMask"));
	const FName AsyncPreviousListenerPinName(TEXT("PreviousListener"));

	// Parameters of UGameplayResourceFunctionLibrary::BPGetResourceProperty.
	const FName GetPropertyResourceParamName(TEXT("Resource"));
	const FName GetPropertyNameParamName(TEXT("PropertyName"));
	const FName GetPropertyValueParamName(TEXT("Value"));
}

// ===================================================================================================
// Pins
// ===================================================================================================

void UK2Node_OnGlobalResourceChanged::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	// The base creates the Key pin after the extra fixed pins; list the mask after the key, since the key is
	// what the node is about.
	UEdGraphPin* KeyPin = GetKeyPin();
	UEdGraphPin* MaskPin = GetChangeMaskPin();
	if (KeyPin != nullptr && MaskPin != nullptr)
	{
		Pins.Remove(MaskPin);
		Pins.Insert(MaskPin, Pins.IndexOfByKey(KeyPin) + 1);
	}
}

void UK2Node_OnGlobalResourceChanged::AllocateExtraFixedPins()
{
	UEdGraphPin* ContextPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), UEdGraphSchema_K2::PN_Self);
	ContextPin->PinFriendlyName = FText::FromName(ContextPinFriendlyName);

	UEdGraphPin* MaskPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, UEdGraphSchema_K2::PSC_Bitmask,
		StaticEnum<EGameplayResourceChangeType>(), ChangeMaskPinName);
	MaskPin->DefaultValue = FString::FromInt(static_cast<int32>(EGameplayResourceChangeType::All));

	UEdGraphPin* AsyncActionPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Object,
		UAsyncAction_ListenForGlobalResourceChange::StaticClass(), AsyncActionPinName);
	AsyncActionPin->PinFriendlyName = LOCTEXT("AsyncActionPinName", "Async Action");

	UEdGraphPin* OnChangedPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, OnChangedPinName);
	OnChangedPin->PinFriendlyName = LOCTEXT("OnChangedPinName", "On Changed");

	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Byte, StaticEnum<EGameplayResourceChangeType>(), ChangeTypePinName);
}

bool UK2Node_OnGlobalResourceChanged::IsFixedPin(const UEdGraphPin& Pin) const
{
	return Super::IsFixedPin(Pin)
		|| Pin.PinName == ChangeMaskPinName
		|| Pin.PinName == AsyncActionPinName
		|| Pin.PinName == OnChangedPinName
		|| Pin.PinName == ChangeTypePinName;
}

UEdGraphPin* UK2Node_OnGlobalResourceChanged::GetContextPin() const
{
	return FindPin(UEdGraphSchema_K2::PN_Self);
}

UEdGraphPin* UK2Node_OnGlobalResourceChanged::GetChangeMaskPin() const
{
	return FindPin(ChangeMaskPinName);
}

UEdGraphPin* UK2Node_OnGlobalResourceChanged::GetOnChangedPin() const
{
	return FindPin(OnChangedPinName);
}

UEdGraphPin* UK2Node_OnGlobalResourceChanged::GetChangeTypePin() const
{
	return FindPin(ChangeTypePinName);
}

UEdGraphPin* UK2Node_OnGlobalResourceChanged::GetAsyncActionPin() const
{
	return FindPin(AsyncActionPinName);
}

// ===================================================================================================
// Presentation
// ===================================================================================================

FText UK2Node_OnGlobalResourceChanged::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "On Global Resource Changed");
}

FText UK2Node_OnGlobalResourceChanged::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Starts listening for changes to a global gameplay resource. On Changed fires for every change type in "
		"Change Mask; the property pins hold the new value (Added/Changed) or the removed value (Removed).\n"
		"Leave Key empty to listen to every key. Call Cancel on Async Action to stop. Running this node again "
		"replaces the previous listener, and the listener stops when this object is destroyed.");
}

bool UK2Node_OnGlobalResourceChanged::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	// Async action nodes spawn a custom event, which only exists in event graphs.
	const UEdGraphSchema* GraphSchema = TargetGraph != nullptr ? TargetGraph->GetSchema() : nullptr;
	return GraphSchema != nullptr
		&& GraphSchema->GetGraphType(TargetGraph) == GT_Ubergraph
		&& Super::IsCompatibleWithGraph(TargetGraph);
}

// ===================================================================================================
// Expansion
// ===================================================================================================

void UK2Node_OnGlobalResourceChanged::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	// Skip UK2Node_ResourceAccessorBase::ExpandNode: this node does not expand into chained accessor calls.
	UK2Node::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	UClass* ActionClass = UAsyncAction_ListenForGlobalResourceChange::StaticClass();
	bool bIsErrorFree = true;

	const UFunction* FactoryFunction = ActionClass->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UAsyncAction_ListenForGlobalResourceChange, ListenForGlobalResourceChange));
	check(FactoryFunction != nullptr);

	// The async node: factory, delegate binding, Activate. Its own expansion runs after this one.
	UK2Node_AsyncAction* AsyncNode = CompilerContext.SpawnIntermediateNode<UK2Node_AsyncAction>(this, SourceGraph);
	AsyncNode->InitializeProxyFromFunction(FactoryFunction);
	AsyncNode->AllocateDefaultPins();

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *AsyncNode->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetContextPin(), *AsyncNode->FindPinChecked(AsyncWorldContextPinName));

	UEdGraphPin* AsyncKeyPin = AsyncNode->FindPinChecked(AsyncKeyPinName);
	AsyncKeyPin->DefaultValue = GetKeyPin()->DefaultValue;
	CompilerContext.MovePinLinksToIntermediate(*GetKeyPin(), *AsyncKeyPin);

	UEdGraphPin* AsyncMaskPin = AsyncNode->FindPinChecked(AsyncChangeMaskPinName);
	AsyncMaskPin->DefaultValue = GetChangeMaskPin()->DefaultValue;
	CompilerContext.MovePinLinksToIntermediate(*GetChangeMaskPin(), *AsyncMaskPin);

	// Ubergraph locals persist across executions, so this remembers the action the node created last time and
	// hands it back to the factory to be cancelled.
	UK2Node_TemporaryVariable* PreviousListener = CompilerContext.SpawnInternalVariable(this, UEdGraphSchema_K2::PC_Object, NAME_None, ActionClass);
	bIsErrorFree &= Schema->TryCreateConnection(PreviousListener->GetVariablePin(), AsyncNode->FindPinChecked(AsyncPreviousListenerPinName));

	UEdGraphPin* AsyncActionPin = AsyncNode->FindPinChecked(AsyncActionPinName);
	CompilerContext.MovePinLinksToIntermediate(*GetAsyncActionPin(), *AsyncActionPin);

	// Then: remember the new action, then continue downstream.
	UK2Node_AssignmentStatement* RememberListener = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	RememberListener->AllocateDefaultPins();
	bIsErrorFree &= Schema->TryCreateConnection(AsyncNode->FindPinChecked(UEdGraphSchema_K2::PN_Then), RememberListener->GetExecPin());
	bIsErrorFree &= Schema->TryCreateConnection(PreviousListener->GetVariablePin(), RememberListener->GetVariablePin());
	RememberListener->NotifyPinConnectionListChanged(RememberListener->GetVariablePin());
	bIsErrorFree &= Schema->TryCreateConnection(AsyncActionPin, RememberListener->GetValuePin());
	RememberListener->NotifyPinConnectionListChanged(RememberListener->GetValuePin());
	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *RememberListener->GetThenPin());

	// On Changed: the delegate's outputs, then one read per connected property pin from the delivered snapshot.
	// The snapshot itself isn't exposed; the property pins are the node's view of it.
	UEdGraphPin* AsyncResourcePin = AsyncNode->FindPinChecked(ResourcePinName);
	CompilerContext.MovePinLinksToIntermediate(*GetChangeTypePin(), *AsyncNode->FindPinChecked(ChangeTypePinName));

	UEdGraphPin* LastThenPin = AsyncNode->FindPinChecked(GET_MEMBER_NAME_CHECKED(UAsyncAction_ListenForGlobalResourceChange, OnResourceChanged));

	TArray<UEdGraphPin*> PropertyPins;
	GetResourcePins(PropertyPins);
	for (UEdGraphPin* PropertyPin : PropertyPins)
	{
		if (PropertyPin->LinkedTo.Num() == 0)
		{
			continue;
		}

		UK2Node_CallFunction* GetProperty = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		GetProperty->FunctionReference.SetExternalMember(
			GET_FUNCTION_NAME_CHECKED(UGameplayResourceFunctionLibrary, BPGetResourceProperty), UGameplayResourceFunctionLibrary::StaticClass());
		GetProperty->AllocateDefaultPins();

		GetProperty->FindPinChecked(GetPropertyNameParamName)->DefaultValue = PropertyPin->PinName.ToString();
		bIsErrorFree &= Schema->TryCreateConnection(AsyncResourcePin, GetProperty->FindPinChecked(GetPropertyResourceParamName));

		// Force the wildcard Value pin to this property's concrete type, then move links.
		UEdGraphPin* ValuePin = GetProperty->FindPinChecked(GetPropertyValueParamName);
		ValuePin->PinType = PropertyPin->PinType;
		CompilerContext.MovePinLinksToIntermediate(*PropertyPin, *ValuePin);

		bIsErrorFree &= Schema->TryCreateConnection(LastThenPin, GetProperty->GetExecPin());
		LastThenPin = GetProperty->GetThenPin();
	}

	CompilerContext.MovePinLinksToIntermediate(*GetOnChangedPin(), *LastThenPin);

	if (!bIsErrorFree)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("ExpansionFailed", "@@ failed to expand. Refresh the node and recompile.").ToString(), this);
	}

	BreakAllNodeLinks();
}

#undef LOCTEXT_NAMESPACE
