#include "LuaDelegateCaller.h"

#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#include "lua.hpp"
#include "LuaObjectBase.h"
#include "LuaState.h"
#include "LuaStackGuard.h"

const TCHAR* ULuaDelegateCaller::DelegateFunctionName = TEXT("NeverUsed");

void ULuaDelegateCaller::ProcessEvent(UFunction* Function, void* Parameters)
{
	if (!LuaState.IsValid() || LuaFunctionIndex == LUA_NOREF
		|| !Function || !Function->GetName().Equals(DelegateFunctionName))
	{
		return;
	}

	lua_State* L = LuaState.Pin()->GetState();
	FLuaStackGuard Gurad(L);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LuaFunctionIndex);
	if (lua_type(L, -1) != LUA_TFUNCTION)
	{
		return;
	}

	bool bWithSelf = false;
	if (LuaFunctionOwnerIndex != LUA_NOREF)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, LuaFunctionOwnerIndex);
		bWithSelf = true;
	}

	// stack = [BindingFunction]
	LuaState.Pin()->CallLuaFunction(SignatureFunction, Parameters, bWithSelf);
}

void ULuaDelegateCaller::BeginDestroy()
{
	ReleaseLuaFunction();

	Super::BeginDestroy();
}

ULuaDelegateCaller* ULuaDelegateCaller::CreateDelegate(UObject* InDelegateOwner, TSharedPtr<FLuaState> InLuaState, UFunction* InSignatureFunction, int InLuaFunctionIndex)
{
	if (!InLuaState.IsValid())
	{
		return nullptr;
	}

	ULuaDelegateCaller* LuaDelegateCaller = NewObject<ULuaDelegateCaller>(InDelegateOwner, ULuaDelegateCaller::StaticClass(), NAME_None);
	if (!LuaDelegateCaller)
	{
		return nullptr;
	}

	LuaDelegateCaller->BindDelegateOwner(InDelegateOwner);
	LuaDelegateCaller->BindLuaState(InLuaState);

	lua_pushvalue(InLuaState->GetState(), InLuaFunctionIndex);
	LuaDelegateCaller->BindLuaFunction(InSignatureFunction, luaL_ref(InLuaState->GetState(), LUA_REGISTRYINDEX));

	if (InLuaFunctionIndex != 2)
	{
		lua_pushvalue(InLuaState->GetState(), InLuaFunctionIndex - 1);
		LuaDelegateCaller->BindLuaFunctionOwner(luaL_ref(InLuaState->GetState(), LUA_REGISTRYINDEX));
	}

	return LuaDelegateCaller;
}

void ULuaDelegateCaller::BindLuaState(TSharedPtr<FLuaState> InLuaState)
{
	LuaState = InLuaState;
}

void ULuaDelegateCaller::BindDelegateOwner(UObject* InOwner)
{
	DelegateOwner = InOwner;
}

void ULuaDelegateCaller::BindLuaFunction(UFunction* InSignatureFunction, int InLuaFunctionIndex)
{
	ReleaseLuaFunction();

	SignatureFunction = InSignatureFunction;
	LuaFunctionIndex = InLuaFunctionIndex;
}

void ULuaDelegateCaller::BindLuaFunctionOwner(int InLuaFunctionOwerIndex)
{
	LuaFunctionOwnerIndex = InLuaFunctionOwerIndex;
}

void ULuaDelegateCaller::BindSignatureFunction(UFunction* InSignatureFunction)
{
	SignatureFunction = InSignatureFunction;
}

void ULuaDelegateCaller::ReleaseLuaFunction()
{
	SignatureFunction = nullptr;

	if (LuaState.IsValid())
	{
		if (LuaFunctionIndex != LUA_NOREF)
		{
			luaL_unref(LuaState.Pin()->GetState(), LUA_REGISTRYINDEX, LuaFunctionIndex);
		}

		if (LuaFunctionOwnerIndex != LUA_NOREF)
		{
			luaL_unref(LuaState.Pin()->GetState(), LUA_REGISTRYINDEX, LuaFunctionOwnerIndex);
		}
	}

	LuaFunctionIndex = LUA_NOREF;
	LuaFunctionOwnerIndex = LUA_NOREF;
}

bool ULuaDelegateCaller::IsBound()
{
	return DelegateOwner.IsValid();
}
