#pragma once

#include "CoreMinimal.h"
#include "GameplayResourcesDeveloperSettings.h"
#include "UObject/UnrealType.h"
#include "StructUtils/InstancedStruct.h"
#include "Runtime/Engine/Public/Subsystems/WorldSubsystem.h"
#include "GameplayResourceSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FGlobalResourceReference
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (GetOptions = "GameplayResources.GameplayResourcesDeveloperSettings.GetKeys"))
	FName Key;
};


UCLASS()
class GAMEPLAYRESOURCES_API UGameplayResourceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	
	static UGameplayResourceSubsystem* Get(const UObject* WorldContext);
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	template<typename T>
	const T* GetValue(FName Key) const;
	
	template<typename T>
	bool SetValue(FName Key, const T* Value) const;
	
	const FInstancedStruct* GetAsInstancedStruct(FName Key) const;
	
	bool SetValue(FName Key, const FInstancedStruct& InValue);
	
	void ResetValue(FName Key);
	
	UFUNCTION(BlueprintCallable, meta = (DefaultToSelf = "WorldContext", WorldContex = "WorldContext"))
	static void ResetGlobalResource(const UObject* WorldContext, UPARAM(meta = (GetOptions = "GameplayResources.GameplayResourcesDeveloperSettings.GetKeys")) FName Key);
	
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = "Value", BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContext"))
	static void BPGetValue(const UObject* WorldContext, FName Key, int32& Value);
	
	DECLARE_FUNCTION(execBPGetValue);
	
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = "Value", BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContext"))
	static void BPSetValue(const UObject* WorldContext, FName Key, const int32& Value);
	
	DECLARE_FUNCTION(execBPSetValue);
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnValueUpdated, FName)
	
	FOnValueUpdated OnValueUpdated;

private:
	
	TMap<FName, FInstancedStruct> Values;
};

template <typename T>
const T* UGameplayResourceSubsystem::GetValue(FName Key) const
{
	static_assert(TIsDerivedFrom<T, UStruct>::IsDerived, "T must be a UStruct type");

	const FInstancedStruct* Found = GetAsInstancedStruct(Key);
	if (!Found)
	{
		return nullptr;
	}

	const UScriptStruct* ExpectedStruct = Found->GetScriptStruct();
	const UScriptStruct* ActualStruct = T::StaticStruct(); 

	if (ActualStruct != ExpectedStruct)
	{
		return nullptr;
	}

	return Found->GetPtr<T>();
}

template <typename T>
bool UGameplayResourceSubsystem::SetValue(FName Key, const T* Value) const
{
	static_assert(TIsDerivedFrom<T, UStruct>::IsDerived, "T must be a UStruct type");

	if (Value == nullptr || Key.IsNone())
	{
		return false;
	}
	
	return SetValue(Key, FInstancedStruct::Make<T>(*Value));
}
