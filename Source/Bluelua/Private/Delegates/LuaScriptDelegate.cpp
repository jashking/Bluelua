#include "LuaScriptDelegate.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "Bluelua.h"
#include "lua.hpp"
#include "LuaFunctionDelegate.h"
#include "LuaState.h"

FLuaScriptDelegate::FLuaScriptDelegate(void* InSource, UFunction* InFunction)
	: FLuaUDelegate(InSource, InFunction)
{

}

FLuaScriptDelegate::~FLuaScriptDelegate()
{

}

FLuaUDelegate* FLuaScriptDelegate::Create(lua_State* L, void* InSource, UFunction* InFunction)
{
	uint8* UserData = (uint8*)lua_newuserdata(L, sizeof(FLuaScriptDelegate));

	FLuaScriptDelegate* LuaScriptDelegate = new(UserData) FLuaScriptDelegate(InSource, InFunction);

	return LuaScriptDelegate;
}

void FLuaScriptDelegate::OnAdd(ULuaFunctionDelegate* LuaFunctionDelegate)
{
	FScriptDelegate* ScriptDelegate = reinterpret_cast<FScriptDelegate*>(Source);
	if (ScriptDelegate)
	{
		ScriptDelegate->BindUFunction(LuaFunctionDelegate, ULuaFunctionDelegate::DelegateFunctionName);
	}
}

void FLuaScriptDelegate::OnRemove(ULuaFunctionDelegate* LuaFunctionDelegate)
{
	FScriptDelegate* ScriptDelegate = reinterpret_cast<FScriptDelegate*>(Source);
	if (ScriptDelegate)
	{
		ScriptDelegate->Clear();
	}
}

void FLuaScriptDelegate::OnClear()
{
	FScriptDelegate* ScriptDelegate = reinterpret_cast<FScriptDelegate*>(Source);
	if (ScriptDelegate)
	{
		ScriptDelegate->Clear();
	}
}

TArray<UObject*> FLuaScriptDelegate::OnGetAllObjects() const
{
	TArray<UObject*> OutputList;

	FScriptDelegate* ScriptDelegate = reinterpret_cast<FScriptDelegate*>(Source);
	if (ScriptDelegate)
	{
		OutputList.Add(ScriptDelegate->GetUObject());
	}

	return OutputList;
}

FString FLuaScriptDelegate::OnGetName()
{
	return TEXT("ScriptDelegate");
}

