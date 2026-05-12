#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace AngelscriptStaticJITAotFixture
{
	namespace
	{
		bool SetGeneratedOutputError(FString* OutError, const FString& Detail)
		{
			if (OutError != nullptr)
			{
				*OutError = FString::Printf(
					TEXT("%s\nStaticJIT AOT test artifacts are generated locally; run the setup workflow before running the AOT tests:\n%s"),
					*Detail,
					*GetGeneratedSetupInstructions());
			}
			return false;
		}
	}

	const FName& GetModuleName()
	{
		static const FName ModuleName(TEXT("ASStaticJITAotFixture"));
		return ModuleName;
	}

	const FString& GetSourceFilename()
	{
		static const FString SourceFilename(TEXT("StaticJIT/AOT/Fixtures/StaticJITAotFixture.as"));
		return SourceFilename;
	}

	const FString& GetScriptSource()
	{
		static const FString ScriptSource =
			TEXT("int AddForAOT(int Value)\n")
			TEXT("{\n")
			TEXT("\treturn Value + 7;\n")
			TEXT("}\n")
			TEXT("\n")
			TEXT("int Entry()\n")
			TEXT("{\n")
			TEXT("\treturn AddForAOT(35);\n")
			TEXT("}\n");
		return ScriptSource;
	}

	const FString& GetEntryDeclaration()
	{
		static const FString EntryDeclaration(TEXT("int Entry()"));
		return EntryDeclaration;
	}

	const FGuid& GetPrecompiledDataGuid()
	{
		static const FGuid Guid(0x2f90f2a1, 0x6df34d23, 0x9bc4474a, 0xa05de08c);
		return Guid;
	}

	int32 GetExpectedEntryResult()
	{
		return 42;
	}

	FString GetGeneratedDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Plugins/Angelscript/Source/AngelscriptTest/StaticJIT/AOT/Generated")));
	}

	FString GetPrecompiledCacheFilename()
	{
		return FPaths::Combine(GetGeneratedDirectory(), TEXT("StaticJITAotFixture.Cache"));
	}

	const FString& GetGeneratedSetupInstructions()
	{
		static const FString Instructions =
			TEXT("powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\\RunBuild.ps1 -Label staticjit-aot-build -TimeoutMs 180000\n")
			TEXT("powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\\RunCommandlet.ps1 -Commandlet AngelscriptStaticJITAotTest -Label staticjit-aot-generate -TimeoutMs 600000 -ExtraArgs \"-Mode=Generate\"\n")
			TEXT("powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\\RunBuild.ps1 -Label staticjit-aot-generated -TimeoutMs 180000\n")
			TEXT("powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\\RunTests.ps1 -TestPrefix \"Angelscript.TestModule.StaticJIT.AOT\" -Label staticjit-aot -TimeoutMs 600000");
		return Instructions;
	}

	bool IsGeneratedOutputAvailable(FString* OutError)
	{
		const FString GeneratedDirectory = GetGeneratedDirectory();
		if (!IFileManager::Get().DirectoryExists(*GeneratedDirectory))
		{
			return SetGeneratedOutputError(OutError, FString::Printf(TEXT("StaticJIT AOT generated directory does not exist: %s"), *GeneratedDirectory));
		}

		const TArray<FString> RequiredGeneratedFiles =
		{
			TEXT("ASStaticJITAotFixture.as.jit.hpp"),
			TEXT("AngelscriptJitCode_0.jit.cpp"),
			TEXT("AngelscriptJitInfo.jit.cpp"),
		};

		for (const FString& RequiredGeneratedFile : RequiredGeneratedFiles)
		{
			const FString GeneratedFile = FPaths::Combine(GeneratedDirectory, RequiredGeneratedFile);
			if (!IFileManager::Get().FileExists(*GeneratedFile))
			{
				return SetGeneratedOutputError(OutError, FString::Printf(TEXT("StaticJIT AOT generated file does not exist: %s"), *GeneratedFile));
			}
		}

		const FString PrecompiledCache = GetPrecompiledCacheFilename();
		if (!IFileManager::Get().FileExists(*PrecompiledCache))
		{
			return SetGeneratedOutputError(OutError, FString::Printf(TEXT("StaticJIT AOT local precompiled cache does not exist: %s"), *PrecompiledCache));
		}

		return true;
	}
}
