#pragma once

#include "CoreMinimal.h"

#include "LuaObjectBase.h"

class BLUELUA_API FLuaUClass : public FLuaObjectBase
{
public:
	FLuaUClass(UClass* InSource);
	~FLuaUClass();

	static int Push(lua_State* L, UClass* InSource);
	static UClass* Fetch(lua_State* L, int32 Index);

protected:
	static int Construct(lua_State* L);
	static int Index(lua_State* L);
	static int NewIndex(lua_State* L);
	static int ToString(lua_State* L);
	static int CallStaticUFunction(lua_State* L);

protected:
	TWeakObjectPtr<UClass> Source;

	static const char* UCLASS_METATABLE;
};