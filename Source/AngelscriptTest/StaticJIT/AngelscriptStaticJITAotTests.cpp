#include "Misc/AutomationTest.h"

#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/StaticJITHeader.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_StaticJIT_AOT_Private
{
	asCScriptFunction* FindEntryFunction(FAngelscriptEngine& Engine)
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(AngelscriptStaticJITAotFixture::GetModuleName().ToString());
		if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
		{
			return nullptr;
		}

		FTCHARToUTF8 EntryDecl(*AngelscriptStaticJITAotFixture::GetEntryDeclaration());
		return static_cast<asCScriptFunction*>(ModuleDesc->ScriptModule->GetFunctionByDecl(EntryDecl.Get()));
	}

	bool LoadAotFixtureFromPrecompiledData(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		FString AvailabilityError;
		if (!Test.TestTrue(TEXT("StaticJIT.AOT generated output and local cache should be available before runtime verification"), AngelscriptStaticJITAotFixture::IsGeneratedOutputAvailable(&AvailabilityError)))
		{
			Test.AddError(AvailabilityError);
			return false;
		}

		FString LoadError;
		if (!Test.TestTrue(TEXT("StaticJIT.AOT should load fixture precompiled data"), Engine.LoadPrecompiledDataForTesting(AngelscriptStaticJITAotFixture::GetPrecompiledCacheFilename(), &LoadError)))
		{
			Test.AddError(LoadError);
			return false;
		}

		FString CompileError;
		if (!Test.TestTrue(TEXT("StaticJIT.AOT should compile fixture from precompiled data"), Engine.CompileLoadedPrecompiledDataForTesting(ECompileType::Initial, &CompileError)))
		{
			Test.AddError(CompileError);
			return false;
		}

		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStaticJITAotGeneratedOutputVerifyTest,
	"Angelscript.TestModule.StaticJIT.AOT.GeneratedOutputVerify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptStaticJITAotGeneratedOutputVerifyTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptStaticJITAotGeneration;
	const FStaticJITAotGenerationResult Result = Run(EStaticJITAotGenerationMode::Verify);
	if (!TestTrue(TEXT("StaticJIT.AOT generated output should match the fixture"), Result.bSuccess))
	{
		AddError(Result.Error);
	}
	return Result.bSuccess;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStaticJITAotRuntimeRegistrationTest,
	"Angelscript.TestModule.StaticJIT.AOT.RuntimeRegistersGeneratedFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptStaticJITAotRuntimeRegistrationTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_StaticJIT_AOT_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	FAngelscriptEngineScope EngineScope(Engine);

	if (!LoadAotFixtureFromPrecompiledData(*this, Engine))
	{
		return false;
	}

	asCScriptFunction* EntryFunction = FindEntryFunction(Engine);
	if (!TestNotNull(TEXT("StaticJIT.AOT should resolve the fixture entry function"), EntryFunction))
	{
		return false;
	}

	uint32 FunctionId = 0;
	if (!TestTrue(TEXT("StaticJIT.AOT should map the fixture function to a StaticJIT function id"), Engine.GetStaticJITFunctionIdForTesting(EntryFunction, FunctionId)))
	{
		return false;
	}

	TestTrue(TEXT("StaticJIT.AOT should register generated C++ for the fixture function id"), FStaticJITTestHooks::IsFunctionRegistered(FunctionId));
	TestTrue(TEXT("StaticJIT.AOT should attach a non-null jitFunction to the fixture entry function"), EntryFunction->jitFunction != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStaticJITAotRuntimeExecutionTest,
	"Angelscript.TestModule.StaticJIT.AOT.RuntimeExecuteUsesGeneratedEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptStaticJITAotRuntimeExecutionTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_StaticJIT_AOT_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	FAngelscriptEngineScope EngineScope(Engine);

	if (!LoadAotFixtureFromPrecompiledData(*this, Engine))
	{
		return false;
	}

	asCScriptFunction* EntryFunction = FindEntryFunction(Engine);
	if (!TestNotNull(TEXT("StaticJIT.AOT should resolve the fixture entry function before execution"), EntryFunction))
	{
		return false;
	}

	uint32 FunctionId = 0;
	if (!TestTrue(TEXT("StaticJIT.AOT should expose the fixture StaticJIT function id before execution"), Engine.GetStaticJITFunctionIdForTesting(EntryFunction, FunctionId)))
	{
		return false;
	}

	FStaticJITTestHooks::ResetEntryCounters();

	asIScriptContext* Context = Engine.GetScriptEngine()->CreateContext();
	if (!TestNotNull(TEXT("StaticJIT.AOT should create an execution context"), Context))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		Context->Release();
	};

	if (!TestEqual(TEXT("StaticJIT.AOT should prepare the fixture entry function"), Context->Prepare(EntryFunction), asSUCCESS))
	{
		return false;
	}

	if (!TestEqual(TEXT("StaticJIT.AOT should execute the fixture entry function"), Context->Execute(), asEXECUTION_FINISHED))
	{
		return false;
	}

	TestEqual(TEXT("StaticJIT.AOT should return the expected fixture result"), static_cast<int32>(Context->GetReturnDWord()), AngelscriptStaticJITAotFixture::GetExpectedEntryResult());
	TestEqual(TEXT("StaticJIT.AOT should mark exactly one generated entry execution"), FStaticJITTestHooks::GetEntryCount(FunctionId), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStaticJITAotMultiEngineSequentialLoadTest,
	"Angelscript.TestModule.StaticJIT.AOT.MultiEngine.SequentialLoadsKeepGeneratedRegistryVisible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptStaticJITAotMultiEngineSequentialLoadTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_StaticJIT_AOT_Private;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		if (!LoadAotFixtureFromPrecompiledData(*this, Engine))
		{
			return false;
		}

		asCScriptFunction* EntryFunction = FindEntryFunction(Engine);
		if (!TestNotNull(*FString::Printf(TEXT("StaticJIT.AOT multi-engine pass %d should resolve the fixture entry function"), Index), EntryFunction))
		{
			return false;
		}

		uint32 FunctionId = 0;
		if (!TestTrue(*FString::Printf(TEXT("StaticJIT.AOT multi-engine pass %d should expose the fixture function id"), Index), Engine.GetStaticJITFunctionIdForTesting(EntryFunction, FunctionId)))
		{
			return false;
		}

		if (!TestTrue(*FString::Printf(TEXT("StaticJIT.AOT multi-engine pass %d should keep generated registry visible"), Index), FStaticJITTestHooks::IsFunctionRegistered(FunctionId)))
		{
			return false;
		}
	}

	return true;
}

#endif
