#include "LuaUClass.h"

#include "UObject/Class.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

#include "Bluelua.h"
#include "lua.hpp"
#include "LuaObjectBase.h"
#include "LuaState.h"
#include "LuaUObject.h"

DECLARE_CYCLE_STAT(TEXT("ClassPush"), STAT_ClassPush, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("ClassConstruct"), STAT_ClassConstruct, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("ClassIndex"), STAT_ClassIndex, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("ClassNewIndex"), STAT_ClassNewIndex, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("ClassCallStaticUFunction"), STAT_ClassCallStaticUFunction, STATGROUP_Bluelua);

const char* FLuaUClass::UCLASS_METATABLE = "UClass_Metatable";

FLuaUClass::FLuaUClass(UClass* InSource)
	: Source(InSource)
{

}

FLuaUClass::~FLuaUClass()
{

}

int FLuaUClass::Push(lua_State* L, UClass* InSource)
{
	SCOPE_CYCLE_COUNTER(STAT_ClassPush);

	if (!InSource)
	{
		lua_pushnil(L);
		return 1;
	}

	FLuaState* LuaStateWrapper = FLuaState::GetStateWrapper(L);
	if (LuaStateWrapper && LuaStateWrapper->GetFromCache(InSource))
	{
		return 1;
	}

	void* Buffer = lua_newuserdata(L, sizeof(FLuaUClass));
	FLuaUClass* LuaUClass = new(Buffer) FLuaUClass(InSource);

	if (luaL_newmetatable(L, UCLASS_METATABLE))
	{
		static struct luaL_Reg Metamethods[] =
		{
			{ "__call", Construct },
			{ "__index", Index },
			{ "__newindex", NewIndex },
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

UClass* FLuaUClass::Fetch(lua_State* L, int32 Index)
{
	if (lua_isnil(L, Index))
	{
		return nullptr;
	}

	FLuaUClass* LuaUClass = (FLuaUClass*)luaL_checkudata(L, Index, UCLASS_METATABLE);

	return LuaUClass->Source.IsValid() ? LuaUClass->Source.Get() : nullptr;
}

int FLuaUClass::Construct(lua_State* L)
{
	SCOPE_CYCLE_COUNTER(STAT_ClassConstruct);

	FLuaUClass* LuaUClass = (FLuaUClass*)luaL_checkudata(L, 1, UCLASS_METATABLE);
	if (!LuaUClass->Source.IsValid())
	{
		return 0;
	}

	UObject* Owner = FLuaUObject::Fetch(L, 2);
	if (!Owner)
	{
		return 0;
	}

	FName ObjectName;
	if (lua_isstring(L, 3))
	{
		FLuaObjectBase::Fetch(L, 3, ObjectName);
	}

	UObject* Object = NewObject<UObject>(Owner, LuaUClass->Source.Get(), ObjectName);

	return FLuaUObject::Push(L, Object, Owner);
}

int FLuaUClass::Index(lua_State* L)
{
	SCOPE_CYCLE_COUNTER(STAT_ClassIndex);

	FLuaUClass* LuaUClass = (FLuaUClass*)luaL_checkudata(L, 1, UCLASS_METATABLE);
	if (!LuaUClass->Source.IsValid())
	{
		return 0;
	}

	const char* PropertyName = lua_tostring(L, 2);
	if (UFunction* Function = LuaUClass->Source->FindFunctionByName(PropertyName))
	{
		if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
		{
			luaL_error(L, "Function[%s] is not blueprint callable!", TCHAR_TO_UTF8(*Function->GetName()));
		}

		lua_pushlightuserdata(L, Function);
		lua_pushcclosure(L, CallStaticUFunction, 1);

		return 1;
	}
	else if (UProperty* Property = LuaUClass->Source->FindPropertyByName(PropertyName))
	{
		return FLuaObjectBase::PushProperty(L, Property, LuaUClass->Source->GetDefaultObject(), false);
	}

	return 0;
}

int FLuaUClass::NewIndex(lua_State* L)
{
	SCOPE_CYCLE_COUNTER(STAT_ClassNewIndex);

	FLuaUClass* LuaUClass = (FLuaUClass*)luaL_checkudata(L, 1, UCLASS_METATABLE);
	if (!LuaUClass->Source.IsValid())
	{
		return 0;
	}

	UObject* ClassDefaultObject = LuaUClass->Source->GetDefaultObject();
	const char* PropertyName = lua_tostring(L, 2);

	UProperty* Property = LuaUClass->Source->FindPropertyByName(PropertyName);
	if (Property)
	{
		if (Property->PropertyFlags & CPF_BlueprintReadOnly)
		{
			luaL_error(L, "Can't write to a readonly property[%s] in class[%s]!", PropertyName, TCHAR_TO_UTF8(*(LuaUClass->Source->GetName())));
		}

		FLuaObjectBase::FetchProperty(L, Property, ClassDefaultObject, 3);
	}
	else
	{
		luaL_error(L, "Can't find property[%s] in class[%s]!", PropertyName, TCHAR_TO_UTF8(*(LuaUClass->Source->GetName())));
	}

	return 0;
}

int FLuaUClass::ToString(lua_State* L)
{
	FLuaUClass* LuaUClass = (FLuaUClass*)luaL_checkudata(L, 1, UCLASS_METATABLE);

	lua_pushstring(L, TCHAR_TO_UTF8(*FString::Printf(TEXT("UClass[%s]"), LuaUClass->Source.IsValid() ? *(LuaUClass->Source->GetName()) : TEXT("null"))));

	return 1;
}

int FLuaUClass::CallStaticUFunction(lua_State* L)
{
	SCOPE_CYCLE_COUNTER(STAT_ClassCallStaticUFunction);

	UFunction* Function = (UFunction*)lua_touserdata(L, lua_upvalueindex(1));
	FLuaUClass* LuaUClass = (FLuaUClass*)luaL_checkudata(L, 1, UCLASS_METATABLE);
	if (!LuaUClass->Source.IsValid())
	{
		return 0;
	}

	return FLuaObjectBase::CallFunction(L, LuaUClass->Source->GetDefaultObject(), Function);
}
