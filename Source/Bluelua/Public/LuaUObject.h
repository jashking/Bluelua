#pragma once

#include "CoreMinimal.h"

#include "LuaObjectBase.h"

class BLUELUA_API FLuaUObject : public FLuaObjectBase
{
public:
	FLuaUObject(UObject* InSource, UObject* InParent);
	~FLuaUObject();

	static int Push(lua_State* L, UObject* InSource, UObject* InParent = nullptr);
	static UObject* Fetch(lua_State* L, int32 Index);

	static int LuaLoadObject(lua_State* L);
	static int LuaUnLoadObject(lua_State* L);

protected:
	static int Index(lua_State* L);
	static int NewIndex(lua_State* L);
	static int ToString(lua_State* L);
	static int CallUFunction(lua_State* L);
	static int GC(lua_State* L);
	static int CastToLua(lua_State* L);
	static int IsValid(lua_State* L);

protected:
	TWeakObjectPtr<UObject> Source;
	UObject* Parent;

	static const char* UOBJECT_METATABLE;
};
