#pragma once

#include "CoreMinimal.h"

#include "LuaObjectBase.h"

class BLUELUA_API FLuaUStruct : public FLuaObjectBase
{
public:
	FLuaUStruct(UScriptStruct* InSource, uint8* InScriptBuffer, bool InbCopyValue);
	~FLuaUStruct();

	int32 GetStructureSize() const;

	static int Push(lua_State* L, UScriptStruct* InSource, void* InBuffer = nullptr, bool InbCopyValue = true);
	static bool Fetch(lua_State* L, int32 Index, UScriptStruct* OutStruct, uint8* OutBuffer);

protected:
	static int Index(lua_State* L);
	static int NewIndex(lua_State* L);
	static int GC(lua_State* L);
	static int ToString(lua_State* L);

	static class UProperty* FindStructPropertyByName(UScriptStruct* Source, FName Name);

protected:
	UScriptStruct* Source;
	uint8* ScriptBuffer;
	bool bCopyValue;

	static const char* USTRUCT_METATABLE;
};
