#pragma once

#include "CoreMinimal.h"

#include "LuaUDelegate.h"

class BLUELUA_API FLuaScriptDelegate : public FLuaUDelegate
{
public:
	FLuaScriptDelegate(void* InSource, UFunction* InFunction);
	virtual ~FLuaScriptDelegate();

	static FLuaUDelegate* Create(lua_State* L, void* InSource, UFunction* InFunction);

protected:
	virtual void OnAdd(ULuaFunctionDelegate* LuaFunctionDelegate) override;
	virtual void OnRemove(ULuaFunctionDelegate* LuaFunctionDelegate) override;
	virtual void OnClear() override;
	virtual TArray<UObject*> OnGetAllObjects() const override;
	virtual FString OnGetName() override;
};
