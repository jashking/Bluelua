#pragma once

#include "CoreMinimal.h"

#include "LuaObjectBase.h"

class BLUELUA_API FLuaUObject : public FLuaObjectBase
{
public:
	FLuaUObject(UObject* InSource, bool InbOwnedByLua);
	~FLuaUObject();

	static int Push(lua_State* L, UObject* InSource, bool bOwnedByLua = false);
	static UObject* Fetch(lua_State* L, int32 Index);

protected:
	static int Index(lua_State* L);
	static int NewIndex(lua_State* L);
	static int ToString(lua_State* L);
	static int CallUFunction(lua_State* L);
	static int GC(lua_State* L);

protected:
	TWeakObjectPtr<UObject> Source;
	bool bOwnedByLua;

	static const char* UOBJECT_METATABLE;
};
