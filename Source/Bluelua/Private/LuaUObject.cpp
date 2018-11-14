#include "LuaUObject.h"

#include "Blueprint/WidgetTree.h"
#include "UObject/Class.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

#include "lua.hpp"
#include "LuaState.h"

const char* FLuaUObject::UOBJECT_METATABLE = "UObject_Metatable";

FLuaUObject::FLuaUObject(UObject* InSource, bool InbRef)
	: Source(InSource)
	, bRef(bRef)
{

}

FLuaUObject::~FLuaUObject()
{

}

int FLuaUObject::Push(lua_State* L, UObject* InSource, bool bRef/* = false*/)
{
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

	void* Buffer = lua_newuserdata(L, sizeof(FLuaUObject));
	FLuaUObject* LuaUObject = new(Buffer) FLuaUObject(InSource, bRef);

	if (luaL_newmetatable(L, UOBJECT_METATABLE))
	{
		static struct luaL_Reg Metamethods[] =
		{
			{ "__index", Index },
			{ "__newindex", NewIndex },
			{ "__tostring", ToString },
			{ "__gc", GC },
			{ NULL, NULL },
		};

		luaL_setfuncs(L, Metamethods, 0);
	}

	lua_setmetatable(L, -2);

	if (LuaStateWrapper)
	{
		LuaStateWrapper->AddToCache(InSource);

		if (bRef)
		{
			LuaStateWrapper->AddReference(InSource);
		}
	}

	return 1;
}

UObject* FLuaUObject::Fetch(lua_State* L, int32 Index)
{
	if (lua_isnil(L, Index))
	{
		return nullptr;
	}

	FLuaUObject* LuaUObject = (FLuaUObject*)luaL_checkudata(L, Index, UOBJECT_METATABLE);

	return LuaUObject->Source.IsValid() ? LuaUObject->Source.Get() : nullptr;
}

int FLuaUObject::LuaLoadObject(lua_State* L)
{
	UObject* Owner = FLuaUObject::Fetch(L, 1);
	if (!Owner)
	{
		return 0;
	}

	const char* ObjectPath = lua_tostring(L, 2);
	if (!ObjectPath)
	{
		return 0;
	}

	FLuaState* LuaStateWrapper = FLuaState::GetStateWrapper(L);
	if (!LuaStateWrapper)
	{
		return 0;
	}

	UObject* Object = LoadObject<UObject>(Owner, UTF8_TO_TCHAR(ObjectPath));
	LuaStateWrapper->AddReference(Object, Owner);

	return FLuaUObject::Push(L, Object, false);
}

int FLuaUObject::LuaUnLoadObject(lua_State* L)
{
	UObject* Object = FLuaUObject::Fetch(L, 1);
	if (!Object)
	{
		return 0;
	}

	FLuaState* LuaStateWrapper = FLuaState::GetStateWrapper(L);
	if (LuaStateWrapper)
	{
		LuaStateWrapper->RemoveReference(Object, Object->GetOuter());
	}

	return 0;
}

int FLuaUObject::Index(lua_State* L)
{
	FLuaUObject* LuaUObject = (FLuaUObject*)luaL_checkudata(L, 1, UOBJECT_METATABLE);
	if (!LuaUObject->Source.IsValid())
	{
		return 0;
	}

	const char* PropertyName = lua_tostring(L, 2);
	UClass* Class = LuaUObject->Source->GetClass();

	if (UFunction* Function = Class->FindFunctionByName(PropertyName))
	{
		if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure))
		{
			luaL_error(L, "Function[%s] is not blueprint callable!", TCHAR_TO_UTF8(*Function->GetName()));
		}

		lua_pushlightuserdata(L, Function);
		lua_pushcclosure(L, CallUFunction, 1);

		return 1;
	}
	else if (UProperty* Property = Class->FindPropertyByName(PropertyName))
	{
		return FLuaObjectBase::PushProperty(L, Property, LuaUObject->Source.Get());
	}

	return 0;
}

int FLuaUObject::NewIndex(lua_State* L)
{
	FLuaUObject* LuaUObject = (FLuaUObject*)luaL_checkudata(L, 1, UOBJECT_METATABLE);
	if (!LuaUObject->Source.IsValid())
	{
		return 0;
	}

	const char* PropertyName = lua_tostring(L, 2);
	UClass* Class = LuaUObject->Source->GetClass();
	UProperty* Property = Class->FindPropertyByName(PropertyName);
	if (Property)
	{
		if (Property->PropertyFlags & CPF_BlueprintReadOnly)
		{
			luaL_error(L, "Can't write to a readonly property[%s] in object[%s]!", PropertyName, TCHAR_TO_UTF8(*(LuaUObject->Source->GetName())));
		}

		FLuaObjectBase::FetchProperty(L, Property, LuaUObject->Source.Get(), 3);
	}
	else
	{
		luaL_error(L, "Can't find property[%s] in object[%s]!", PropertyName, TCHAR_TO_UTF8(*(LuaUObject->Source->GetName())));
	}

	return 0;
}

int FLuaUObject::ToString(lua_State* L)
{
	FLuaUObject* LuaUObject = (FLuaUObject*)luaL_checkudata(L, 1, UOBJECT_METATABLE);

	lua_pushstring(L, TCHAR_TO_UTF8(*FString::Printf(TEXT("UObject[%s]"), LuaUObject->Source.IsValid() ? *(LuaUObject->Source->GetName()) : TEXT("null"))));

	return 1;
}

int FLuaUObject::CallUFunction(lua_State* L)
{
	UFunction* Function = (UFunction*)lua_touserdata(L, lua_upvalueindex(1));
	FLuaUObject* LuaUObject = (FLuaUObject*)luaL_checkudata(L, 1, UOBJECT_METATABLE);
	if (!LuaUObject->Source.IsValid())
	{
		return 0;
	}

	return FLuaObjectBase::CallFunction(L, LuaUObject->Source.Get(), Function);
}

int FLuaUObject::GC(lua_State* L)
{
	FLuaUObject* LuaUObject = (FLuaUObject*)luaL_checkudata(L, 1, UOBJECT_METATABLE);

	FLuaState* LuaStateWrapper = FLuaState::GetStateWrapper(L);
	if (LuaStateWrapper && LuaUObject && LuaUObject->bRef)
	{
		LuaStateWrapper->RemoveReference(LuaUObject->Source.Get());
	}

	LuaUObject->Source.Reset();

	return 0;
}
