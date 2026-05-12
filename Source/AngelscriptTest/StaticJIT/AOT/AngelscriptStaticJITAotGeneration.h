#pragma once

#include "CoreMinimal.h"

namespace AngelscriptStaticJITAotGeneration
{
	enum class EStaticJITAotGenerationMode : uint8
	{
		Generate,
		Verify,
	};

	struct FStaticJITAotGenerationResult
	{
		bool bSuccess = false;
		TArray<FString> WrittenFiles;
		TArray<FString> StaleFiles;
		FString Error;
	};

	ANGELSCRIPTTEST_API FStaticJITAotGenerationResult Run(EStaticJITAotGenerationMode Mode);
}
