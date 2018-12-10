#include "LuaImplementableInterface.h"

#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "Bluelua.h"
#include "lua.hpp"
#include "LuaObjectBase.h"
#include "LuaState.h"
#include "LuaStackGuard.h"
#include "LuaUObject.h"

DECLARE_CYCLE_STAT(TEXT("InitLuaObject"), STAT_InitLuaObject, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("ReleaseLuaObject"), STAT_ReleaseLuaObject, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("CleanAllLuaObject"), STAT_CleanAllLuaObject, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("ProcessLuaObjectOverrideEvent"), STAT_ProcessLuaObjectOverrideEvent, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("CallBPFunctionOverride"), STAT_CallBPFunctionOverride, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("FillBPFunctionOverrideOutProperty"), STAT_FillBPFunctionOverrideOutProperty, STATGROUP_Bluelua);

struct FLuaAutoCleanGlobal
{
public:
	FLuaAutoCleanGlobal(lua_State* L, const char* InGlobalName)
		: GuardingStack(L)
		, GlobalName(InGlobalName)
	{

	}

	~FLuaAutoCleanGlobal()
	{
		if (GuardingStack)
		{
			lua_pushnil(GuardingStack);
			lua_setglobal(GuardingStack, GlobalName);
		}
	}

private:
	lua_State * GuardingStack;
	const char* GlobalName;
};

TMap<FLuaState*, TSet<ILuaImplementableInterface*>> ILuaImplementableInterface::LuaImplementableObjects;

void ILuaImplementableInterface::PreInit(TSharedPtr<FLuaState> InLuaState /*= nullptr*/)
{
	OnInit(BindingLuaPath, InLuaState);
}

bool ILuaImplementableInterface::IsLuaBound() const
{
	return (LuaState.IsValid() && ModuleReferanceIndex != LUA_NOREF);
}

bool ILuaImplementableInterface::FetchLuaModule()
{
	if (IsLuaBound())
	{
		lua_rawgeti(LuaState->GetState(), LUA_REGISTRYINDEX, ModuleReferanceIndex);
		if (lua_type(LuaState->GetState(), -1) != LUA_TTABLE)
		{
			lua_pop(LuaState->GetState(), 1);
			return false;
		}

		return true;
	}

	return false;
}

void ILuaImplementableInterface::CleanAllLuaImplementableObject(FLuaState* InLuaState/* = nullptr*/)
{
	SCOPE_CYCLE_COUNTER(STAT_CleanAllLuaObject);

	TArray<ILuaImplementableInterface*> PendingCleanObjects;
	for (auto& Iter : LuaImplementableObjects)
	{
		if (!InLuaState || Iter.Key == InLuaState)
		{
			for (auto& Interface : Iter.Value)
			{
				PendingCleanObjects.Emplace(Interface);
			}
		}
	}

	for (auto& Iter : PendingCleanObjects)
	{
		UObject* Object = Cast<UObject>(Iter);
		if (Object && Object->IsValidLowLevel())
		{
			Iter->OnRelease();
		}
	}

	if (InLuaState)
	{
		LuaImplementableObjects.Remove(InLuaState);
	}
	else
	{
		LuaImplementableObjects.Empty();
	}
}

void ILuaImplementableInterface::PreRegisterLua(const FString& InLuaFilePath)
{
	if (!InLuaFilePath.IsEmpty())
	{
		BindingLuaPath = InLuaFilePath;
	}
}

bool ILuaImplementableInterface::OnInit(const FString& InLuaFilePath, TSharedPtr<FLuaState> InLuaState/* = nullptr*/)
{
	SCOPE_CYCLE_COUNTER(STAT_InitLuaObject);

	if (InLuaFilePath.IsEmpty())
	{
		return false;
	}

	if (!BindingLuaPath.Equals(InLuaFilePath))
	{
		// new lua file
		OnRelease();
	}

	if (IsLuaBound())
	{
		// already init
		return true;
	}

	LuaState = InLuaState.IsValid() ? InLuaState : FBlueluaModule::Get().GetDefaultLuaState();
	if (!LuaState.IsValid())
	{
		return false;
	}

	BindingLuaPath = InLuaFilePath;
	AddToLuaObjectList(LuaState.Get(), this);

	lua_State* L = LuaState->GetState();
	FLuaStackGuard StackGuard(L);

	FLuaAutoCleanGlobal AutoCleanGlobal(L, "Super");

	FLuaUObject::Push(L, Cast<UObject>(this));
	lua_setglobal(L, "Super");

	if (!LuaState->DoFile(FPaths::ProjectContentDir() / InLuaFilePath))
	{
		return false;
	}

	if (lua_type(L, -1) != LUA_TTABLE)
	{
		return false;
	}

	ModuleReferanceIndex = luaL_ref(L, LUA_REGISTRYINDEX);

	InitBPFunctionOverriding();

	return true;
}

void ILuaImplementableInterface::OnRelease()
{
	SCOPE_CYCLE_COUNTER(STAT_ReleaseLuaObject);

	ClearBPFunctionOverriding();

	RemoveFromLuaObjectList(LuaState.Get(), this);

	if (LuaState.IsValid())
	{
		LuaState->RemoveReferenceByOwner(Cast<UObject>(this));

		if (ModuleReferanceIndex != LUA_NOREF)
		{
			luaL_unref(LuaState->GetState(), LUA_REGISTRYINDEX, ModuleReferanceIndex);
		}
	}

	ModuleReferanceIndex = LUA_NOREF;
	LuaState.Reset();
	BindingLuaPath = TEXT("");
}

bool ILuaImplementableInterface::OnProcessEvent(UFunction* Function, void* Parameters)
{
	if (!LuaState.IsValid() || ModuleReferanceIndex == LUA_NOREF
		|| !Function || !Function->HasAnyFunctionFlags(FUNC_BlueprintEvent))
	{
		return false;
	}

	SCOPE_CYCLE_COUNTER(STAT_ProcessLuaObjectOverrideEvent);

	lua_State* L = LuaState->GetState();
	FLuaStackGuard Gurad(L);

	if (!PrepareLuaFunction(Function->GetName()))
	{
		return false;
	}

	return LuaState->CallLuaFunction(Function, Parameters);
}

bool ILuaImplementableInterface::InitBPFunctionOverriding()
{
	lua_State* L = LuaState->GetState();
	FLuaStackGuard Gurad(L);

	ClearBPFunctionOverriding();

	if (!PrepareLuaFunction(TEXT("OnInitBPFunctionOverriding")))
	{
		return false;
	}

	const int ModuleIndex = lua_absindex(L, -3);

	if (!LuaState->CallLuaFunction(1, 1))
	{
		return false;
	}

	if (lua_type(L, -1) != LUA_TTABLE)
	{
		// lua return value is not an array
		return false;
	}

	UClass* Class = Cast<UObject>(this)->GetClass();
	const int ReturnTableIndex = lua_gettop(L);

	lua_pushnil(L); // initial key, stack = [..., nil]
	while (lua_next(L, ReturnTableIndex)) // stack = [..., key, value]
	{
		FString TargetBPFunctionName;
		FLuaObjectBase::Fetch(L, -1, TargetBPFunctionName);

		const bool bIsModuleFunction = (lua_getfield(L, ModuleIndex, TCHAR_TO_UTF8(*TargetBPFunctionName)) == LUA_TFUNCTION); // stack = [..., key, value, func]
		lua_pop(L, 1); // stack = [..., key, value]

		if (bIsModuleFunction)
		{
			UFunction* BPFunction = Class->FindFunctionByName(*TargetBPFunctionName);
			if (BPFunction && !BPFunction->IsNative())
			{
				// set bp function to native and change native function from UObject::ProcessInternal to
				// our own function so that when bp function get called we can know that and redirect to lua
				BPFunction->FunctionFlags |= FUNC_Native;
				BPFunction->SetNativeFunc(&ILuaImplementableInterface::ProcessBPFunctionOverride);
				OverridedBPFunctionList.Emplace(TargetBPFunctionName);
			}
			else
			{
				if (!BPFunction)
				{
					UE_LOG(LogBluelua, Warning, TEXT("Lua override bp function failed! No bp function named: %s in class: %s"), *TargetBPFunctionName, *Class->GetName());
				}
				else
				{
					UE_LOG(LogBluelua, Warning, TEXT("Lua override bp function failed! Cann't override a native function: %s in class: %s"), *TargetBPFunctionName, *Class->GetName());
				}
			}
		}
		else
		{
			UE_LOG(LogBluelua, Warning, TEXT("Lua override bp function failed! No lua function named: %s in %s"), *TargetBPFunctionName, *BindingLuaPath);
		}

		lua_pop(L, 1); // stack = [..., key]
	}

	return true;
}

void ILuaImplementableInterface::ClearBPFunctionOverriding()
{
	OverridedBPFunctionList.Empty();
}

bool ILuaImplementableInterface::HasBPFunctionOverrding(const FString& FunctionName)
{
	return (IsLuaBound() && OverridedBPFunctionList.Contains(FunctionName));
}

bool ILuaImplementableInterface::PrepareLuaFunction(const FString& FunctionName)
{
	if (!IsLuaBound())
	{
		return false;
	}

	lua_State* L = LuaState->GetState();

	lua_rawgeti(L, LUA_REGISTRYINDEX, ModuleReferanceIndex);
	if (lua_type(L, -1) != LUA_TTABLE)
	{
		return false;
	} // stack = [Module]

	if (lua_getfield(L, -1, TCHAR_TO_UTF8(*FunctionName)) != LUA_TFUNCTION)
	{
		return false;
	} // stack = [Module, Function]

	lua_pushvalue(L, -2); // stack = [Module, Function, Module]
	return true;
}

void ILuaImplementableInterface::AddToLuaObjectList(FLuaState* InLuaState, ILuaImplementableInterface* Object)
{
	if (!Object)
	{
		return;
	}

	LuaImplementableObjects.FindOrAdd(InLuaState).Emplace(Object);
}

void ILuaImplementableInterface::RemoveFromLuaObjectList(FLuaState* InLuaState, ILuaImplementableInterface* Object)
{
	TSet<ILuaImplementableInterface*>* ValuePtr = LuaImplementableObjects.Find(InLuaState);
	if (!ValuePtr)
	{
		return;
	}

	(*ValuePtr).Remove(Object);
}

int ILuaImplementableInterface::FillBPFunctionOverrideOutProperty(struct lua_State* L)
{
	SCOPE_CYCLE_COUNTER(STAT_FillBPFunctionOverrideOutProperty);

	int32 OutParamsCount = lua_tointeger(L, lua_upvalueindex(1));
	FOutParmRec* OutParamsList = (FOutParmRec*)lua_touserdata(L, lua_upvalueindex(2 + OutParamsCount));

	int32 OutParamUpValueIndex = 2;
	for (FOutParmRec* OutParam = OutParamsList; OutParam; OutParam = OutParam->NextOutParm)
	{
		FLuaObjectBase::FetchProperty(L, OutParam->Property, OutParam->PropAddr, lua_upvalueindex(OutParamUpValueIndex++));
	}

	return 0;
}

void ILuaImplementableInterface::ProcessBPFunctionOverride(UObject* Context, FFrame& Stack, void* const Z_Param__Result)
{
	UFunction* Function = Stack.CurrentNativeFunction;
	ILuaImplementableInterface* LuaObject = Cast<ILuaImplementableInterface>(Context);
	if (LuaObject && LuaObject->CallBPFunctionOverride(Function, Stack, Z_Param__Result))
	{
		return;
	}

	// One bp call another bp function: ... -> CallFunction -> ProcessInternal, so we can call CallFunction
	// without FUNC_Native again in our own ProcessInternal to back to original pass:
	Function->FunctionFlags &= ~FUNC_Native;
	Context->CallFunction(Stack, Z_Param__Result, Function);
	Function->FunctionFlags |= FUNC_Native;
}

bool ILuaImplementableInterface::CallBPFunctionOverride(UFunction* Function, FFrame& Stack, void* const Z_Param__Result)
{
	SCOPE_CYCLE_COUNTER(STAT_CallBPFunctionOverride);

	if (!Function || !HasBPFunctionOverrding(Function->GetName()))
	{
		return false;
	}

	lua_State* L = LuaState->GetState();
	FLuaStackGuard Gurad(L);

	if (!PrepareLuaFunction(Function->GetName()))
	{
		return false;
	}

	uint8* Frame = (uint8*)FMemory_Alloca(Function->PropertiesSize);
	FMemory::Memzero(Frame, Function->PropertiesSize);

	int32 InParamsCount = 0;
	int32 OutParamsCount = 0;
	FOutParmRec* OutParamsList = nullptr;
	FOutParmRec* LastOut = nullptr;

	for (UProperty* Property = (UProperty*)Function->Children; *Stack.Code != EX_EndFunctionParms; Property = (UProperty*)Property->Next)
	{
		Stack.MostRecentPropertyAddress = NULL;

		// Skip the return parameter case, as we've already handled it above
		const bool bIsReturnParam = ((Property->PropertyFlags & CPF_ReturnParm) != 0);
		if (bIsReturnParam)
		{
			UE_LOG(LogBluelua, Warning, TEXT("Call bp function in lua overriding is not supposed to have a return property!"));
			continue;
		}

		if (Property->PropertyFlags & CPF_OutParm)
		{
			// evaluate the expression for this parameter, which sets Stack.MostRecentPropertyAddress to the address of the property accessed
			Stack.Step(Stack.Object, NULL);

			CA_SUPPRESS(6263)
			FOutParmRec* Out = (FOutParmRec*)FMemory_Alloca(sizeof(FOutParmRec));
			// set the address and property in the out param info
			// warning: Stack.MostRecentPropertyAddress could be NULL for optional out parameters
			// if that's the case, we use the extra memory allocated for the out param in the function's locals
			// so there's always a valid address
			ensure(Stack.MostRecentPropertyAddress); // possible problem - output param values on local stack are neither initialized nor cleaned.
			Out->PropAddr = (Stack.MostRecentPropertyAddress != NULL) ? Stack.MostRecentPropertyAddress : Property->ContainerPtrToValuePtr<uint8>(Frame);
			Out->Property = Property;
			Out->NextOutParm = nullptr;

			if (Property->PropertyFlags & CPF_ReferenceParm)
			{
				// also an in param
				++InParamsCount;
				FLuaObjectBase::PushProperty(L, Property, Out->PropAddr);
			}

			if (!(Property->PropertyFlags & CPF_ConstParm))
			{
				++OutParamsCount;

				// chain all out params
				if (!LastOut)
				{
					LastOut = Out;
					OutParamsList = Out;
				}
				else
				{
					LastOut->NextOutParm = Out;
					LastOut = Out;
				}
			}
		}
		else
		{
			// copy the result of the expression for this parameter into the appropriate part of the local variable space
			uint8* Param = Property->ContainerPtrToValuePtr<uint8>(Frame);
			checkSlow(Param);

			Property->InitializeValue_InContainer(Frame);

			Stack.Step(Stack.Object, Param);

			++InParamsCount;
			FLuaObjectBase::PushProperty(L, Property, Param);
		}
	}

	Stack.Code++;

	// Initialize any local struct properties with defaults
	for (UProperty* LocalProp = Function->FirstPropertyToInit; LocalProp != NULL; LocalProp = (UProperty*)LocalProp->Next)
	{
		LocalProp->InitializeValue_InContainer(Frame);
	}

	if (LuaState->CallLuaFunction(InParamsCount + 1, OutParamsCount) && OutParamsCount > 0)
	{
		lua_pushinteger(L, OutParamsCount);
		lua_insert(L, -(OutParamsCount + 1));
		lua_pushlightuserdata(L, OutParamsList);
		lua_pushcclosure(L, FillBPFunctionOverrideOutProperty, OutParamsCount + 2);

		LuaState->CallLuaFunction(0, 0, false);
	}

	// destruct properties on the stack, except for out params since we know we didn't use that memory
	for (UProperty* Destruct = Function->DestructorLink; Destruct; Destruct = Destruct->DestructorLinkNext)
	{
		if (!Destruct->HasAnyPropertyFlags(CPF_OutParm))
		{
			Destruct->DestroyValue_InContainer(Frame);
		}
	}

	return true;
}
