# Bluelua for UE4 #

Replace the blueprint with Lua, keep it in a consistent way with the blueprint, and switch seamlessly. Accessing UObject's properties and methods with reflection and without the need to generate glue code, more simple and easy to expand.

Android, iOS, Mac, Windows, Linux are supported now.

## Used Open Source Libraries ##

* [lua](https://www.lua.org/) : Lua is a powerful, efficient, lightweight, embeddable scripting language
* [luasocket](https://github.com/diegonehab/luasocket) : Network support for the Lua language
* [Tencent LuaPanda](https://github.com/Tencent/LuaPanda) : Pandora Lua Debugger for VS Code

## How to use ##

Clone Bluelua to your project's *Plugins* folder, regenerate project solution and build.

### How to inherit C++ with lua ###

* `ILuaImplementableInterface`

    All C++ classes that can be used with LUA subclasses need to be inherited from this interface and the corresponding methods are called in the corresponding virtual functions, the main types of overrideing are as follows:

    All C++ classes (or blueprint classes) need to override two important `BlueprintNativeEvent`: `ShouldEnableLuaBinding`: should use lua subclass on this C++/Blueprint class. `OnInitBindingLuaPath`: return corresponding lua file path
    
    It's optional to override `OnInitLuaState`: return custom FLuaState wrapper, default use of global lua state

    * Class `AActor`, see [`LuaImplementableActor.h`](https://github.com/jashking/Bluelua/blob/master/Source/Bluelua/Public/LuaImplementableActor.h)
        * Call `OnInitLuaBinding` in `BeginPlay`
        * Call `OnReleaseLuaBinding` in `EndPlay`
        * Call `OnProcessLuaOverrideEvent` in `ProcessEvent`

    * Class `UUserWidget`, see [`LuaImplementableWidget.h`](https://github.com/jashking/Bluelua/blob/master/Source/Bluelua/Public/LuaImplementableWidget.h)
        * Call `OnInitLuaBinding` in `NativeConstruct`
        * Call `OnReleaseLuaBinding` in `NativeDestruct`
        * Call `OnProcessLuaOverrideEvent` in `ProcessEvent`

        The UUserWidget class also has a special place where the `LatentAction` owned by the class is not affected by the pause, so you also need to override `NativeTick` and call `LatentActionManager` in the virtual function to handle all `LatentAction` objects. See `TickActions` in [`LuaImplementableWidget.h`](https://github.com/jashking/Bluelua/blob/master/Source/Bluelua/Public/LuaImplementableWidget.h)

    * Class `UObject`, see [`RPGAnimNotifyState.h`](https://github.com/jashking/LuaActionRPG/blob/master/Source/ActionRPG/Public/RPGAnimNotifyState.h) in [Demo LuaActionRPG](https://github.com/jashking/LuaActionRPG)
        * Call `OnReleaseLuaBinding` in `BeginDestroy`
        * Call `OnProcessLuaOverrideEvent` in `ProcessEvent`
        * Because the Uobject class does not have an initialized virtual function that can be overrided, so it can call `OnInitLuaBinding` the first time the `ProcessEvent` is called, as shown in the demo

    * Class `UGameInstance`, see [`RPGGameInstanceBase.h`](https://github.com/jashking/LuaActionRPG/blob/master/Source/ActionRPG/Public/RPGGameInstanceBase.h) in [Demo LuaActionRPG](https://github.com/jashking/LuaActionRPG)
        * Call `OnInitLuaBinding` in `Init`
        * Call `OnReleaseLuaBinding` in `Shutdown`
        * Call `OnProcessLuaOverrideEvent` in `ProcessEvent`

* keyword `Super`

    When you use Lua to subclass c++ classes or blueprints, there is a temporary global object called Super represent the parent UObject that you can access it's properties and methods. It's only avaliable when lua file is loaded(`luaL_loadbuffer`), so you need to cache it's reference in your lua file like `local MyParent = Super` and use `MyParent` in your lua

* `CastToLua`

    When you have a UObject in lua and want to know if the object has a lua subclass, you can call `CastToLua` on that object like `local LuaObject = OtherObject:CastToLua()`, it returns a lua table represent the lua object if `OtherObject` has a lua subclass. `CastToLua` works like cast in blueprint:

    ![](Doc/Images/cast.png)

* Modular lua subcalss

    All lua subclass should implement in modular table and methods should declare with **:** not **.**, see see [`GameInstance.lua`](https://github.com/jashking/LuaActionRPG/blob/master/Content/Lua/Blueprints/GameInstance.lua) in [Demo LuaActionRPG](https://github.com/jashking/LuaActionRPG)

    ``` lua
    local m = {}

    function m:func()
    end

    return m
    ```

* inheritance in lua

    You can use lua metatable to implement inheritance like you do in C++ or blueprint, see
    [PlayerCharacter.lua](https://github.com/jashking/LuaActionRPG/blob/master/Content/Lua/Blueprints/PlayerCharacter.lua) and [Character.lua](https://github.com/jashking/LuaActionRPG/blob/master/Content/Lua/Blueprints/Character.lua) in [Demo LuaActionRPG](https://github.com/jashking/LuaActionRPG)

* When a UFUNCTION expose to blueprint, it can has a alias, like `BeginPlay` in blueprint, it actually called `ReceiveBeginPlay`. so when you override this function in lua you should use `ReceiveBeginPlay` not `BeginPlay`.

## Samples ##

* [LuaActionRPG](https://github.com/jashking/LuaActionRPG): Epic's ActionRPG demo in lua implementation, still work in progress
* [BlueluaDemo](https://github.com/jashking/BlueluaDemo): benchmark and test

## TODO ##

* expose lua file reader to project, do not assume lua files under ProjectContent folder
* hot reload lua

## 中文版使用介绍 ##

[使用介绍](https://github.com/jashking/Bluelua/wiki/%E5%A6%82%E4%BD%95%E4%BD%BF%E7%94%A8)