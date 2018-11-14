# Bluelua for UE4 #

用 Lua 替换蓝图脚本，保持和蓝图一致的使用方式，无缝切换

## 使用 ##

复制到项目 *Plugins* 目录下即可

### 使用约定 ###

* Super

    用 Lua 去子类化 Widget 或者 Actor 时候，会传入一个临时父类对象 Super，需要自己去声明一个本地变量来引用

## Samples ##

* [BlueluaDemo](https://github.com/jashking/BlueluaDemo): 性能对比测试和简单用法

## TODO ##

* require (search path)
* statistics
* call BlueprintNativeEvent default implemention
* call lua function in blueprint