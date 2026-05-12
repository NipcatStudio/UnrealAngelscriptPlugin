#include "StaticJIT/StaticJITConfig.h"
#if UE_BUILD_DEVELOPMENT
#ifndef AS_SKIP_JITTED_CODE


#include "StaticJIT/StaticJITHeader.h"



extern int32 AS_ASStaticJITAotFixture__AddForAOT(FScriptExecution& Execution, asDWORD p_Value);


#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASStaticJITAotFixture = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASStaticJITAotFixture
#endif


int32 AS_ASStaticJITAotFixture__AddForAOT(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int AddForAOT(int)", 3);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITTestHooks::MarkEntry(0xe7c1f010u);
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// SUSPEND 
// ADDIi v1, v0, 7
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) + value_as<int>((asDWORD)0x7u);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 1
  return (int32)l_dwordRegister;
}
static void AS_ASStaticJITAotFixture__AddForAOT_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_ASStaticJITAotFixture__AddForAOT(Execution,
		*(asDWORD*)(l_fp + 0));
}
static void AS_ASStaticJITAotFixture__AddForAOT_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_ASStaticJITAotFixture__AddForAOT(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__AddForAOT_Register(0xe7c1f010u, &AS_ASStaticJITAotFixture__AddForAOT_VMEntry, &AS_ASStaticJITAotFixture__AddForAOT_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__AddForAOT);

int32 AS_ASStaticJITAotFixture__Entry(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int Entry()", 8);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITTestHooks::MarkEntry(0xd030ba65u);
alignas(8) asBYTE l_stack[4];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// SUSPEND 
// PshC4 35
// CALL
// int AddForAOT(const int Value)
SCRIPT_DEBUG_CALLSTACK_LINE(8);
{
l_dwordRegister = (asDWORD) AS_ASStaticJITAotFixture__AddForAOT(Execution,
		value_as<asDWORD>(((asDWORD)0x23u)));
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// CpyRtoV4 v2
v_TEMP_dword_2 = l_dwordRegister;
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// RET 0
  return (int32)l_dwordRegister;
}
static void AS_ASStaticJITAotFixture__Entry_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_ASStaticJITAotFixture__Entry(Execution);
}
static void AS_ASStaticJITAotFixture__Entry_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_ASStaticJITAotFixture__Entry(Execution);
}
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__Entry_Register(0xd030ba65u, &AS_ASStaticJITAotFixture__Entry_VMEntry, &AS_ASStaticJITAotFixture__Entry_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__Entry);

#endif
#endif
