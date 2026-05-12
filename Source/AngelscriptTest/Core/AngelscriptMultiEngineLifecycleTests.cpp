#include "AngelscriptEngine.h"
#include "AngelscriptBinds.h"
#include "CQTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "Testing/AngelscriptBindExecutionObservation.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_module.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FAngelscriptMultiEngineTestAccess
{
	static void DestroyGlobalEngine()
	{
		FAngelscriptEngine::DestroyGlobal();
	}

	static FAngelscriptEngine* GetGlobalEngine()
	{
		return FAngelscriptEngine::TryGetGlobalEngine();
	}

	static FString MakeModuleName(const FAngelscriptEngine& Engine, const FString& ModuleName)
	{
		return Engine.MakeModuleName(ModuleName);
	}

	static asIScriptModule* CreateNamedModule(FAngelscriptEngine& Engine, const FString& ModuleName)
	{
		return Engine.Engine->GetModule(TCHAR_TO_ANSI(*Engine.MakeModuleName(ModuleName)), asGM_ALWAYS_CREATE);
	}

	static asIScriptModule* FindNamedModule(FAngelscriptEngine& Engine, const FString& ModuleName)
	{
		return Engine.Engine->GetModule(TCHAR_TO_ANSI(*Engine.MakeModuleName(ModuleName)), asGM_ONLY_IF_EXISTS);
	}

	static void TrackNamedModule(FAngelscriptEngine& Engine, const FString& ModuleName, asIScriptModule* ScriptModule)
	{
		TSharedRef<FAngelscriptModuleDesc> ModuleDesc = MakeShared<FAngelscriptModuleDesc>();
		ModuleDesc->ModuleName = ModuleName;
		ModuleDesc->ScriptModule = static_cast<asCModule*>(ScriptModule);
		Engine.ActiveModules.Add(Engine.MakeModuleName(ModuleName), ModuleDesc);
		Engine.ModulesByScriptModule.Add(ScriptModule, ModuleDesc);
	}

	static int32 GetActiveParticipants(const FAngelscriptEngine& Engine)
	{
		return Engine.GetActiveParticipantsForTesting();
	}

	static int32 GetActiveCloneCount(const FAngelscriptEngine& Engine)
	{
		return Engine.GetActiveCloneCountForTesting();
	}

	static int32 GetLocalPooledContextCount(asIScriptEngine* ScriptEngine)
	{
		return FAngelscriptEngine::GetLocalPooledContextCountForTesting(ScriptEngine);
	}
};

namespace AngelscriptTest_Core_AngelscriptMultiEngineLifecycleTests_Private
{

struct FMultiEngineContextStackGuard
{
	TArray<FAngelscriptEngine*> SavedStack;
	FMultiEngineContextStackGuard()
	{
		SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
	}
	~FMultiEngineContextStackGuard()
	{
		FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
	}
};

static void ResetToIsolatedEngineState()
{
	if (FAngelscriptEngine::IsInitialized())
	{
		FAngelscriptMultiEngineTestAccess::DestroyGlobalEngine();
	}
}

static FName MakeUniqueStartupBindName(const TCHAR* Prefix)
{
	return FName(*FString::Printf(TEXT("%s.%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
}

bool RunCloneModuleIsolation(FAutomationTestBase& Test)
{
	ResetToIsolatedEngineState();

	const FString ModuleName = TEXT("Tests.SharedModule");
	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> PrimaryEngine = FAngelscriptEngine::CreateUncompiled(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> CloneA = FAngelscriptEngine::CreateCloneFrom(*PrimaryEngine, Config);
	TUniquePtr<FAngelscriptEngine> CloneB = FAngelscriptEngine::CreateCloneFrom(*PrimaryEngine, Config);

	if (!Test.TestNotNull(TEXT("MultiEngine.CloneModuleIsolation should create primary engine"), PrimaryEngine.Get())
		|| !Test.TestNotNull(TEXT("MultiEngine.CloneModuleIsolation should create first clone"), CloneA.Get())
		|| !Test.TestNotNull(TEXT("MultiEngine.CloneModuleIsolation should create second clone"), CloneB.Get()))
	{
		return false;
	}

	asIScriptModule* CloneAModule = FAngelscriptMultiEngineTestAccess::CreateNamedModule(*CloneA, ModuleName);
	asIScriptModule* CloneBModule = FAngelscriptMultiEngineTestAccess::CreateNamedModule(*CloneB, ModuleName);
	FAngelscriptMultiEngineTestAccess::TrackNamedModule(*CloneA, ModuleName, CloneAModule);
	FAngelscriptMultiEngineTestAccess::TrackNamedModule(*CloneB, ModuleName, CloneBModule);

	Test.TestNotNull(TEXT("MultiEngine.CloneModuleIsolation should create the first clone module"), CloneAModule);
	Test.TestNotNull(TEXT("MultiEngine.CloneModuleIsolation should create the second clone module"), CloneBModule);
	Test.TestTrue(TEXT("MultiEngine.CloneModuleIsolation should give Clone A an internal module name"), FAngelscriptMultiEngineTestAccess::MakeModuleName(*CloneA, ModuleName).Contains(TEXT("::")));
	Test.TestTrue(TEXT("MultiEngine.CloneModuleIsolation should give Clone B an internal module name"), FAngelscriptMultiEngineTestAccess::MakeModuleName(*CloneB, ModuleName).Contains(TEXT("::")));
	Test.TestNotEqual(TEXT("MultiEngine.CloneModuleIsolation should isolate internal module names per clone"), FAngelscriptMultiEngineTestAccess::MakeModuleName(*CloneA, ModuleName), FAngelscriptMultiEngineTestAccess::MakeModuleName(*CloneB, ModuleName));
	Test.TestTrue(TEXT("MultiEngine.CloneModuleIsolation should keep external lookup working for Clone A"), CloneA->GetModuleByModuleName(ModuleName).IsValid());
	Test.TestTrue(TEXT("MultiEngine.CloneModuleIsolation should keep external lookup working for Clone B"), CloneB->GetModuleByModuleName(ModuleName).IsValid());
	return Test.TestTrue(TEXT("MultiEngine.CloneModuleIsolation should create distinct underlying script modules"), CloneAModule != CloneBModule);
}

bool RunCloneDestroyDoesNotAffectPrimary(FAutomationTestBase& Test)
{
	ResetToIsolatedEngineState();

	const FString ModuleName = TEXT("Tests.SharedModule");
	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> PrimaryEngine = FAngelscriptEngine::CreateUncompiled(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> CloneEngine = FAngelscriptEngine::CreateCloneFrom(*PrimaryEngine, Config);

	if (!Test.TestNotNull(TEXT("MultiEngine.CloneDestroyDoesNotAffectPrimary should create primary engine"), PrimaryEngine.Get())
		|| !Test.TestNotNull(TEXT("MultiEngine.CloneDestroyDoesNotAffectPrimary should create clone engine"), CloneEngine.Get()))
	{
		return false;
	}

	asIScriptModule* PrimaryModule = FAngelscriptMultiEngineTestAccess::CreateNamedModule(*PrimaryEngine, ModuleName);
	asIScriptModule* CloneModule = FAngelscriptMultiEngineTestAccess::CreateNamedModule(*CloneEngine, ModuleName);
	FAngelscriptMultiEngineTestAccess::TrackNamedModule(*PrimaryEngine, ModuleName, PrimaryModule);
	FAngelscriptMultiEngineTestAccess::TrackNamedModule(*CloneEngine, ModuleName, CloneModule);

	if (!Test.TestNotNull(TEXT("MultiEngine.CloneDestroyDoesNotAffectPrimary should create the primary module"), PrimaryModule)
		|| !Test.TestNotNull(TEXT("MultiEngine.CloneDestroyDoesNotAffectPrimary should create the clone module"), CloneModule))
	{
		return false;
	}

	CloneEngine.Reset();

	Test.TestTrue(TEXT("MultiEngine.CloneDestroyDoesNotAffectPrimary should keep the primary module descriptor registered"), PrimaryEngine->GetModuleByModuleName(ModuleName).IsValid());
	return Test.TestNotNull(TEXT("MultiEngine.CloneDestroyDoesNotAffectPrimary should keep the primary underlying script module alive"), FAngelscriptMultiEngineTestAccess::FindNamedModule(*PrimaryEngine, ModuleName));
}

bool RunCloneKeepsSharedStateAlive(FAutomationTestBase& Test)
{
	ResetToIsolatedEngineState();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> SourceEngine = FAngelscriptEngine::CreateUncompiled(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> CloneEngine = FAngelscriptEngine::CreateCloneFrom(*SourceEngine, Config);

	if (!Test.TestNotNull(TEXT("MultiEngine.CloneKeepsSharedStateAlive should create a source engine"), SourceEngine.Get())
		|| !Test.TestNotNull(TEXT("MultiEngine.CloneKeepsSharedStateAlive should create a clone engine"), CloneEngine.Get()))
	{
		return false;
	}

	int32 RegisteredTypeCountBeforeDestroy = 0;
	{
		FAngelscriptEngineScope SourceScope(*SourceEngine);
		RegisteredTypeCountBeforeDestroy = FAngelscriptType::GetTypes().Num();
	}
	if (!Test.TestTrue(TEXT("MultiEngine.CloneKeepsSharedStateAlive should start with registered types"), RegisteredTypeCountBeforeDestroy > 0))
	{
		return false;
	}

	Test.AddExpectedError(TEXT("Rejecting Full engine shutdown while Clone instances still reference shared state"), EAutomationExpectedErrorFlags::Contains, 1);
	SourceEngine.Reset();

	{
		FAngelscriptEngineScope CloneScope(*CloneEngine);
		Test.TestTrue(TEXT("MultiEngine.CloneKeepsSharedStateAlive should keep shared type registrations alive while the clone remains"), FAngelscriptType::GetTypes().Num() > 0);
	}
	return Test.TestNotNull(TEXT("MultiEngine.CloneKeepsSharedStateAlive should keep the shared script engine reachable from the clone"), CloneEngine->GetScriptEngine());
}

bool RunDestroyingSourceWhileCloneAliveIsRejected(FAutomationTestBase& Test)
{
	ResetToIsolatedEngineState();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> SourceEngine = FAngelscriptEngine::CreateUncompiled(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> CloneEngine = FAngelscriptEngine::CreateCloneFrom(*SourceEngine, Config);

	if (!Test.TestNotNull(TEXT("MultiEngine.DestroyingSourceWhileCloneAliveIsRejected should create a source engine"), SourceEngine.Get())
		|| !Test.TestNotNull(TEXT("MultiEngine.DestroyingSourceWhileCloneAliveIsRejected should create a clone engine"), CloneEngine.Get()))
	{
		return false;
	}

	Test.AddExpectedError(TEXT("Rejecting Full engine shutdown while Clone instances still reference shared state"), EAutomationExpectedErrorFlags::Contains, 1);
	SourceEngine.Reset();

	Test.TestNull(TEXT("MultiEngine.DestroyingSourceWhileCloneAliveIsRejected should clear the clone's source-engine link once the source owner is gone"), CloneEngine->GetSourceEngine());
	return Test.TestNotNull(TEXT("MultiEngine.DestroyingSourceWhileCloneAliveIsRejected should leave the clone with a usable shared script engine reference"), CloneEngine->GetScriptEngine());
}

bool RunDeferredSharedStateReleasePurgesLocalContextPool(FAutomationTestBase& Test)
{
	ResetToIsolatedEngineState();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> SourceEngine = FAngelscriptEngine::CreateUncompiled(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> CloneEngine = FAngelscriptEngine::CreateCloneFrom(*SourceEngine, Config);

	if (!Test.TestNotNull(TEXT("MultiEngine.DeferredSharedStateReleasePurgesLocalContextPool should create a source engine"), SourceEngine.Get())
		|| !Test.TestNotNull(TEXT("MultiEngine.DeferredSharedStateReleasePurgesLocalContextPool should create a clone engine"), CloneEngine.Get()))
	{
		return false;
	}

	asIScriptEngine* SharedScriptEngine = SourceEngine->GetScriptEngine();
	if (!Test.TestNotNull(TEXT("MultiEngine.DeferredSharedStateReleasePurgesLocalContextPool should resolve the shared script engine"), SharedScriptEngine))
	{
		return false;
	}

	{
		FAngelscriptEngineScope SourceScope(*SourceEngine);
		{
			FAngelscriptPooledContextBase SeedContext;
		}
	}

	if (!Test.TestTrue(
		TEXT("MultiEngine.DeferredSharedStateReleasePurgesLocalContextPool should place the seeded context into the local pool"),
		FAngelscriptMultiEngineTestAccess::GetLocalPooledContextCount(SharedScriptEngine) > 0))
	{
		return false;
	}

	Test.AddExpectedError(TEXT("Rejecting Full engine shutdown while Clone instances still reference shared state"), EAutomationExpectedErrorFlags::Contains, 1);
	SourceEngine.Reset();

	if (!Test.TestTrue(
		TEXT("MultiEngine.DeferredSharedStateReleasePurgesLocalContextPool should keep the pooled shared context alive while the clone still references shared state"),
		FAngelscriptMultiEngineTestAccess::GetLocalPooledContextCount(SharedScriptEngine) > 0))
	{
		return false;
	}

	CloneEngine.Reset();
	return Test.TestEqual(
		TEXT("MultiEngine.DeferredSharedStateReleasePurgesLocalContextPool should purge pooled contexts when the deferred shared state is finally released"),
		FAngelscriptMultiEngineTestAccess::GetLocalPooledContextCount(SharedScriptEngine),
		0);
}

bool RunCloneHonorsInjectedDependencies(FAutomationTestBase& Test)
{
	ResetToIsolatedEngineState();

	FAngelscriptEngineConfig Config;
	Config.bIsEditor = true;

	const FAngelscriptEngineDependencies SourceDependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> SourceEngine = FAngelscriptEngine::CreateUncompiledWithMode(Config, SourceDependencies, EAngelscriptEngineCreationMode::Full);
	if (!Test.TestNotNull(TEXT("MultiEngine.CloneHonorsInjectedDependencies should create a source testing full engine"), SourceEngine.Get()))
	{
		return false;
	}

	FAngelscriptEngineScope GlobalScope(*SourceEngine);

	bool bMakeDirectoryCalled = false;
	FString CreatedPath;

	FAngelscriptEngineDependencies InjectedDependencies;
	InjectedDependencies.GetProjectDir = []()
	{
		return FString(TEXT("C:/InjectedCloneProject"));
	};
	InjectedDependencies.ConvertRelativePathToFull = [](const FString& Path)
	{
		return Path;
	};
	InjectedDependencies.DirectoryExists = [](const FString& Path)
	{
		return false;
	};
	InjectedDependencies.MakeDirectory = [&bMakeDirectoryCalled, &CreatedPath](const FString& Path, bool bTree)
	{
		bMakeDirectoryCalled = true;
		CreatedPath = Path;
		return true;
	};
	InjectedDependencies.GetEnabledPluginScriptRoots = []()
	{
		return TArray<FString>();
	};

	TUniquePtr<FAngelscriptEngine> CloneEngine = FAngelscriptEngine::CreateUncompiledWithMode(Config, InjectedDependencies, EAngelscriptEngineCreationMode::Clone);
	if (!Test.TestNotNull(TEXT("MultiEngine.CloneHonorsInjectedDependencies should create a clone engine"), CloneEngine.Get()))
	{
		return false;
	}

	TArray<FString> Roots = CloneEngine->DiscoverScriptRoots(false);
	Test.TestTrue(TEXT("MultiEngine.CloneHonorsInjectedDependencies should honor the injected editor filesystem hooks"), bMakeDirectoryCalled);
	if (Roots.Num() > 0)
	{
		Test.TestEqual(TEXT("MultiEngine.CloneHonorsInjectedDependencies should use the injected project root"), Roots[0], FString(TEXT("C:/InjectedCloneProject/Script")));
	}
	return Test.TestEqual(TEXT("MultiEngine.CloneHonorsInjectedDependencies should create the expected injected clone project root path"), CreatedPath, FString(TEXT("C:/InjectedCloneProject/Script")));
}

bool RunStartupBindObservationFullCreate(FAutomationTestBase& Test)
{
	ResetToIsolatedEngineState();

	const FName FirstBindName = MakeUniqueStartupBindName(TEXT("Automation.StartupBind.Full.First"));
	const FName SecondBindName = MakeUniqueStartupBindName(TEXT("Automation.StartupBind.Full.Second"));
	FAngelscriptBinds::FBind FirstBind(FirstBindName, -25, []() {});
	FAngelscriptBinds::FBind SecondBind(SecondBindName, 25, []() {});

	FAngelscriptBindExecutionObservation::Reset();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptEngine::CreateUncompiled(Config, Dependencies);
	if (!Test.TestNotNull(TEXT("MultiEngine.StartupBindObservation.FullCreateRecordsOrderedBinds should create a full engine"), Engine.Get()))
	{
		return false;
	}

	const FAngelscriptBindExecutionSnapshot Snapshot = FAngelscriptBindExecutionObservation::GetLastSnapshot();
	if (!Test.TestEqual(TEXT("MultiEngine.StartupBindObservation.FullCreateRecordsOrderedBinds should observe a single startup bind pass"), FAngelscriptBindExecutionObservation::GetInvocationCount(), 1))
	{
		return false;
	}

	const int32 FirstIndex = Snapshot.ExecutedBindNames.IndexOfByKey(FirstBindName);
	const int32 SecondIndex = Snapshot.ExecutedBindNames.IndexOfByKey(SecondBindName);
	if (!Test.TestTrue(TEXT("MultiEngine.StartupBindObservation.FullCreateRecordsOrderedBinds should observe the first named bind"), FirstIndex != INDEX_NONE)
		|| !Test.TestTrue(TEXT("MultiEngine.StartupBindObservation.FullCreateRecordsOrderedBinds should observe the second named bind"), SecondIndex != INDEX_NONE))
	{
		return false;
	}

	return Test.TestTrue(TEXT("MultiEngine.StartupBindObservation.FullCreateRecordsOrderedBinds should preserve bind order in the observed startup pass"), FirstIndex < SecondIndex);
}

bool RunStartupBindObservationCloneCreate(FAutomationTestBase& Test)
{
	ResetToIsolatedEngineState();

	const FName BindName = MakeUniqueStartupBindName(TEXT("Automation.StartupBind.Clone.Named"));
	FAngelscriptBinds::FBind NamedBind(BindName, []() {});

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> SourceEngine = FAngelscriptEngine::CreateUncompiled(Config, Dependencies);
	if (!Test.TestNotNull(TEXT("MultiEngine.StartupBindObservation.CloneCreateDoesNotReplayBinds should create a source engine"), SourceEngine.Get()))
	{
		return false;
	}

	FAngelscriptBindExecutionObservation::Reset();
	TUniquePtr<FAngelscriptEngine> CloneEngine = FAngelscriptEngine::CreateCloneFrom(*SourceEngine, Config);
	if (!Test.TestNotNull(TEXT("MultiEngine.StartupBindObservation.CloneCreateDoesNotReplayBinds should create a clone engine"), CloneEngine.Get()))
	{
		return false;
	}

	if (!Test.TestEqual(TEXT("MultiEngine.StartupBindObservation.CloneCreateDoesNotReplayBinds should not observe a fresh startup bind pass for clone creation"), FAngelscriptBindExecutionObservation::GetInvocationCount(), 0))
	{
		return false;
	}

	const FAngelscriptBindExecutionSnapshot Snapshot = FAngelscriptBindExecutionObservation::GetLastSnapshot();
	return Test.TestEqual(TEXT("MultiEngine.StartupBindObservation.CloneCreateDoesNotReplayBinds should not append any executed bind names during clone creation"), Snapshot.ExecutedBindNames.Num(), 0);
}

bool RunStartupBindObservationCreateForTestingClone(FAutomationTestBase& Test)
{
	ResetToIsolatedEngineState();

	const FName BindName = MakeUniqueStartupBindName(TEXT("Automation.StartupBind.CreateForTesting.Clone.Named"));
	FAngelscriptBinds::FBind NamedBind(BindName, []() {});

	FAngelscriptEngineConfig Config;
	Config.bIsEditor = true;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> SourceEngine = FAngelscriptEngine::CreateUncompiledWithMode(Config, Dependencies, EAngelscriptEngineCreationMode::Full);
	if (!Test.TestNotNull(TEXT("MultiEngine.StartupBindObservation.CreateForTestingCloneDoesNotReplayBinds should create a source full engine"), SourceEngine.Get()))
	{
		return false;
	}

	FAngelscriptEngineScope GlobalScope(*SourceEngine);
	FAngelscriptBindExecutionObservation::Reset();

	TUniquePtr<FAngelscriptEngine> TestEngine = FAngelscriptEngine::CreateUncompiledWithMode(Config, Dependencies, EAngelscriptEngineCreationMode::Clone);
	if (!Test.TestNotNull(TEXT("MultiEngine.StartupBindObservation.CreateForTestingCloneDoesNotReplayBinds should create a clone testing engine"), TestEngine.Get()))
	{
		return false;
	}

	Test.TestEqual(TEXT("MultiEngine.StartupBindObservation.CreateForTestingCloneDoesNotReplayBinds should choose clone mode when a global source engine exists"), TestEngine->GetCreationMode(), EAngelscriptEngineCreationMode::Clone);
	if (!Test.TestEqual(TEXT("MultiEngine.StartupBindObservation.CreateForTestingCloneDoesNotReplayBinds should not observe a fresh bind pass"), FAngelscriptBindExecutionObservation::GetInvocationCount(), 0))
	{
		return false;
	}

	const FAngelscriptBindExecutionSnapshot Snapshot = FAngelscriptBindExecutionObservation::GetLastSnapshot();
	return Test.TestEqual(TEXT("MultiEngine.StartupBindObservation.CreateForTestingCloneDoesNotReplayBinds should keep the observed bind list empty"), Snapshot.ExecutedBindNames.Num(), 0);
}

bool RunStartupBindObservationCreateForTestingFullFallback(FAutomationTestBase& Test)
{
	ResetToIsolatedEngineState();
	FMultiEngineContextStackGuard StackGuard;

	const FName FirstBindName = MakeUniqueStartupBindName(TEXT("Automation.StartupBind.CreateForTesting.FullFallback.First"));
	const FName SecondBindName = MakeUniqueStartupBindName(TEXT("Automation.StartupBind.CreateForTesting.FullFallback.Second"));
	FAngelscriptBinds::FBind FirstBind(FirstBindName, -50, []() {});
	FAngelscriptBinds::FBind SecondBind(SecondBindName, 50, []() {});

	FAngelscriptBindExecutionObservation::Reset();

	FAngelscriptEngineConfig Config;
	Config.bIsEditor = true;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> TestEngine = FAngelscriptEngine::CreateUncompiledWithMode(Config, Dependencies, EAngelscriptEngineCreationMode::Clone);
	if (!Test.TestNotNull(TEXT("MultiEngine.StartupBindObservation.CreateForTestingFullFallbackReplaysBinds should create a fallback full engine"), TestEngine.Get()))
	{
		return false;
	}

	Test.TestEqual(TEXT("MultiEngine.StartupBindObservation.CreateForTestingFullFallbackReplaysBinds should fall back to full mode when no global engine exists"), TestEngine->GetCreationMode(), EAngelscriptEngineCreationMode::Full);
	if (!Test.TestEqual(TEXT("MultiEngine.StartupBindObservation.CreateForTestingFullFallbackReplaysBinds should observe one startup bind pass"), FAngelscriptBindExecutionObservation::GetInvocationCount(), 1))
	{
		return false;
	}

	const FAngelscriptBindExecutionSnapshot Snapshot = FAngelscriptBindExecutionObservation::GetLastSnapshot();
	const int32 FirstIndex = Snapshot.ExecutedBindNames.IndexOfByKey(FirstBindName);
	const int32 SecondIndex = Snapshot.ExecutedBindNames.IndexOfByKey(SecondBindName);
	if (!Test.TestTrue(TEXT("MultiEngine.StartupBindObservation.CreateForTestingFullFallbackReplaysBinds should observe the first bind"), FirstIndex != INDEX_NONE)
		|| !Test.TestTrue(TEXT("MultiEngine.StartupBindObservation.CreateForTestingFullFallbackReplaysBinds should observe the second bind"), SecondIndex != INDEX_NONE))
	{
		return false;
	}

	return Test.TestTrue(TEXT("MultiEngine.StartupBindObservation.CreateForTestingFullFallbackReplaysBinds should preserve order for the fallback full startup pass"), FirstIndex < SecondIndex);
}

bool RunSharedStateParticipantCounts(FAutomationTestBase& Test)
{
	ResetToIsolatedEngineState();

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> SourceEngine = FAngelscriptEngine::CreateUncompiled(Config, Dependencies);
	if (!Test.TestNotNull(TEXT("MultiEngine.SharedState.ParticipantCountsTrackFullAndClones should create the full owner"), SourceEngine.Get()))
	{
		return false;
	}

	if (!Test.TestEqual(TEXT("MultiEngine.SharedState.ParticipantCountsTrackFullAndClones should start with one active participant"), FAngelscriptMultiEngineTestAccess::GetActiveParticipants(*SourceEngine), 1)
		|| !Test.TestEqual(TEXT("MultiEngine.SharedState.ParticipantCountsTrackFullAndClones should start with zero active clones"), FAngelscriptMultiEngineTestAccess::GetActiveCloneCount(*SourceEngine), 0))
	{
		return false;
	}

	TUniquePtr<FAngelscriptEngine> CloneA = FAngelscriptEngine::CreateCloneFrom(*SourceEngine, Config);
	TUniquePtr<FAngelscriptEngine> CloneB = FAngelscriptEngine::CreateCloneFrom(*SourceEngine, Config);
	if (!Test.TestNotNull(TEXT("MultiEngine.SharedState.ParticipantCountsTrackFullAndClones should create clone A"), CloneA.Get())
		|| !Test.TestNotNull(TEXT("MultiEngine.SharedState.ParticipantCountsTrackFullAndClones should create clone B"), CloneB.Get()))
	{
		return false;
	}

	Test.TestEqual(TEXT("MultiEngine.SharedState.ParticipantCountsTrackFullAndClones should count the full owner and two clones"), FAngelscriptMultiEngineTestAccess::GetActiveParticipants(*SourceEngine), 3);
	Test.TestEqual(TEXT("MultiEngine.SharedState.ParticipantCountsTrackFullAndClones should count two active clones"), FAngelscriptMultiEngineTestAccess::GetActiveCloneCount(*SourceEngine), 2);

	CloneB.Reset();
	Test.TestEqual(TEXT("MultiEngine.SharedState.ParticipantCountsTrackFullAndClones should decrement participants when one clone is destroyed"), FAngelscriptMultiEngineTestAccess::GetActiveParticipants(*SourceEngine), 2);
	return Test.TestEqual(TEXT("MultiEngine.SharedState.ParticipantCountsTrackFullAndClones should decrement clone count when one clone is destroyed"), FAngelscriptMultiEngineTestAccess::GetActiveCloneCount(*SourceEngine), 1);
}

}

TEST_CLASS_WITH_FLAGS(FAngelscriptMultiEngineLifecycleTests,
	"Angelscript.TestModule.Engine.MultiEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CloneModuleIsolation)
	{
		using namespace AngelscriptTest_Core_AngelscriptMultiEngineLifecycleTests_Private;
		RunCloneModuleIsolation(*TestRunner);
	}

	TEST_METHOD(CloneDestroyDoesNotAffectPrimary)
	{
		using namespace AngelscriptTest_Core_AngelscriptMultiEngineLifecycleTests_Private;
		RunCloneDestroyDoesNotAffectPrimary(*TestRunner);
	}

	TEST_METHOD(CloneKeepsSharedStateAlive)
	{
		using namespace AngelscriptTest_Core_AngelscriptMultiEngineLifecycleTests_Private;
		RunCloneKeepsSharedStateAlive(*TestRunner);
	}

	TEST_METHOD(DestroyingSourceWhileCloneAliveIsRejected)
	{
		using namespace AngelscriptTest_Core_AngelscriptMultiEngineLifecycleTests_Private;
		RunDestroyingSourceWhileCloneAliveIsRejected(*TestRunner);
	}

	TEST_METHOD(DeferredSharedStateReleasePurgesLocalContextPool)
	{
		using namespace AngelscriptTest_Core_AngelscriptMultiEngineLifecycleTests_Private;
		RunDeferredSharedStateReleasePurgesLocalContextPool(*TestRunner);
	}

	TEST_METHOD(CloneHonorsInjectedDependencies)
	{
		using namespace AngelscriptTest_Core_AngelscriptMultiEngineLifecycleTests_Private;
		RunCloneHonorsInjectedDependencies(*TestRunner);
	}

	TEST_METHOD(StartupBindObservationFullCreate)
	{
		using namespace AngelscriptTest_Core_AngelscriptMultiEngineLifecycleTests_Private;
		RunStartupBindObservationFullCreate(*TestRunner);
	}

	TEST_METHOD(StartupBindObservationCloneCreate)
	{
		using namespace AngelscriptTest_Core_AngelscriptMultiEngineLifecycleTests_Private;
		RunStartupBindObservationCloneCreate(*TestRunner);
	}

	TEST_METHOD(StartupBindObservationCreateForTestingClone)
	{
		using namespace AngelscriptTest_Core_AngelscriptMultiEngineLifecycleTests_Private;
		RunStartupBindObservationCreateForTestingClone(*TestRunner);
	}

	TEST_METHOD(StartupBindObservationCreateForTestingFullFallback)
	{
		using namespace AngelscriptTest_Core_AngelscriptMultiEngineLifecycleTests_Private;
		RunStartupBindObservationCreateForTestingFullFallback(*TestRunner);
	}

	TEST_METHOD(SharedStateParticipantCounts)
	{
		using namespace AngelscriptTest_Core_AngelscriptMultiEngineLifecycleTests_Private;
		RunSharedStateParticipantCounts(*TestRunner);
	}

};

#endif
