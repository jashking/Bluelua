#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

struct lua_State;
class ULuaDelegateCaller;

class BLUELUA_API FLuaState : public FGCObject, public TSharedFromThis<FLuaState>
{
public:
	FLuaState();
	virtual ~FLuaState();

	lua_State* GetState() const;

	bool DoBuffer(const uint8* Buffer, uint32 BufferSize, const char* Name = nullptr);
	bool DoString(const FString& String);
	bool DoFile(const FString& FilePath);
	bool CallLuaFunction(UFunction* SignatureFunction, void* Parameters, bool bWithSelf = true);
	bool CallLuaFunction(int32 InParamsCount, int32 OutParamsCount, bool bWithSelf = true);
	bool GetFromCache(void* InObject);
	bool AddToCache(void* InObject);

	void AddReference(UObject* Object, UObject* Owner);
	void RemoveReference(UObject* Object, UObject* Owner);
	void RemoveReferenceByOwner(UObject* Owner);
	void GetObjectsByOwner(UObject* Owner, TSet<UObject*>& Objects);

	// Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	// End FGCObject interface

	inline static FLuaState* GetStateWrapper(lua_State* InL);

protected:
	static int LuaError(lua_State* L);
	static int LuaPanic(lua_State* L);
	static int LuaPrint(lua_State* L);
	static int LuaSearcher(lua_State* L);
	static void* LuaAlloc(void* UserData, void* Ptr, size_t OldSize, size_t NewSize);

	static int LuaLoadClass(lua_State* L);
	static int LuaLoadStruct(lua_State* L);
	static int GetEnumValue(lua_State* L);

	static int FillOutProperty(lua_State* L);

	void OnPostGarbageCollect();

	static FString MakeRelativePathToContent(const FString& InPath);

protected:
	lua_State* L;

	int CacheObjectRefIndex;

	TMap<UObject*, TWeakObjectPtr<UObject>> ReferencedObjectsWithOwner;

	FDelegateHandle PostGarbageCollectDelegate;
};
