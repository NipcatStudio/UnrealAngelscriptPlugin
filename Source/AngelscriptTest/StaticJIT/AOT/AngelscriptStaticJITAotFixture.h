#pragma once

#include "CoreMinimal.h"

namespace AngelscriptStaticJITAotFixture
{
	ANGELSCRIPTTEST_API const FName& GetModuleName();
	ANGELSCRIPTTEST_API const FString& GetSourceFilename();
	ANGELSCRIPTTEST_API const FString& GetScriptSource();
	ANGELSCRIPTTEST_API const FString& GetEntryDeclaration();
	ANGELSCRIPTTEST_API const FGuid& GetPrecompiledDataGuid();
	ANGELSCRIPTTEST_API int32 GetExpectedEntryResult();
	ANGELSCRIPTTEST_API FString GetGeneratedDirectory();
	ANGELSCRIPTTEST_API FString GetPrecompiledCacheFilename();
	ANGELSCRIPTTEST_API const FString& GetGeneratedSetupInstructions();
	ANGELSCRIPTTEST_API bool IsGeneratedOutputAvailable(FString* OutError = nullptr);
}
