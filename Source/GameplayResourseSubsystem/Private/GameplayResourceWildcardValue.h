// © Volga Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

namespace UE::GameplayResources
{
	// Scratch size used by the Set thunks when the target property can't be resolved (the schema changed
	// under an already-compiled Blueprint). Comfortably covers every Blueprint-expressible scalar literal.
	constexpr int32 UnresolvedLiteralStorageSize = 256;

	/**
	 * True when a value can be copied between two properties byte-for-byte.
	 *
	 * Compared structurally rather than by FProperty class, because a Blueprint pin and the property bag
	 * slot it mirrors don't always use the same class for the same type: an enum reaches the VM as an
	 * FByteProperty on the pin side while the bag stores an FEnumProperty. Containers recurse into their
	 * element types, so TArray<int32> is not mistaken for TArray<FString> (both are one FScriptArray wide).
	 */
	inline bool ArePropertiesLayoutCompatible(const FProperty* A, const FProperty* B)
	{
		if (A == nullptr || B == nullptr)
		{
			return false;
		}

		if (const FArrayProperty* ArrayA = CastField<FArrayProperty>(A))
		{
			const FArrayProperty* ArrayB = CastField<FArrayProperty>(B);
			return ArrayB != nullptr && ArePropertiesLayoutCompatible(ArrayA->Inner, ArrayB->Inner);
		}

		if (const FSetProperty* SetA = CastField<FSetProperty>(A))
		{
			const FSetProperty* SetB = CastField<FSetProperty>(B);
			return SetB != nullptr && ArePropertiesLayoutCompatible(SetA->ElementProp, SetB->ElementProp);
		}

		if (const FMapProperty* MapA = CastField<FMapProperty>(A))
		{
			const FMapProperty* MapB = CastField<FMapProperty>(B);
			return MapB != nullptr
				&& ArePropertiesLayoutCompatible(MapA->KeyProp, MapB->KeyProp)
				&& ArePropertiesLayoutCompatible(MapA->ValueProp, MapB->ValueProp);
		}

		// Enums: a byte property on one side and an enum property on the other describe the same byte.
		const bool bAIsEnumLike = A->IsA<FByteProperty>() || A->IsA<FEnumProperty>();
		const bool bBIsEnumLike = B->IsA<FByteProperty>() || B->IsA<FEnumProperty>();
		if (bAIsEnumLike && bBIsEnumLike)
		{
			return A->GetElementSize() == B->GetElementSize();
		}

		if (const FStructProperty* StructA = CastField<FStructProperty>(A))
		{
			const FStructProperty* StructB = CastField<FStructProperty>(B);
			return StructB != nullptr && StructA->Struct == StructB->Struct;
		}

		if (const FObjectPropertyBase* ObjectA = CastField<FObjectPropertyBase>(A))
		{
			const FObjectPropertyBase* ObjectB = CastField<FObjectPropertyBase>(B);
			return ObjectB != nullptr
				&& A->GetClass() == B->GetClass()
				&& ObjectA->PropertyClass == ObjectB->PropertyClass;
		}

		return A->GetClass() == B->GetClass() && A->GetElementSize() == B->GetElementSize();
	}
}
