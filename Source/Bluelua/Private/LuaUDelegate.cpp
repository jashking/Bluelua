#include "LuaUDelegate.h"

#include "Engine/LatentActionManager.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "Bluelua.h"
#include "lua.hpp"
#include "LuaFunctionDelegate.h"
#include "LuaState.h"
#include "LuaUObject.h"

DECLARE_CYCLE_STAT(TEXT("DelegatePush"), STAT_DelegatePush, STATGROUP_Bluelua);

const char* FLuaUDelegate::UDELEGATE_METATABLE = "UDelegate_Metatable";

FLuaUDelegate::FLuaUDelegate(void* InSource, UFunction* InFunction, bool InbIsMulticast)
	: Source(InSource)
	, Function(InFunction)
	, bIsMulticast(InbIsMulticast)
{

}

FLuaUDelegate::~FLuaUDelegate()
{

}

int FLuaUDelegate::Push(lua_State* L, void* InSource, UFunction* InFunction, bool InbIsMulticast, void* InBuffer /*= nullptr*/)
{
	SCOPE_CYCLE_COUNTER(STAT_DelegatePush);

	if (!InSource || !InFunction)
	{
		lua_pushnil(L);
		return 1;
	}

	FLuaState* LuaStateWrapper = FLuaState::GetStateWrapper(L);
	if (LuaStateWrapper && LuaStateWrapper->GetFromCache(InSource))
	{
		return 1;
	}

	uint8* UserData = (uint8*)lua_newuserdata(L, sizeof(FLuaUDelegate));

	FLuaUDelegate* LuaUDelegate = new(UserData) FLuaUDelegate(InSource, InFunction, InbIsMulticast);

	if (luaL_newmetatable(L, UDELEGATE_METATABLE))
	{
		static struct luaL_Reg Metamethods[] =
		{
			{ "__index", Index },
			{ "__tostring", ToString },
			{ NULL, NULL },
		};

		luaL_setfuncs(L, Metamethods, 0);
	}

	lua_setmetatable(L, -2);

	if (LuaStateWrapper)
	{
		LuaStateWrapper->AddToCache(InSource);
	}

	return 1;
}

bool FLuaUDelegate::Fetch(lua_State* L, int32 Index, UFunction* InFunction, FScriptDelegate* InScriptDelegate)
{
	if (!InScriptDelegate || !InFunction || lua_isnil(L, Index))
	{
		return false;
	}

	ULuaFunctionDelegate* FunctionDelegate = ULuaFunctionDelegate::Fetch(L, Index);

	FunctionDelegate->BindSignatureFunction(InFunction);
	InScriptDelegate->BindUFunction(FunctionDelegate, ULuaFunctionDelegate::DelegateFunctionName);

	return true;
}

int FLuaUDelegate::CreateFunctionDelegate(lua_State* L)
{
	UObject* DelegateOwner = FLuaUObject::Fetch(L, 1);
	if (!DelegateOwner)
	{
		luaL_error(L, "Create delegate failed! Param 1 must be a UObject as owner!");
		return 0;
	}

	const int Param2Type = lua_type(L, 2);
	if (Param2Type != LUA_TFUNCTION && lua_type(L, 3) != LUA_TFUNCTION)
	{
		luaL_error(L, "Create delegate failed! Param 2 or Param 3 must be a function!");
	}

	const int FunctionIndex = (Param2Type == LUA_TFUNCTION) ? 2 : 3;

	FLuaState* LuaStateWrapper = FLuaState::GetStateWrapper(L);
	if (!LuaStateWrapper)
	{
		return 0;
	}

	ULuaFunctionDelegate* FunctionDelegate = ULuaFunctionDelegate::Create(DelegateOwner, LuaStateWrapper->AsShared(), nullptr, FunctionIndex);
	if (!FunctionDelegate)
	{
		return 0;
	}

	return FLuaUObject::Push(L, FunctionDelegate, DelegateOwner);
}

int FLuaUDelegate::CreateLatentAction(lua_State* L)
{
	ULuaFunctionDelegate* FunctionDelegate = ULuaFunctionDelegate::Fetch(L, 1);
	FGuid Guid = FGuid::NewGuid();
	FLatentActionInfo LatentActionInfo(0, GetTypeHash(Guid), ULuaFunctionDelegate::DelegateFunctionName, FunctionDelegate);

	FLuaUStruct::Push(L, FLatentActionInfo::StaticStruct(), (void*)&LatentActionInfo);

	return 1;
}

int FLuaUDelegate::Index(lua_State* L)
{
	FLuaUDelegate* LuaUDelegate = (FLuaUDelegate*)luaL_checkudata(L, 1, UDELEGATE_METATABLE);
	if (!LuaUDelegate->Source)
	{
		return 0;
	}

	const FString& PropertyName = UTF8_TO_TCHAR(lua_tostring(L, 2));
	if (PropertyName.Equals(TEXT("Add")))
	{
		lua_pushcfunction(L, &FLuaUDelegate::Add);
		return 1;
	}
	else if (PropertyName.Equals(TEXT("Remove")))
	{
		lua_pushcfunction(L, &FLuaUDelegate::Remove);
		return 1;
	}

	return 0;
}

int FLuaUDelegate::Add(lua_State* L)
{
	FLuaUDelegate* LuaUDelegate = (FLuaUDelegate*)luaL_checkudata(L, 1, UDELEGATE_METATABLE);
	if (!LuaUDelegate->Source || !LuaUDelegate->Function)
	{
		return 0;
	}

	FLuaState* LuaStateWrapper = FLuaState::GetStateWrapper(L);
	if (!LuaStateWrapper)
	{
		return 0;
	}

	ULuaFunctionDelegate* FunctionDelegate = ULuaFunctionDelegate::Fetch(L, 2);
	FunctionDelegate->BindSignatureFunction(LuaUDelegate->Function);

	if (LuaUDelegate->bIsMulticast)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(FunctionDelegate, ULuaFunctionDelegate::DelegateFunctionName);

		FMulticastScriptDelegate* MulticastScriptDelegate = reinterpret_cast<FMulticastScriptDelegate*>(LuaUDelegate->Source);
		if (MulticastScriptDelegate)
		{
			MulticastScriptDelegate->AddUnique(Delegate);
		}
	}
	else
	{
		FScriptDelegate* ScriptDelegate = reinterpret_cast<FScriptDelegate*>(LuaUDelegate->Source);
		if (ScriptDelegate)
		{
			ScriptDelegate->BindUFunction(FunctionDelegate, ULuaFunctionDelegate::DelegateFunctionName);
		}
	}

	lua_pushboolean(L, 1);
	return 1;
}

int FLuaUDelegate::Remove(lua_State* L)
{
	FLuaUDelegate* LuaUDelegate = (FLuaUDelegate*)luaL_checkudata(L, 1, UDELEGATE_METATABLE);
	ULuaFunctionDelegate* FunctionDelegate = ULuaFunctionDelegate::Fetch(L, 2);

	FunctionDelegate->Clear();

	FLuaState* LuaStateWrapper = FLuaState::GetStateWrapper(L);
	if (LuaStateWrapper)
	{
		LuaStateWrapper->RemoveReference(FunctionDelegate, FunctionDelegate->GetOuter());
	}

	if (LuaUDelegate->bIsMulticast)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(FunctionDelegate, ULuaFunctionDelegate::DelegateFunctionName);

		FMulticastScriptDelegate* MulticastScriptDelegate = reinterpret_cast<FMulticastScriptDelegate*>(LuaUDelegate->Source);
		if (MulticastScriptDelegate)
		{
			MulticastScriptDelegate->Remove(Delegate);
		}
	}
	else
	{
		FScriptDelegate* ScriptDelegate = reinterpret_cast<FScriptDelegate*>(LuaUDelegate->Source);
		if (ScriptDelegate)
		{
			ScriptDelegate->Unbind();
		}
	}

	return 0;
}

int FLuaUDelegate::ToString(lua_State* L)
{
	FLuaUDelegate* LuaUDelegate = (FLuaUDelegate*)luaL_checkudata(L, 1, UDELEGATE_METATABLE);

	lua_pushstring(L, TCHAR_TO_UTF8(*FString::Printf(TEXT("UDelegate[%s]"), LuaUDelegate->Function ? *(LuaUDelegate->Function->GetName()) : TEXT("null"))));

	return 1;
}
