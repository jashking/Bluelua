#include "LuaState.h"

#include "GenericPlatform/GenericPlatformMemory.h"
#include "HAL/UnrealMemory.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#include "Bluelua.h"
#include "lua.hpp"
#include "LuaDelegateCaller.h"
#include "LuaObjectBase.h"
#include "LuaStackGuard.h"
#include "LuaUClass.h"
#include "LuaUDelegate.h"
#include "LuaUObject.h"
#include "LuaUScriptStruct.h"

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("LuaMemory"), STAT_LuaMemory, STATGROUP_Bluelua);

FLuaState::FLuaState()
	: L(nullptr)
	, CacheObjectRefIndex(LUA_NOREF)
{
	L = lua_newstate(LuaAlloc, nullptr);
	if (L)
	{
		FLuaStackGuard Guard(L);

		lua_atpanic(L, LuaPanic);

		static const luaL_Reg LoadedLibs[] = {
			{ "_G", luaopen_base },
			{ LUA_LOADLIBNAME, luaopen_package },
			{ LUA_TABLIBNAME, luaopen_table },
			{ LUA_STRLIBNAME, luaopen_string },
			{ LUA_MATHLIBNAME, luaopen_math },
			{ LUA_UTF8LIBNAME, luaopen_utf8 },
			{ LUA_DBLIBNAME, luaopen_debug },
			{ NULL, NULL }
		};

		for (const luaL_Reg* Lib = LoadedLibs; Lib->func; ++Lib)
		{
			luaL_requiref(L, Lib->name, Lib->func, 1);
			lua_pop(L, 1);
		}

		// add custom searcher to the beginning of package.searchers: preload, custom, lua, c
		lua_pushcfunction(L, LuaSearcher);
		const int CustomSearcherIndex = lua_gettop(L);

		lua_getglobal(L, "package");
		lua_getfield(L, -1, "searchers");

		const int SearchersIndex = lua_gettop(L);

		for (int i = lua_rawlen(L, SearchersIndex) + 1; i > 2; --i)
		{
			lua_rawgeti(L, SearchersIndex, i - 1);
			lua_rawseti(L, SearchersIndex, i);
		}
		lua_pushvalue(L, CustomSearcherIndex);
		lua_rawseti(L, SearchersIndex, 2);

		lua_register(L, "print", LuaPrint);
		lua_register(L, "LoadObject", &FLuaUObject::LuaLoadObject);
		lua_register(L, "UnloadObject", &FLuaUObject::LuaUnLoadObject);
		lua_register(L, "LoadClass", LuaLoadClass);
		lua_register(L, "LoadStruct", LuaLoadStruct);
		lua_register(L, "GetEnum", GetEnumValue);
		lua_register(L, "CreateDelegate", &FLuaUDelegate::CreateDelegate);
		lua_register(L, "DeleteDelegate", &FLuaUDelegate::DeleteDelegate);
		lua_register(L, "CreateLatentAction", &FLuaUDelegate::CreateLatentAction);

		// bind this to L
		*((void**)lua_getextraspace(L)) = this;

		// weak cache table
		lua_newtable(L);
		lua_newtable(L);
		lua_pushstring(L, "kv");
		lua_setfield(L, -2, "__mode");
		lua_setmetatable(L, -2);
		CacheObjectRefIndex = luaL_ref(L, LUA_REGISTRYINDEX);

		UE_LOG(LogBluelua, Display, TEXT("Lua state created. LuaState[0x%x], L[0x%x]."), this, L);
	}

	PostGarbageCollectDelegate = FCoreUObjectDelegates::GetPostGarbageCollect().AddRaw(this, &FLuaState::OnPostGarbageCollect);
}

FLuaState::~FLuaState()
{
	FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGarbageCollectDelegate);

	if (L)
	{
		if (CacheObjectRefIndex != LUA_NOREF)
		{
			luaL_unref(L, LUA_REGISTRYINDEX, CacheObjectRefIndex);
		}

		CacheObjectRefIndex = LUA_NOREF;

		lua_close(L);
	}

	UE_LOG(LogBluelua, Display, TEXT("Lua state closed. LuaState[0x%x], L[0x%x]."), this, L);
	L = nullptr;
}

lua_State* FLuaState::GetState() const
{
	return L;
}

bool FLuaState::DoBuffer(const uint8* Buffer, uint32 BufferSize, const char* Name/* = nullptr*/)
{
	if (!L)
	{
		return false;
	}

	lua_pushcfunction(L, LuaError);
	const int32 LuaErrorFunctionIndex = lua_gettop(L);

	if (LUA_OK != luaL_loadbuffer(L, (const char *)Buffer, BufferSize, Name ? Name : (const char *)Buffer))
	{
		const char* ErrorInfo = lua_tostring(L, -1);
		UE_LOG(LogBluelua, Error, TEXT("Lua load buffer error: %s"), UTF8_TO_TCHAR(ErrorInfo));

		lua_pop(L, 2);

		return false;
	}

	const int LuaStatus = lua_pcall(L, 0, LUA_MULTRET, -2);
	if (LuaStatus != LUA_OK)
	{
		lua_pop(L, 1);
	}

	lua_remove(L, LuaErrorFunctionIndex);
	
	return (LuaStatus == LUA_OK);
}

bool FLuaState::DoString(const FString& String)
{
	if (!L)
	{
		return false;
	}

	const char* Buffer = TCHAR_TO_UTF8(*String);

	return DoBuffer((const uint8*)Buffer, FCStringAnsi::Strlen(Buffer));
}

bool FLuaState::DoFile(const FString& FilePath)
{
	if (!L)
	{
		return false;
	}

	TArray<uint8> FileContent;
	if (!FFileHelper::LoadFileToArray(FileContent, *FilePath))
	{
		return false;
	}

	return DoBuffer(FileContent.GetData(), FileContent.Num(), TCHAR_TO_UTF8(*FPaths::GetCleanFilename(FilePath)));
}

bool FLuaState::CallLuaFunction(UFunction* SignatureFunction, void* Parameters, bool bWithSelf/* = true*/)
{
	FLuaStackGuard Gurad(L);
	
	lua_pushcfunction(L, FLuaState::LuaError);
	const int32 LuaErrorFunctionIndex = bWithSelf ? lua_absindex(L, -3) : lua_absindex(L, -2);
	lua_insert(L, LuaErrorFunctionIndex);

	int32 InParamsCount = bWithSelf ? 1 : 0;
	int32 OutParamsCount = 0;

	UProperty* ReturnValue = nullptr;
	for (TFieldIterator<UProperty> ParamIter(SignatureFunction); ParamIter && (ParamIter->PropertyFlags & CPF_Parm); ++ParamIter)
	{
		UProperty* ParamProperty = *ParamIter;

		OutParamsCount += ((ParamIter->PropertyFlags & (CPF_ConstParm | CPF_OutParm)) == CPF_OutParm) ? 1 : 0;

		if (ParamIter->PropertyFlags & CPF_ReturnParm)
		{
			ReturnValue = ParamProperty;
		}
		else
		{
			++InParamsCount;
			FLuaObjectBase::PushProperty(L, ParamProperty, Parameters);
		}
	}

	if (LUA_OK != lua_pcall(L, InParamsCount, OutParamsCount, LuaErrorFunctionIndex))
	{
		return false;
	}

	if (ReturnValue || (SignatureFunction && SignatureFunction->HasAnyFunctionFlags(FUNC_HasOutParms)))
	{
		lua_pushinteger(L, OutParamsCount);
		lua_insert(L, -(OutParamsCount + 1));
		lua_pushlightuserdata(L, ReturnValue);
		lua_pushlightuserdata(L, SignatureFunction);
		lua_pushlightuserdata(L, Parameters);
		lua_pushcclosure(L, FillOutProperty, OutParamsCount + 4);

		if (LUA_OK != lua_pcall(L, 0, 0, LuaErrorFunctionIndex))
		{
			return false;
		}
	}

	return true;
}

bool FLuaState::GetFromCache(void* InObject)
{
	if (!InObject || !L || CacheObjectRefIndex == LUA_NOREF)
	{
		return false;
	}

	lua_rawgeti(L, LUA_REGISTRYINDEX, CacheObjectRefIndex);
	lua_pushlightuserdata(L, InObject);
	lua_rawget(L, -2);
	lua_remove(L, -2);

	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		return false;
	}

	return true;
}

bool FLuaState::AddToCache(void* InObject)
{
	if (!InObject || !L || CacheObjectRefIndex == LUA_NOREF)
	{
		return false;
	}

	lua_rawgeti(L, LUA_REGISTRYINDEX, CacheObjectRefIndex);
	lua_pushlightuserdata(L, InObject);
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	lua_pop(L, 1);

	return true;
}

void FLuaState::AddReference(UObject* Object)
{
	ReferencedObjects.Emplace(Object);
}

void FLuaState::AddReference(UObject* Object, UObject* Owner)
{
	ReferencedObjectsWithOwner.FindOrAdd(Object) = Owner;
}

void FLuaState::RemoveReference(UObject* Object)
{
	ReferencedObjects.Remove(Object);
}

void FLuaState::RemoveReference(UObject* Object, UObject* Owner)
{
	ReferencedObjectsWithOwner.Remove(Object);
}

void FLuaState::GetObjectsByOwner(UObject* Owner, TSet<UObject*>& Objects)
{
	for (auto Object : ReferencedObjectsWithOwner)
	{
		if (Object.Value.IsValid() && Object.Value.Get() == Owner && Object.Key->IsValidLowLevel())
		{
			Objects.Emplace(Object.Key);
		}
	}
}

void FLuaState::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(ReferencedObjects);
	Collector.AddReferencedObjects(ReferencedObjectsWithOwner);
}

FString FLuaState::GetReferencerName() const
{
	return TEXT("FLuaState Referencer");
}

FLuaState* FLuaState::GetStateWrapper(lua_State* InL)
{
	return InL ? (FLuaState*)*((void**)lua_getextraspace(InL)) : nullptr;
}

int FLuaState::LuaError(lua_State* L)
{
	luaL_traceback(L, L, lua_tostring(L, -1), 1);

	const char* ErrorInfoWithStack = lua_tostring(L, -1);
	UE_LOG(LogBluelua, Error, TEXT("%s"), UTF8_TO_TCHAR(ErrorInfoWithStack));

	return 1;
}

int FLuaState::LuaPanic(lua_State* L)
{
	UE_LOG(LogBluelua, Error, TEXT("PANIC: unprotected error in call to Lua API(%s)"), UTF8_TO_TCHAR(lua_tostring(L, -1)));
	return 0;
}

int FLuaState::LuaPrint(lua_State * L)
{
	FString Message;

	const int32 ParamsCount = lua_gettop(L);
	for (int32 Index = 1; Index <= ParamsCount; ++Index)
	{
		const char* StackMessage = lua_tostring(L, Index);
		if (!StackMessage)
		{
			if (lua_isboolean(L, Index))
			{
				StackMessage = lua_toboolean(L, Index) ? "true" : "false";
			}
			else
			{
				luaL_callmeta(L, Index, "__tostring");
				StackMessage = lua_tostring(L, -1);
			}
		}

		Message = FString::Printf(TEXT("%s%s%s"), *Message, Message.IsEmpty() ? TEXT("") : TEXT("\t"), UTF8_TO_TCHAR(StackMessage));
	}

	UE_LOG(LogBluelua, Display, TEXT("Lua log: %s"), *Message);

	return 0;
}

int FLuaState::LuaSearcher(lua_State* L)
{
	const FString FileName = UTF8_TO_TCHAR(lua_tostring(L, 1));
	const FString BaseFilePath = FPaths::Combine(FPaths::ProjectContentDir(), FileName.Replace(TEXT("."), TEXT("/")));

	FString FullFilePath;
	if (FPaths::FileExists(BaseFilePath + TEXT(".lua")))
	{
		FullFilePath = BaseFilePath + TEXT(".lua");
	}
	else if (FPaths::FileExists(BaseFilePath + TEXT(".luac")))
	{
		FullFilePath = BaseFilePath + TEXT(".luac");
	}
	else if (FPaths::FileExists(FPaths::Combine(BaseFilePath, TEXT("init.lua"))))
	{
		FullFilePath = FPaths::Combine(BaseFilePath, TEXT("init.lua"));
	}
	else if (FPaths::FileExists(FPaths::Combine(BaseFilePath, TEXT("init.luac"))))
	{
		FullFilePath = FPaths::Combine(BaseFilePath, TEXT("init.luac"));
	}
	else
	{
		UE_LOG(LogBluelua, Warning, TEXT("Lua require failed! File[%s] not exists!"), *FileName);
		return 0;
	}

	TArray<uint8> FileContent;
	if (!FFileHelper::LoadFileToArray(FileContent, *FullFilePath))
	{
		UE_LOG(LogBluelua, Warning, TEXT("Lua require failed! File[%s] load failed!"), *FileName);
		return 0;
	}

	if (LUA_OK != luaL_loadbuffer(L, (const char *)FileContent.GetData(), FileContent.Num(), TCHAR_TO_UTF8(*FPaths::GetCleanFilename(FullFilePath))))
	{
		const char* ErrorInfo = lua_tostring(L, -1);
		UE_LOG(LogBluelua, Error, TEXT("Lua require failed! Lua load buffer failed! %s"), UTF8_TO_TCHAR(ErrorInfo));

		return 0;
	}

	return 1;
}

void* FLuaState::LuaAlloc(void* UserData, void* Ptr, size_t OldSize, size_t NewSize)
{
	(void)UserData; /* not used */
	const size_t NewBytes = NewSize - (Ptr ? OldSize : 0);

	INC_DWORD_STAT_BY(STAT_LuaMemory, NewBytes);

	if (NewSize == 0)
	{
		FMemory::Free(Ptr);
		return nullptr;
	}
	else
	{
		return FMemory::Realloc(Ptr, NewSize);
	}
}

int FLuaState::LuaLoadClass(lua_State* L)
{
	const char* ClassName = lua_tostring(L, 1);
	if (ClassName)
	{
		const FString ClassPath = UTF8_TO_TCHAR(ClassName);

		UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassPath);
		if (!Class)
		{
			FString ObjectPath = ClassPath;

			ObjectPath.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);
			LoadObject<UObject>(nullptr, *ObjectPath);

			Class = FindObject<UClass>(ANY_PACKAGE, *ClassPath);
		}

		if (Class)
		{
			return FLuaUClass::Push(L, Class);
		}

		luaL_error(L, "Lua load class failed! Can't find class[%s]!", ClassName);
	}

	return 0;
}

int FLuaState::LuaLoadStruct(lua_State* L)
{
	const char* StructName = lua_tostring(L, 1);
	if (StructName)
	{
		UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(ANY_PACKAGE, UTF8_TO_TCHAR(StructName));
		if (!ScriptStruct)
		{
			ScriptStruct = LoadObject<UScriptStruct>(nullptr, UTF8_TO_TCHAR(StructName));
		}

		if (ScriptStruct)
		{
			return FLuaUScriptStruct::Push(L, ScriptStruct);
		}

		luaL_error(L, "Lua load struct failed! Can't find struct[%s]!", StructName);
	}

	return 0;
}

int FLuaState::GetEnumValue(lua_State* L)
{
	const char* StackEnumTypeValueName = lua_tostring(L, 1);
	if (!StackEnumTypeValueName)
	{
		luaL_error(L, "Get enum value failed! Require a valid name!");
	}

	const FString EnumTypeValueName = UTF8_TO_TCHAR(StackEnumTypeValueName);

	FString EnumType, ValueName;
	if (!EnumTypeValueName.Split(TEXT("."), &EnumType, &ValueName) || EnumType.IsEmpty() || ValueName.IsEmpty())
	{
		luaL_error(L, "Get enum value failed! Required format[EnumType.ValueName] got[%s]!", StackEnumTypeValueName);
	}

	UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, *EnumType);
	if (!Enum)
	{
		luaL_error(L, "Get enum value failed! Can't find enum type[%s]!", TCHAR_TO_UTF8(*EnumType));
	}

	const int32 EnumIndex = Enum->GetIndexByName(*ValueName);
	if (EnumIndex == INDEX_NONE)
	{
		luaL_error(L, "Get enum value failed! Can't find enum value[%s]!", TCHAR_TO_UTF8(*ValueName));
	}

	lua_pushinteger(L, Enum->GetValueByIndex(EnumIndex));

	return 1;
}

int FLuaState::FillOutProperty(lua_State* L)
{
	int32 OutParamsCount = lua_tointeger(L, lua_upvalueindex(1));
	UProperty* ReturnValue = (UProperty*)lua_touserdata(L, lua_upvalueindex(2 + OutParamsCount));
	UFunction* Function = (UFunction*)lua_touserdata(L, lua_upvalueindex(3 + OutParamsCount));
	void* Parameters = lua_touserdata(L, lua_upvalueindex(4 + OutParamsCount));

	int32 OutParamUpValueIndex = 2;
	if (ReturnValue)
	{
		FLuaObjectBase::FetchProperty(L, ReturnValue, Parameters, lua_upvalueindex(OutParamUpValueIndex));
		++OutParamUpValueIndex;
	}

	if (Function->HasAnyFunctionFlags(FUNC_HasOutParms))
	{
		for (TFieldIterator<UProperty> ParamIter(Function); ParamIter && ((ParamIter->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm); ++ParamIter)
		{
			if ((ParamIter->PropertyFlags & (CPF_ConstParm | CPF_OutParm)) == CPF_OutParm)
			{
				FLuaObjectBase::FetchProperty(L, *ParamIter, Parameters, lua_upvalueindex(OutParamUpValueIndex++));
			}
		}
	}

	return 0;
}

void FLuaState::OnPostGarbageCollect()
{
	TSet<UObject*> ObjectsNeedGC;

	for (auto Object : ReferencedObjectsWithOwner)
	{
		if (Object.Value.IsValid())
		{
			ObjectsNeedGC.Emplace(Object.Key);
		}
	}

	for (auto Object : ObjectsNeedGC)
	{
		ReferencedObjectsWithOwner.Remove(Object);
	}
}
