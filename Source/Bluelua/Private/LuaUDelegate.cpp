#include "LuaUDelegate.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "lua.hpp"
#include "LuaDelegateCaller.h"
#include "LuaState.h"

const char* FLuaUDelegate::UDELEGATE_METATABLE = "UDelegate_Metatable";

FLuaUDelegate::FLuaUDelegate(UObject* InOwner, void* InSource, UFunction* InFunction, bool InbIsMulticast)
	: Owner(InOwner)
	, Source(InSource)
	, Function(InFunction)
	, bIsMulticast(InbIsMulticast)
{

}

FLuaUDelegate::~FLuaUDelegate()
{

}

int FLuaUDelegate::Push(lua_State* L, UObject* Owner, void* InSource, UFunction* InFunction, bool InbIsMulticast, void* InBuffer /*= nullptr*/)
{
	if (!InSource || !InFunction || !Owner || !Owner->IsValidLowLevel())
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

	FLuaUDelegate* LuaUDelegate = new(UserData) FLuaUDelegate(Owner, InSource, InFunction, InbIsMulticast);

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
	if (!LuaUDelegate->Owner || !LuaUDelegate->Source || !LuaUDelegate->Function)
	{
		return 0;
	}

	const int Param2Type = lua_type(L, 2);
	if (Param2Type != LUA_TFUNCTION && lua_type(L, 3) != LUA_TFUNCTION)
	{
		luaL_error(L, "Add delegate failed! Param 2 or Param 3 must be a function!");
	}

	const int FunctionIndex = (Param2Type == LUA_TFUNCTION) ? 2 : 3;

	FLuaState* LuaStateWrapper = FLuaState::GetStateWrapper(L);
	if (!LuaStateWrapper)
	{
		return 0;
	}

	ULuaDelegateCaller* LuaDelegateCaller = ULuaDelegateCaller::CreateDelegate(LuaUDelegate->Owner, LuaStateWrapper->AsShared(), LuaUDelegate->Function, FunctionIndex);
	if (!LuaDelegateCaller)
	{
		return 0;
	}

	if (LuaUDelegate->bIsMulticast)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(LuaDelegateCaller, TEXT("NeverUsed"));

		FMulticastScriptDelegate* MulticastScriptDelegate = reinterpret_cast<FMulticastScriptDelegate*>(LuaUDelegate->Source);
		MulticastScriptDelegate->AddUnique(Delegate);
	}
	else
	{
		FScriptDelegate* ScriptDelegate = reinterpret_cast<FScriptDelegate*>(LuaUDelegate->Source);
		ScriptDelegate->BindUFunction(LuaDelegateCaller, TEXT("NeverUsed"));
	}

	lua_pushlightuserdata(L, LuaDelegateCaller);
	LuaStateWrapper->AddDelegateReference(LuaDelegateCaller);

	return 1;
}

int FLuaUDelegate::Remove(lua_State* L)
{
	FLuaUDelegate* LuaUDelegate = (FLuaUDelegate*)luaL_checkudata(L, 1, UDELEGATE_METATABLE);

	ULuaDelegateCaller* LuaDelegateCaller = reinterpret_cast<ULuaDelegateCaller*>(lua_touserdata(L, 2));
	if (!LuaDelegateCaller->IsValidLowLevel())
	{
		return 0;
	}

	LuaDelegateCaller->ReleaseLuaFunction();

	FLuaState* LuaStateWrapper = FLuaState::GetStateWrapper(L);
	if (LuaStateWrapper)
	{
		LuaStateWrapper->RemoveDelegateReference(LuaDelegateCaller);
	}

	return 0;
}

int FLuaUDelegate::ToString(lua_State* L)
{
	FLuaUDelegate* LuaUDelegate = (FLuaUDelegate*)luaL_checkudata(L, 1, UDELEGATE_METATABLE);

	lua_pushstring(L, TCHAR_TO_UTF8(*FString::Printf(TEXT("UDelegate[%s]"), LuaUDelegate->Function ? *(LuaUDelegate->Function->GetName()) : TEXT("null"))));

	return 1;
}
