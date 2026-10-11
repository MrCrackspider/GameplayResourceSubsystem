
#include "K2Node_GlobalResourceAccessor.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "GameplayResource.h"
#include "GameplayResourceFunctionLibrary.h"
#include "GameplayResourcesDeveloperSettings.h"
#include "GameplayResourceSubsystem.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Knot.h"
#include "K2Node_Variable.h"
#include "KismetCompiler.h"
#include "Kismet2/CompilerResultsLog.h"
#include "PropertyBagDetails.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "K2Node_GetSetGlobalResource"

struct FK2Node_ResourceAccessorHelper
{
	static const FName KeyPinName;
	static const FName PropertyNamePinName;
	static const FName ValuePinName;
	static const FName FoundPinName;
	static const FName ResourcePinName;
	static const FName ContextPinFriendlyName;
};

const FName FK2Node_ResourceAccessorHelper::KeyPinName(TEXT("Key"));
const FName FK2Node_ResourceAccessorHelper::PropertyNamePinName(TEXT("PropertyName"));
const FName FK2Node_ResourceAccessorHelper::ValuePinName(TEXT("Value"));
const FName FK2Node_ResourceAccessorHelper::FoundPinName(TEXT("Found"));
const FName FK2Node_ResourceAccessorHelper::ResourcePinName(TEXT("Resource"));
const FName FK2Node_ResourceAccessorHelper::ContextPinFriendlyName(TEXT("WorldContext"));

namespace
{
	// Reads a variable's default FGameplayResource.Key. Prefers the blueprint's generated class CDO
	// (authoritative for member-variable defaults) over whatever scope the node resolves to.
	bool ReadResourceKeyFromVariable(const UK2Node_Variable* VarNode, FName& OutKey)
	{
		if (VarNode == nullptr)
		{
			return false;
		}

		auto TryReadFrom = [&OutKey](const FProperty* Prop, const UObject* Container) -> bool
		{
			const FStructProperty* StructProp = CastField<FStructProperty>(Prop);
			if (StructProp == nullptr || StructProp->Struct != FGameplayResource::StaticStruct() || Container == nullptr)
			{
				return false;
			}
			if (const FGameplayResource* Resource = StructProp->ContainerPtrToValuePtr<FGameplayResource>(Container))
			{
				OutKey = Resource->Key;
				return true;
			}
			return false;
		};

		const FName VarName = VarNode->GetVarName();
		if (const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(VarNode))
		{
			if (UClass* GenClass = Blueprint->GeneratedClass)
			{
				if (TryReadFrom(GenClass->FindPropertyByName(VarName), GenClass->GetDefaultObject()))
				{
					return true;
				}
			}
			if (UClass* SkelClass = Blueprint->SkeletonGeneratedClass)
			{
				if (TryReadFrom(SkelClass->FindPropertyByName(VarName), SkelClass->GetDefaultObject()))
				{
					return true;
				}
			}
		}

		// Fallback: whatever property/scope the node itself resolves to.
		if (const FProperty* Prop = VarNode->GetPropertyForVariable())
		{
			if (const UClass* OwnerClass = Prop->GetOwnerClass())
			{
				if (TryReadFrom(Prop, OwnerClass->GetDefaultObject()))
				{
					return true;
				}
			}
		}

		return false;
	}

	// Follows an FGameplayResource pin's link (through reroute knots) to a source variable and reads
	// that variable's default Key. Returns false when the source isn't a resolvable variable.
	bool TraceResourceKeyFromPin(const UEdGraphPin* ResourcePin, FName& OutKey)
	{
		if (ResourcePin == nullptr || ResourcePin->LinkedTo.Num() == 0)
		{
			return false;
		}

		const UEdGraphPin* SourcePin = ResourcePin->LinkedTo[0];
		while (SourcePin != nullptr)
		{
			UEdGraphNode* SourceNode = SourcePin->GetOwningNodeUnchecked();

			if (const UK2Node_Knot* Knot = Cast<UK2Node_Knot>(SourceNode))
			{
				const UEdGraphPin* KnotInput = Knot->GetInputPin();
				if (KnotInput == nullptr || KnotInput->LinkedTo.Num() == 0)
				{
					return false;
				}
				SourcePin = KnotInput->LinkedTo[0];
				continue;
			}

			if (const UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(SourceNode))
			{
				return ReadResourceKeyFromVariable(VarNode, OutKey);
			}

			return false;
		}

		return false;
	}
}

// ===================================================================================================
// UK2Node_ResourceAccessorBase
// ===================================================================================================

UEdGraphPin* UK2Node_ResourceAccessorBase::GetKeyPin() const
{
	return FindPin(FK2Node_ResourceAccessorHelper::KeyPinName);
}

UEdGraphPin* UK2Node_ResourceAccessorBase::GetFoundPin() const
{
	return FindPin(FK2Node_ResourceAccessorHelper::FoundPinName);
}

FText UK2Node_ResourceAccessorBase::GetMenuCategory() const
{
	return LOCTEXT("GameplayResourceSubsystem", "Gameplay Resource Subsystem");
}

void UK2Node_ResourceAccessorBase::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	Super::GetPinHoverText(Pin, HoverTextOut);
	if (&Pin == GetKeyPin())
	{
		HoverTextOut = UGameplayResourcesDeveloperSettings::GetDescription(FName{Pin.DefaultValue});
	}
}

void UK2Node_ResourceAccessorBase::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool UK2Node_ResourceAccessorBase::TryGetSchemaKey(FName& OutKey) const
{
	if (const UEdGraphPin* KeyPin = GetKeyPin())
	{
		OutKey = FName(*KeyPin->DefaultValue);
		return true;
	}
	return false;
}

bool UK2Node_ResourceAccessorBase::TryGetSchemaKeyFromOldPins(const TArray<UEdGraphPin*>& OldPins, FName& OutKey) const
{
	for (const UEdGraphPin* OldPin : OldPins)
	{
		if (OldPin->PinName == FK2Node_ResourceAccessorHelper::KeyPinName)
		{
			OutKey = FName(*OldPin->DefaultValue);
			return true;
		}
	}
	return false;
}

bool UK2Node_ResourceAccessorBase::IsResyncTriggerPin(const UEdGraphPin* Pin) const
{
	return Pin == GetKeyPin();
}

void UK2Node_ResourceAccessorBase::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

	AllocateExtraFixedPins();

	if (HasKeyPin())
	{
		CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Name, FK2Node_ResourceAccessorHelper::KeyPinName);
	}

	if (HasFoundPin())
	{
		CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, FK2Node_ResourceAccessorHelper::FoundPinName);
	}

	SyncResourcePins();
}

void UK2Node_ResourceAccessorBase::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);

	// AllocateDefaultPins ran with a fresh Key pin; restore the old Key value for display.
	if (HasKeyPin())
	{
		for (const UEdGraphPin* OldPin : OldPins)
		{
			if (OldPin->PinName == FK2Node_ResourceAccessorHelper::KeyPinName)
			{
				if (UEdGraphPin* KeyPin = GetKeyPin())
				{
					KeyPin->DefaultValue = OldPin->DefaultValue;
				}
				break;
			}
		}
	}

	// Rebuild property pins from the pre-reconstruction state so the base can restore their links.
	FName Key;
	if (TryGetSchemaKeyFromOldPins(OldPins, Key))
	{
		RebuildResourcePins(Key);
	}
}

void UK2Node_ResourceAccessorBase::GetResourcePins(TArray<UEdGraphPin*>& OutPins) const
{
	OutPins.Reset();
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin == nullptr || Pin->ParentPin != nullptr || IsFixedPin(*Pin))
		{
			continue;
		}

		OutPins.Add(Pin);
	}
}

bool UK2Node_ResourceAccessorBase::IsFixedPin(const UEdGraphPin& Pin) const
{
	const FName PinName = Pin.PinName;
	return PinName == UEdGraphSchema_K2::PN_Execute
		|| PinName == UEdGraphSchema_K2::PN_Then
		|| PinName == UEdGraphSchema_K2::PN_Self
		|| PinName == FK2Node_ResourceAccessorHelper::KeyPinName
		|| PinName == FK2Node_ResourceAccessorHelper::FoundPinName
		|| PinName == FK2Node_ResourceAccessorHelper::ResourcePinName;
}

bool UK2Node_ResourceAccessorBase::SyncResourcePins()
{
	FName Key;
	if (!TryGetSchemaKey(Key))
	{
		Key = NAME_None;
	}
	return RebuildResourcePins(Key);
}

bool UK2Node_ResourceAccessorBase::RebuildResourcePins(FName Key)
{
	const UPropertyBag* Schema = UGameplayResourcesDeveloperSettings::FindResourceSchema(Key);

	// Build the desired (name, type) list from the schema. Array and Set descs come back as container
	// pins; property bags have no map container type, so map pins can't occur here.
	TArray<TPair<FName, FEdGraphPinType>> Desired;
	if (Schema)
	{
		for (const FPropertyBagPropertyDesc& Desc : Schema->GetPropertyDescs())
		{
			FEdGraphPinType PinType = UE::StructUtils::GetPropertyDescAsPin(Desc);

			// Blueprint pins use PC_Byte (with an enum sub-object) rather than PC_Enum.
			if (PinType.PinCategory == UEdGraphSchema_K2::PC_Enum)
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
			}

			Desired.Emplace(Desc.Name, MoveTemp(PinType));
		}
	}

	// Early-out when the current property pins already match the schema (avoid needless pin churn).
	const EEdGraphPinDirection Direction = ValuePinAsOutput() ? EGPD_Output : EGPD_Input;
	TArray<UEdGraphPin*> ExistingPins;
	GetResourcePins(ExistingPins);

	bool bStructurallyEqual = ExistingPins.Num() == Desired.Num();
	if (bStructurallyEqual)
	{
		for (int32 Index = 0; Index < Desired.Num(); ++Index)
		{
			UEdGraphPin* Existing = ExistingPins[Index];
			if (Existing->PinName != Desired[Index].Key
				|| Existing->Direction != Direction
				|| Existing->PinType != Desired[Index].Value)
			{
				bStructurallyEqual = false;
				break;
			}
		}
	}

	if (bStructurallyEqual)
	{
		return false;
	}

	// Rebuild: drop the current property pins, then recreate them from the schema.
	for (UEdGraphPin* Pin : ExistingPins)
	{
		if (Pin->SubPins.Num() > 0)
		{
			GetSchema()->RecombinePin(Pin);
		}
		Pin->BreakAllPinLinks();
		RemovePin(Pin);
	}

	for (const TPair<FName, FEdGraphPinType>& Entry : Desired)
	{
		CreatePin(Direction, Entry.Value, Entry.Key);
	}

	return true;
}

void UK2Node_ResourceAccessorBase::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	if (IsResyncTriggerPin(Pin) && SyncResourcePins())
	{
		GetGraph()->NotifyGraphChanged();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

void UK2Node_ResourceAccessorBase::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	if (IsResyncTriggerPin(Pin) && SyncResourcePins())
	{
		GetGraph()->NotifyGraphChanged();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

void UK2Node_ResourceAccessorBase::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	// Verify required connections while links are still intact (before BreakAllNodeLinks below).
	if (!VerifyRequiredConnections(CompilerContext))
	{
		return;
	}

	const FName FunctionName = GetAccessFunctionName();
	UClass* FunctionClass = GetAccessFunctionClass();

	TArray<UEdGraphPin*> ResourcePins;
	GetResourcePins(ResourcePins);

	// One accessor call per resource property, chained on exec.
	TArray<UK2Node_CallFunction*> PropertyCalls;
	PropertyCalls.Reserve(ResourcePins.Num());

	const bool bValueIsInput = !ValuePinAsOutput();

	for (UEdGraphPin* ResourcePin : ResourcePins)
	{
		// Blueprint only allows inline defaults on scalar pins, so an unconnected container input carries
		// no value to write. Skip it: the accessor's wildcard argument has no literal form the compiler
		// could emit for a container, and leaving the property untouched is the sane result anyway.
		if (bValueIsInput && ResourcePin->PinType.IsContainer() && ResourcePin->LinkedTo.Num() == 0)
		{
			continue;
		}

		UK2Node_CallFunction* AccessCall = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		AccessCall->FunctionReference.SetExternalMember(FunctionName, FunctionClass);
		AccessCall->AllocateDefaultPins();

		// PropertyName literal identifies which bag entry this call reads/writes.
		AccessCall->FindPinChecked(FK2Node_ResourceAccessorHelper::PropertyNamePinName)->DefaultValue = ResourcePin->PinName.ToString();

		// Force the wildcard Value pin to this property's concrete type, then move links.
		UEdGraphPin* CallValuePin = AccessCall->FindPinChecked(FK2Node_ResourceAccessorHelper::ValuePinName);
		CallValuePin->PinType = ResourcePin->PinType;
		CompilerContext.MovePinLinksToIntermediate(*ResourcePin, *CallValuePin);

		WirePropertyCallSharedInputs(CompilerContext, AccessCall);

		PropertyCalls.Add(AccessCall);
	}

	// Exec chaining.
	if (PropertyCalls.Num() > 0)
	{
		CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *PropertyCalls[0]->GetExecPin());
		for (int32 Index = 0; Index < PropertyCalls.Num() - 1; ++Index)
		{
			PropertyCalls[Index]->GetThenPin()->MakeLinkTo(PropertyCalls[Index + 1]->GetExecPin());
		}
		CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *PropertyCalls.Last()->GetThenPin());
	}
	else
	{
		// No properties: pass exec straight through from upstream to downstream.
		for (UEdGraphPin* UpstreamThen : GetExecPin()->LinkedTo)
		{
			for (UEdGraphPin* DownstreamExec : GetThenPin()->LinkedTo)
			{
				UpstreamThen->MakeLinkTo(DownstreamExec);
			}
		}
	}

	ExpandExtraPins(CompilerContext, SourceGraph);

	BreakAllNodeLinks();
}

// ===================================================================================================
// UK2Node_GlobalResourceAccessor (subsystem)
// ===================================================================================================

void UK2Node_GlobalResourceAccessor::AllocateExtraFixedPins()
{
	UEdGraphPin* OuterPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), UEdGraphSchema_K2::PN_Self);
	OuterPin->PinFriendlyName = FText::FromName(FK2Node_ResourceAccessorHelper::ContextPinFriendlyName);
}

UEdGraphPin* UK2Node_GlobalResourceAccessor::GetContextPin() const
{
	return FindPin(UEdGraphSchema_K2::PN_Self);
}

UClass* UK2Node_GlobalResourceAccessor::GetAccessFunctionClass() const
{
	return UGameplayResourceSubsystem::StaticClass();
}

void UK2Node_GlobalResourceAccessor::WirePropertyCallSharedInputs(class FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* Call)
{
	CompilerContext.CopyPinLinksToIntermediate(*GetContextPin(),
		*Call->FindPinChecked(FK2Node_ResourceAccessorHelper::ContextPinFriendlyName));

	UEdGraphPin* CallKeyPin = Call->FindPinChecked(FK2Node_ResourceAccessorHelper::KeyPinName);
	CallKeyPin->DefaultValue = GetKeyPin()->DefaultValue;
	CompilerContext.CopyPinLinksToIntermediate(*GetKeyPin(), *CallKeyPin);
}

FName UK2Node_GetGlobalResource::GetAccessFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UGameplayResourceSubsystem, BPGetProperty);
}

void UK2Node_GetGlobalResource::ExpandExtraPins(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	UEdGraphPin* FoundPin = GetFoundPin();
	if (FoundPin == nullptr)
	{
		return;
	}

	UK2Node_CallFunction* HasResourceCall = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	HasResourceCall->FunctionReference.SetExternalMember(
		GET_FUNCTION_NAME_CHECKED(UGameplayResourceSubsystem, HasGlobalResource), UGameplayResourceSubsystem::StaticClass());
	HasResourceCall->AllocateDefaultPins();

	CompilerContext.CopyPinLinksToIntermediate(*GetContextPin(),
		*HasResourceCall->FindPinChecked(FK2Node_ResourceAccessorHelper::ContextPinFriendlyName));

	UEdGraphPin* HasKeyPin = HasResourceCall->FindPinChecked(FK2Node_ResourceAccessorHelper::KeyPinName);
	HasKeyPin->DefaultValue = GetKeyPin()->DefaultValue;
	CompilerContext.CopyPinLinksToIntermediate(*GetKeyPin(), *HasKeyPin);

	CompilerContext.MovePinLinksToIntermediate(*FoundPin, *HasResourceCall->GetReturnValuePin());
}

FName UK2Node_SetGlobalResource::GetAccessFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UGameplayResourceSubsystem, BPSetProperty);
}

FText UK2Node_GetGlobalResource::GetNodeTitle(ENodeTitleType::Type Param) const
{
	return FText::FromString(TEXT("Get Global Resource"));
}

FText UK2Node_SetGlobalResource::GetNodeTitle(ENodeTitleType::Type Param) const
{
	return FText::FromString(TEXT("Set Global Resource"));
}

// ===================================================================================================
// UK2Node_GameplayResourceAccessor (struct)
// ===================================================================================================

void UK2Node_GameplayResourceAccessor::AllocateExtraFixedPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, FGameplayResource::StaticStruct(), FK2Node_ResourceAccessorHelper::ResourcePinName);
}

UEdGraphPin* UK2Node_GameplayResourceAccessor::GetResourcePin() const
{
	return FindPin(FK2Node_ResourceAccessorHelper::ResourcePinName);
}

bool UK2Node_GameplayResourceAccessor::TryGetSchemaKey(FName& OutKey) const
{
	return TraceResourceKeyFromPin(GetResourcePin(), OutKey);
}

bool UK2Node_GameplayResourceAccessor::TryGetSchemaKeyFromOldPins(const TArray<UEdGraphPin*>& OldPins, FName& OutKey) const
{
	for (const UEdGraphPin* OldPin : OldPins)
	{
		if (OldPin->PinName == FK2Node_ResourceAccessorHelper::ResourcePinName)
		{
			return TraceResourceKeyFromPin(OldPin, OutKey);
		}
	}
	return false;
}

bool UK2Node_GameplayResourceAccessor::IsResyncTriggerPin(const UEdGraphPin* Pin) const
{
	return Pin == GetResourcePin();
}

bool UK2Node_GameplayResourceAccessor::VerifyRequiredConnections(class FKismetCompilerContext& CompilerContext) const
{
	const UEdGraphPin* ResourcePin = GetResourcePin();
	if (ResourcePin == nullptr || ResourcePin->LinkedTo.Num() == 0)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("ResourceRequired", "@@ requires its Resource input to be connected.").ToString(), this);
		return false;
	}
	return true;
}

UClass* UK2Node_GameplayResourceAccessor::GetAccessFunctionClass() const
{
	return UGameplayResourceFunctionLibrary::StaticClass();
}

void UK2Node_GameplayResourceAccessor::WirePropertyCallSharedInputs(class FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* Call)
{
	CompilerContext.CopyPinLinksToIntermediate(*GetResourcePin(),
		*Call->FindPinChecked(FK2Node_ResourceAccessorHelper::ResourcePinName));
}

FName UK2Node_GetGameplayResource::GetAccessFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UGameplayResourceFunctionLibrary, BPGetResourceProperty);
}

FName UK2Node_SetGameplayResource::GetAccessFunctionName() const
{
	return GET_FUNCTION_NAME_CHECKED(UGameplayResourceFunctionLibrary, BPSetResourceProperty);
}

FText UK2Node_GetGameplayResource::GetNodeTitle(ENodeTitleType::Type Param) const
{
	return FText::FromString(TEXT("Get Gameplay Resource"));
}

FText UK2Node_SetGameplayResource::GetNodeTitle(ENodeTitleType::Type Param) const
{
	return FText::FromString(TEXT("Set Gameplay Resource"));
}

#undef LOCTEXT_NAMESPACE
