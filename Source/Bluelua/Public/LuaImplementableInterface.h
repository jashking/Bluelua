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
	bool IsLuaBound() const;
	bool CastToLua();
	bool HasBPFunctionOverrding(const FString& FunctionName);

	static void CleanAllLuaImplementableObject(FLuaState* InLuaState = nullptr);

	static void ProcessBPFunctionOverride(UObject* Context, struct FFrame& Stack, void* const Z_Param__Result);

protected:
	virtual bool OnInitLuaBinding();
	virtual void OnReleaseLuaBinding();
	virtual bool OnProcessLuaOverrideEvent(UFunction* Function, void* Parameters);

	UFUNCTION(BlueprintNativeEvent)
	FString OnInitBindingLuaPath();
	virtual FString OnInitBindingLuaPath_Implementation();

	UFUNCTION(BlueprintNativeEvent)
	bool ShouldEnableLuaBinding();
	virtual bool ShouldEnableLuaBinding_Implementation();

	virtual TSharedPtr<FLuaState> OnInitLuaState();

	bool InitBPFunctionOverriding();
	void ClearBPFunctionOverriding();
	bool PrepareLuaFunction(const FString& FunctionName);

	static void AddToLuaObjectList(FLuaState* InLuaState, ILuaImplementableInterface* Object);
	static void RemoveFromLuaObjectList(FLuaState* InLuaState, ILuaImplementableInterface* Object);

	static int FillBPFunctionOverrideOutProperty(struct lua_State* L);
	bool CallBPFunctionOverride(UFunction* Function, FFrame& Stack, void* const Z_Param__Result);

protected:
	TSharedPtr<FLuaState> LuaState;
	int ModuleReferanceIndex = -2;

	TSet<FString> OverridedBPFunctionList;

	static TMap<FLuaState*, TSet<ILuaImplementableInterface*>> LuaImplementableObjects;
};
