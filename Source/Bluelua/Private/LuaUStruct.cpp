#include "LuaUStruct.h"

#include "HAL/UnrealMemory.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "Bluelua.h"
#include "lua.hpp"

DECLARE_CYCLE_STAT(TEXT("StructPush"), STAT_StructPush, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("StructIndex"), STAT_StructIndex, STATGROUP_Bluelua);
DECLARE_CYCLE_STAT(TEXT("StructNewIndex"), STAT_StructNewIndex, STATGROUP_Bluelua);

const char* FLuaUStruct::USTRUCT_METATABLE = "UStruct_Metatable";

FLuaUStruct::FLuaUStruct(UScriptStruct* InSource, uint8* InScriptBuffer, bool InbCopyValue)
	: Source(InSource)
	, ScriptBuffer(InScriptBuffer)
	, bCopyValue(InbCopyValue)
{

}

FLuaUStruct::~FLuaUStruct()
{

}

int32 FLuaUStruct::GetStructureSize() const
{
	return Source.IsValid() ? Source->GetStructureSize() : 0;
}

int FLuaUStruct::Push(lua_State* L, UScriptStruct* InSource, void* InBuffer /*= nullptr*/, bool InbCopyValue/* = true*/)
{
	SCOPE_CYCLE_COUNTER(STAT_StructPush);

	if (!InSource)
	{
		lua_pushnil(L);
		return 1;
	}

	uint8* UserData = (uint8*)lua_newuserdata(L, sizeof(FLuaUStruct));

	uint8* ScriptBuffer = (uint8*)InBuffer;
	if (InbCopyValue)
	{
		ScriptBuffer = (uint8*)FMemory::Malloc(InSource->GetStructureSize());
		InSource->InitializeStruct(ScriptBuffer);

		if (InBuffer)
		{
			InSource->CopyScriptStruct(ScriptBuffer, InBuffer);
		}
	}

	FLuaUStruct* LuaUStruct = new(UserData) FLuaUStruct(InSource, ScriptBuffer, InbCopyValue);

	if (luaL_newmetatable(L, USTRUCT_METATABLE))
	{
		static struct luaL_Reg Metamethods[] =
		{
			{ "__index", Index },
			{ "__newindex", NewIndex },
			{ "__gc", GC },
			{ "__tostring", ToString },
			{ NULL, NULL },
		};

		luaL_setfuncs(L, Metamethods, 0);
	}

	lua_setmetatable(L, -2);

	return 1;
}

bool FLuaUStruct::Fetch(lua_State* L, int32 Index, UScriptStruct* OutStruct, uint8* OutBuffer)
{
	if (!OutStruct || lua_isnil(L, Index))
	{
		return false;
	}

	FLuaUStruct* LuaUStruct = (FLuaUStruct*)luaL_checkudata(L, Index, FLuaUStruct::USTRUCT_METATABLE);

	//const int32 TargetSize = StructProperty->Struct->GetStructureSize();
	//const int32 SourceSize = LuaUStruct->GetStructureSize();

	//if (TargetSize <= SourceSize)
	{
		OutStruct->CopyScriptStruct(OutBuffer, LuaUStruct->ScriptBuffer);
		return true;
	}
	//else
	//{
	//	UE_LOG(LogBluelua, Warning,
	//		TEXT("Fetch property[%s] struct at index[%d] failed! Structure size mismatch! TargetSize[%d], SourceSize[%d]."),
	//		*Property->GetName(), Index, TargetSize, SourceSize);

	//	return false;
	//}
}

int FLuaUStruct::Index(lua_State* L)
{
	SCOPE_CYCLE_COUNTER(STAT_StructIndex);

	FLuaUStruct* LuaUStruct = (FLuaUStruct*)luaL_checkudata(L, 1, USTRUCT_METATABLE);
	if (!LuaUStruct->Source.IsValid())
	{
		return 0;
	}

	const char* PropertyName = lua_tostring(L, 2);
	if (UProperty* Property = FindStructPropertyByName(LuaUStruct->Source.Get(), PropertyName))
	{
		return FLuaObjectBase::PushProperty(L, Property, Property->ContainerPtrToValuePtr<uint8>(LuaUStruct->ScriptBuffer), nullptr, false);
	}

	return 0;
}

int FLuaUStruct::NewIndex(lua_State* L)
{
	SCOPE_CYCLE_COUNTER(STAT_StructNewIndex);

	FLuaUStruct* LuaUStruct = (FLuaUStruct*)luaL_checkudata(L, 1, USTRUCT_METATABLE);
	if (!LuaUStruct->Source.IsValid())
	{
		return 0;
	}

	const char* PropertyName = lua_tostring(L, 2);
	UProperty* Property = FindStructPropertyByName(LuaUStruct->Source.Get(), PropertyName);
	if (Property)
	{
		if (Property->PropertyFlags & CPF_BlueprintReadOnly)
		{
			luaL_error(L, "Can't write to a readonly property[%s] in struct[%s]!", PropertyName, TCHAR_TO_UTF8(*(LuaUStruct->Source->GetName())));
		}

		FLuaObjectBase::FetchProperty(L, Property, Property->ContainerPtrToValuePtr<uint8>(LuaUStruct->ScriptBuffer), 3);
	}
	else
	{
		luaL_error(L, "Can't find property[%s] in struct[%s]!", PropertyName, TCHAR_TO_UTF8(*(LuaUStruct->Source->GetName())));
	}

	return 0;
}

int FLuaUStruct::GC(lua_State* L)
{
	FLuaUStruct* LuaUStruct = (FLuaUStruct*)luaL_checkudata(L, 1, USTRUCT_METATABLE);

	if (LuaUStruct->bCopyValue)
	{
		if (LuaUStruct->Source.IsValid() && LuaUStruct->ScriptBuffer)
		{
			LuaUStruct->Source->DestroyStruct(LuaUStruct->ScriptBuffer);
		}

		if (LuaUStruct->ScriptBuffer)
		{
			FMemory::Free(LuaUStruct->ScriptBuffer);
			LuaUStruct->ScriptBuffer = nullptr;
		}
	}

	return 0;
}

int FLuaUStruct::ToString(lua_State* L)
{
	FLuaUStruct* LuaUStruct = (FLuaUStruct*)luaL_checkudata(L, 1, USTRUCT_METATABLE);

	lua_pushstring(L, TCHAR_TO_UTF8(*FString::Printf(TEXT("UStruct[%s]"), LuaUStruct->Source.IsValid() ? *(LuaUStruct->Source->GetName()) : TEXT("null"))));

	return 1;
}

class UProperty* FLuaUStruct::FindStructPropertyByName(UScriptStruct* Source, FName Name)
{
	UProperty* Property = Source->FindPropertyByName(Name);
	if (Property)
	{
		return Property;
	}

	for (Property = Source->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext)
	{
		if (Property->GetFName().ToString().StartsWith(Name.ToString(), ESearchCase::CaseSensitive))
		{
			return Property;
		}
	}

	return nullptr;
}
