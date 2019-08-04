#include "LuaMulticastScriptDelegate.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "Bluelua.h"
#include "lua.hpp"
#include "LuaFunctionDelegate.h"
#include "LuaState.h"

FLuaMulticastScriptDelegate::FLuaMulticastScriptDelegate(void* InSource, UFunction* InFunction)
	: FLuaUDelegate(InSource, InFunction)
{

}

FLuaMulticastScriptDelegate::~FLuaMulticastScriptDelegate()
{

}

FLuaUDelegate* FLuaMulticastScriptDelegate::Create(lua_State* L, void* InSource, UFunction* InFunction)
{
	uint8* UserData = (uint8*)lua_newuserdata(L, sizeof(FLuaMulticastScriptDelegate));

	FLuaMulticastScriptDelegate* LuaMulticastScriptDelegate = new(UserData) FLuaMulticastScriptDelegate(InSource, InFunction);

	return LuaMulticastScriptDelegate;
}

void FLuaMulticastScriptDelegate::OnAdd(ULuaFunctionDelegate* LuaFunctionDelegate)
{
	FScriptDelegate Delegate;
	Delegate.BindUFunction(LuaFunctionDelegate, ULuaFunctionDelegate::DelegateFunctionName);

	FMulticastScriptDelegate* MulticastScriptDelegate = reinterpret_cast<FMulticastScriptDelegate*>(Source);
	if (MulticastScriptDelegate)
	{
		MulticastScriptDelegate->AddUnique(Delegate);
	}
}

void FLuaMulticastScriptDelegate::OnRemove(ULuaFunctionDelegate* LuaFunctionDelegate)
{
	FScriptDelegate Delegate;
	Delegate.BindUFunction(LuaFunctionDelegate, ULuaFunctionDelegate::DelegateFunctionName);

	FMulticastScriptDelegate* MulticastScriptDelegate = reinterpret_cast<FMulticastScriptDelegate*>(Source);
	if (MulticastScriptDelegate)
	{
		MulticastScriptDelegate->Remove(Delegate);
	}
}

void FLuaMulticastScriptDelegate::OnClear()
{
	FMulticastScriptDelegate* MulticastScriptDelegate = reinterpret_cast<FMulticastScriptDelegate*>(Source);
	if (MulticastScriptDelegate)
	{
		MulticastScriptDelegate->Clear();
	}
}

TArray<UObject*> FLuaMulticastScriptDelegate::OnGetAllObjects() const
{
	FMulticastScriptDelegate* MulticastScriptDelegate = reinterpret_cast<FMulticastScriptDelegate*>(Source);
	if (MulticastScriptDelegate)
	{
		return MulticastScriptDelegate->GetAllObjects();
	}
	else
	{
		return TArray<UObject*>();
	}
}

FString FLuaMulticastScriptDelegate::OnGetName()
{
	return TEXT("MulticastScriptDelegate");
}
