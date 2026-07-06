// © Volga Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "StructUtils/InstancedStruct.h"
#include "GameplayResourcesDeveloperSettings.generated.h"

USTRUCT()
struct FGameplayResourceEntry : public FTableRowBase
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere)
	FText Description;
#endif

	UPROPERTY(EditAnywhere)
	TObjectPtr<UScriptStruct> ResourceType;
};

/**
 * 
 */
UCLASS(Config = Game, DefaultConfig)
class GAMEPLAYRESOURCES_API UGameplayResourcesDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	
	static UDataTable* GetDataTable();
	
	// Can be accessed by UPROPERTY(meta = (GetOptions = "GameplayResources.GameplayResourcesDeveloperSettings.GetKeys"))
	UFUNCTION()
	static TArray<FName> GetKeys();
	
	static UScriptStruct* FindValueStruct(FName Key);
	
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay Resources", meta = (RequiredAssetDataTags = "RowStructure=/Script/GameplayResources.GameplayResourceEntry"))
	TSoftObjectPtr<UDataTable> GlobalResourcesDataTable;
	
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay Resources", meta = (GetKeyOptions = "GetKeys"))
	TMap<FName, FInstancedStruct> DefaultValues;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay Resources")
	bool bDisplayNodeDescription = true;
#endif
	
#if WITH_EDITOR
	
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
	
	static FString GetDescription(FName Key);
#endif
};
