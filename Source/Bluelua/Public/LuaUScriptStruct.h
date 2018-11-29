#pragma once

#include "CoreMinimal.h"

#include "LuaObjectBase.h"

class BLUELUA_API FLuaUScriptStruct : public FLuaObjectBase
{
public:
	FLuaUScriptStruct(UScriptStruct* InSource);
	~FLuaUScriptStruct();

	static int Push(lua_State* L, UScriptStruct* InSource);

protected:
	static int Construct(lua_State* L);
	static int ToString(lua_State* L);

protected:
	TWeakObjectPtr<UScriptStruct> Source;
	
	static const char* USCRIPTSTRUCT_METATABLE;
};
