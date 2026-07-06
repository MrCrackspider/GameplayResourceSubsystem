// © Volga Studios. All rights reserved.


#include "K2Node_GlobalResourceAccessor.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "GameplayResourcesDeveloperSettings.h"
#include "GameplayResourceSubsystem.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"

#define LOCTEXT_NAMESPACE "K2Node_GetSetGlobalResource"

struct FK2Node_ResourceAccessorHelper
{
	static const FName KeyPinName;
	static const FName ValuePinName;
	static const FName ContextPinFriendlyName;
};

const FName FK2Node_ResourceAccessorHelper::KeyPinName(TEXT("Key"));
const FName FK2Node_ResourceAccessorHelper::ValuePinName(TEXT("Value"));
const FName FK2Node_ResourceAccessorHelper::ContextPinFriendlyName(TEXT("WorldContext"));

UEdGraphPin* UK2Node_GlobalResourceAccessor::GetKeyPin() const
{
	return FindPin(FK2Node_ResourceAccessorHelper::KeyPinName);
}

UEdGraphPin* UK2Node_GlobalResourceAccessor::GetValuePin() const
{
	return FindPin(FK2Node_ResourceAccessorHelper::ValuePinName);
}

UEdGraphPin* UK2Node_GlobalResourceAccessor::GetContextPin() const
{
	return FindPin(UEdGraphSchema_K2::PN_Self);
}

FText UK2Node_GlobalResourceAccessor::GetMenuCategory() const
{
	return LOCTEXT("GameplayResourceSubsystem", "Gameplay Resource Subsystem");
}

void UK2Node_GlobalResourceAccessor::PostReconstructNode()
{
	Super::PostReconstructNode();
	UpdateValuePin();
}

void UK2Node_GlobalResourceAccessor::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	Super::GetPinHoverText(Pin, HoverTextOut);
	if (&Pin == GetKeyPin())
	{
		HoverTextOut = UGameplayResourcesDeveloperSettings::GetDescription(FName{Pin.DefaultValue});
	}
}

void UK2Node_GlobalResourceAccessor::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void UK2Node_GlobalResourceAccessor::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	
	const FName FunctionName = GetAccessFunctionName();
	UK2Node_CallFunction* GetValueFunction = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	GetValueFunction->FunctionReference.SetExternalMember(FunctionName, UGameplayResourceSubsystem::StaticClass());
	GetValueFunction->AllocateDefaultPins();
	
	// Exec
	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *(GetValueFunction->GetExecPin()));
	
	// Input WorldContext
	UEdGraphPin* WorldContextPin = GetValueFunction->FindPinChecked(FK2Node_ResourceAccessorHelper::ContextPinFriendlyName);
	CompilerContext.MovePinLinksToIntermediate(*GetContextPin(), *WorldContextPin);
	
	// Input Key
	UEdGraphPin* InputKeyPin = GetValueFunction->FindPinChecked(FK2Node_ResourceAccessorHelper::KeyPinName);
	CompilerContext.MovePinLinksToIntermediate(*GetKeyPin(), *InputKeyPin);

	// Output Value
	UEdGraphPin* ValuePin = GetValuePin();
	UEdGraphPin* FunctionValuePin = GetValueFunction->FindPinChecked(FK2Node_ResourceAccessorHelper::ValuePinName);
	
	// Set the type of the OutRow pin on this expanded mode to match original
	FunctionValuePin->PinType = ValuePin->PinType;
	FunctionValuePin->PinType.PinSubCategoryObject = ValuePin->PinType.PinSubCategoryObject;
	
	CompilerContext.MovePinLinksToIntermediate(*ValuePin, *FunctionValuePin);
	
	// Then
	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *(GetValueFunction->GetThenPin()));

	BreakAllNodeLinks();
}

void UK2Node_GlobalResourceAccessor::UpdateValuePin()
{
	UEdGraphPin* ValuePin = GetValuePin();
	
	UEdGraphPin* KeyPin = GetKeyPin();
	UScriptStruct* StructType = UGameplayResourcesDeveloperSettings::FindValueStruct(FName(*KeyPin->DefaultValue));

	if (StructType == ValuePin->PinType.PinSubCategoryObject)
	{
		return;
	}
	
	if (ValuePin->SubPins.Num() > 0)
	{
		GetSchema()->RecombinePin(ValuePin);
	}
	
	ValuePin->PinType.PinSubCategoryObject = StructType;
	ValuePin->PinType.PinCategory = StructType ? UEdGraphSchema_K2::PC_Struct : UEdGraphSchema_K2::PC_Wildcard;
}

void UK2Node_GlobalResourceAccessor::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();
	
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	
	UEdGraphPin* OuterPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), UEdGraphSchema_K2::PN_Self);
	OuterPin->PinFriendlyName = FText::FromName(FK2Node_ResourceAccessorHelper::ContextPinFriendlyName);
	
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Name, FK2Node_ResourceAccessorHelper::KeyPinName);
	
	CreatePin(ValuePinAsOutput() ? EGPD_Output : EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, FK2Node_ResourceAccessorHelper::ValuePinName);
}

void UK2Node_GlobalResourceAccessor::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	if (Pin == GetKeyPin())
	{
		UpdateValuePin();
	}
}

void UK2Node_GlobalResourceAccessor::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	if (Pin == GetKeyPin())
	{
		UpdateValuePin();
	}
}

FName UK2Node_GetGlobalResource::GetAccessFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UGameplayResourceSubsystem, BPGetValue);
}

FName UK2Node_SetGlobalResource::GetAccessFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UGameplayResourceSubsystem, BPSetValue);
}

FText UK2Node_GetGlobalResource::GetNodeTitle(ENodeTitleType::Type Param) const
{
	return FText::FromString(TEXT("Get Global Resource"));
}

FText UK2Node_SetGlobalResource::GetNodeTitle(ENodeTitleType::Type Param) const
{
	return FText::FromString(TEXT("Set Global Resource"));
}

#undef LOCTEXT_NAMESPACE
