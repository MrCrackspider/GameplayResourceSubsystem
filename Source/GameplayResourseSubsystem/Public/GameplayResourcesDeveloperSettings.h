// © Volga Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "StructUtils/PropertyBag.h"
#include "GameplayResourcesDeveloperSettings.generated.h"

USTRUCT()
struct FGameplayResourceEntry : public FTableRowBase
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere)
	FText Description;
#endif

	// If true, the values authored in Resources seed this key's runtime value at startup.
	// Otherwise the resource starts unset and the authored values are ignored.
	UPROPERTY(EditAnywhere)
	bool bSetAsDefault = false;

	// The set of named, typed variables this resource key may contain (schema).
	// Values are only used when bSetAsDefault is true.
	UPROPERTY(EditAnywhere)
	FInstancedPropertyBag Resources;
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

	// Schema (property bag layout) declared for a key, used to build node pins. Null if unknown.
	static const UPropertyBag* FindResourceSchema(FName Key);

	// The authored bag for a key (schema + values). Returns nullptr if the key is unknown.
	static const FInstancedPropertyBag* FindResourceBag(FName Key);

	UPROPERTY(Config, EditAnywhere, Category = "Gameplay Resources", meta = (RequiredAssetDataTags = "RowStructure=/Script/GameplayResources.GameplayResourceEntry"))
	TSoftObjectPtr<UDataTable> GlobalResourcesDataTable;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Config, EditAnywhere, Category = "Gameplay Resources")
	bool bDisplayNodeDescription = true;
#endif

#if WITH_EDITOR
	static FString GetDescription(FName Key);
#endif
};
