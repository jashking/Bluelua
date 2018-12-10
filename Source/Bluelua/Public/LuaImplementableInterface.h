#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"

#include "LuaImplementableInterface.generated.h"

class FLuaState;
struct lua_State;

UINTERFACE()
class BLUELUA_API ULuaImplementableInterface : public UInterface
{
	GENERATED_BODY()
};

class BLUELUA_API ILuaImplementableInterface
{
	GENERATED_BODY()

public:
	void PreInit(TSharedPtr<FLuaState> InLuaState = nullptr);
	bool IsLuaBound() const;
	bool FetchLuaModule();

	static void CleanAllLuaImplementableObject(FLuaState* InLuaState = nullptr);
	
protected:
	void PreRegisterLua(const FString& InLuaFilePath);

	virtual bool OnInit(const FString& InLuaFilePath, TSharedPtr<FLuaState> InLuaState = nullptr);
	virtual void OnRelease();
	virtual bool OnProcessEvent(UFunction* Function, void* Parameters);

	bool InitBPFunctionOverriding();
	void ClearBPFunctionOverriding();
	bool HasBPFunctionOverrding(const FString& FunctionName);
	bool PrepareLuaFunction(const FString& FunctionName);

	static void AddToLuaObjectList(FLuaState* InLuaState, ILuaImplementableInterface* Object);
	static void RemoveFromLuaObjectList(FLuaState* InLuaState, ILuaImplementableInterface* Object);

	static int FillBPFunctionOverrideOutProperty(struct lua_State* L);
	static void ProcessBPFunctionOverride(UObject* Context, struct FFrame& Stack, void* const Z_Param__Result);
	bool CallBPFunctionOverride(UFunction* Function, FFrame& Stack, void* const Z_Param__Result);

protected:
	TSharedPtr<FLuaState> LuaState;

	FString BindingLuaPath;

	int ModuleReferanceIndex = -2;

	TSet<FString> OverridedBPFunctionList;

	static TMap<FLuaState*, TSet<ILuaImplementableInterface*>> LuaImplementableObjects;
};
