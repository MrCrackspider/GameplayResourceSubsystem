#include "GameplayResourceSubsystem.h"

UGameplayResourceSubsystem* UGameplayResourceSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
	if (World == nullptr || World->GetGameInstance() == nullptr)
	{
		return nullptr;
	}

	return World->GetGameInstance()->GetSubsystem<UGameplayResourceSubsystem>();
}

void UGameplayResourceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	for (const TTuple<FName, FInstancedStruct>& Value : GetDefault<UGameplayResourcesDeveloperSettings>()->DefaultValues)
	{
		SetValue(Value.Key, Value.Value);
	}
}

bool UGameplayResourceSubsystem::SetValue(FName Key, const FInstancedStruct& InValue)
{
	if (!Key.IsValid() || InValue.GetScriptStruct() == nullptr)
	{
		return false;
	}

	if (UGameplayResourcesDeveloperSettings::FindValueStruct(Key) != InValue.GetScriptStruct())
	{
		return false;
	}
	
	Values.Add(Key, InValue);
	OnValueUpdated.Broadcast(Key);
	return true;
}

void UGameplayResourceSubsystem::ResetValue(FName Key)
{
	if (!Key.IsValid())
	{
		return;
	}
	Values.Remove(Key);
}

void UGameplayResourceSubsystem::ResetGlobalResource(const UObject* WorldContext, FName Key)
{
	UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext);
	if (GRS)
	{
		GRS->ResetValue(Key);
	}
}

const FInstancedStruct* UGameplayResourceSubsystem::GetAsInstancedStruct(FName Key) const
{
	return Values.Find(Key);
}

void UGameplayResourceSubsystem::BPGetValue(const UObject* WorldContext, FName Key, int32& Value)
{
	check(0);
}

void UGameplayResourceSubsystem::BPSetValue(const UObject* WorldContext, FName Key, const int32& Value)
{
	check(0);
}

DEFINE_FUNCTION(UGameplayResourceSubsystem::execBPGetValue)
{
	P_GET_OBJECT(UObject, WorldContext);
	P_GET_PROPERTY(FNameProperty, Key);
	
	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* ValuePtr = Stack.MostRecentPropertyAddress;
	
	P_FINISH;
	
	if(StructProp && ValuePtr)
	{
		P_NATIVE_BEGIN;
		const UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext);
		if (GRS)
		{
			const FInstancedStruct* InstancedStruct =  GRS->GetAsInstancedStruct(Key);
			if (InstancedStruct && InstancedStruct->IsValid() && InstancedStruct->GetScriptStruct()->IsChildOf(StructProp->Struct))
			{
				StructProp->Struct->CopyScriptStruct(ValuePtr, InstancedStruct->GetMemory());
			}
		}
		P_NATIVE_END;
	}
}

DEFINE_FUNCTION(UGameplayResourceSubsystem::execBPSetValue)
{
	P_GET_OBJECT(UObject, WorldContext);
	P_GET_PROPERTY(FNameProperty, Key);
	
	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* ValuePtr = Stack.MostRecentPropertyAddress;

	P_FINISH;
	
	if(StructProp && ValuePtr)
	{
		P_NATIVE_BEGIN;
		UGameplayResourceSubsystem* GRS = UGameplayResourceSubsystem::Get(WorldContext);
		if (GRS)
		{
			const UScriptStruct* StructType = UGameplayResourcesDeveloperSettings::FindValueStruct(Key);
			if (StructType == StructProp->Struct)
			{
				FInstancedStruct InValue;
				InValue.InitializeAs(StructProp->Struct, (const uint8*)ValuePtr);
				GRS->SetValue(Key, InValue);
			}
		}
		P_NATIVE_END;
	}
}
