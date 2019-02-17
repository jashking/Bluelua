R"[========](
--[[
Tencent is pleased to support the open source community by making LuaPanda available.
Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
Licensed under the BSD 3-Clause License (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at
https://opensource.org/licenses/BSD-3-Clause
Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

API:
    LuaPanda.printToVSCode()
        打印日志到VSCode Output下Debugger/log中

    LuaPanda.BP()
        强制打断点，可以在协程中使用。建议使用以下写法:
        local ret = LuaPanda and LuaPanda.BP and LuaPanda.BP();
        如果成功加入断点ret返回true，否则是nil

    LuaPanda.getInfo()
        返回获取调试器信息。包括版本号，是否使用lib库，系统是否支持loadstring(load方法)

    LuaPanda.getCWD()
        用户可以调用或在调试控制台中输出这个函数，返回帮助设置CWD的路径。比如
        cwd:      F:/1/2/3/4/5
        getinfo:  @../../../../../unreal_10/slua-unreal_1018/Content//Lua/TestArray.lua
        format:   F:/unreal_10/slua-unreal_1018/Content/Lua/TestArray.lua
        cwd是vscode传来的配置路径。getinfo是通过getinfo获取到的正在运行的文件路径。format是经过 cwd + getinfo 整合后的格式化路径。
        format是传给VSCode的最终路径。
        如果format路径和文件真实路径不符，导致VSCode找不到文件，通过调整工程中launch.json的cwd，使format路径和真实路径一致。

    LuaPanda.getBreaks()
        获取断点信息
]]

--用户设置项
local openAttachMode = true;            --是否开启attach模式。attach模式开启后可以在任意时刻启动vscode连接调试。缺点是不调试时也会略降低lua执行效率(会不断进行attach请求)
local attachInterval = 1;               --attach间隔时间(s)
local TCPSplitChar = "|*|";             --json协议分隔符，如果用户传输的数据中包含相同符号会造成协议被错误分割，保证和传输内容不同即可，如无问题不必修改
--用户设置项END

local debuggerVer = "1.9.15";                 --debugger版本号
LuaPanda = {};
local this = LuaPanda;
local tools = require("DebugTools");     --引用的开源工具，包括json解析和table展开工具
this.tools = tools;
this.curStackId = 0;
--json处理
local json = tools.createJson()
--hook状态列表
local hookState = {
    DISCONNECT_HOOK = 0,                --断开连接
    LITE_HOOK = 1,              --全局无断点
    MID_HOOK = 2,               --全局有断点，本文件无断点
    ALL_HOOK = 3,               --本文件有断点
};
--运行状态列表
local runState = {
    DISCONNECT = 0,             --未连接
    WAIT_CMD = 1,               --已连接，等待命令
    STOP_ON_ENTRY = 2,          --初始状态
    RUN = 3,
    STEPOVER = 4,
    STEPIN = 5,
    STEPOUT = 6,
    STEPOVER_STOP = 7,
    STEPIN_STOP = 8,
    STEPOUT_STOP = 9,
    HIT_BREAKPOINT = 10
};

local MAX_TIMEOUT_SEC = 3600 * 24;   --网络最大超时等待时间
--当前运行状态
local currentRunState;
local currentHookState;
--断点信息
local breaks = {};              --保存断点的数组
this.breaks = breaks;           --供hookLib调用
local recCallbackId = "";
--VSCode端传过来的配置，在VSCode端的launch配置，传过来并赋值
local luaFileExtension = "";    --脚本后缀
local cwd = "";                 --工作路径
local DebuggerFileName = "";    --Debugger文件名, 函数中会自动获取
local DebuggerToolsName = "";
local lastRunFunction = {};     --上一个执行过的函数。在有些复杂场景下(find,getcomponent)一行会挺两次
local currentCallStack = {};    --获取当前调用堆栈信息
local debugMode = false;        --是否开启调试模式
local hitBP = false;            --BP()中的强制断点命中标记
local TempFilePath_luaString = ""; --VSCode端配置的临时文件存放路径
local connectHost;              --记录连接端IP
local connectPort;              --记录连接端口号
local sock;                     --tcp socket
local OSType;                --VSCode识别出的系统类型
local clibPath;                 --chook库在VScode端的路径
local hookLib;                  --chook库的引用实例
--标记位
local logLevel = 1;             --日志等级all/info/error. 初始等级(从Adapter接收赋值之前) 为0
local variableRefIdx = 1;       --变量索引
local variableRefTab = {};      --变量记录table
local lastRunFilePath = "";     --最后执行的文件路径
local inDebugLayer = false;     --debug模式下，调入了Debug层级，用来做变量层级展示判断
local pathCaseSensitivity = 1;  --路径是否发大小写敏感，这个选项接收VScode设置，请勿在此处更改
local recvMsgQueue = {};        --接收的消息队列
local coroutinePool = {};       --保存用户协程的队列
--Step控制标记位
local stepOverCounter = 0;      --STEPOVER over计数器
local stepOutCounter = 0;       --STEPOVER out计数器
local HOOK_LEVEL = 3+2;         --调用栈偏移量
local isUseLoadstring = 0;
local debugger_loadString;
--临时变量
local coroutineCreate;          --用来记录lua原始的coroutine.create函数
local stopConnectTime = 0;      --用来临时记录stop断开连接的时间
local isInMainThread;
local receiveMsgTimer = 0;
local pathFormatCache = {};
--5.1/5.3兼容
if _VERSION == "Lua 5.1" then
    debugger_loadString = loadstring;
else
    debugger_loadString = load;
end

--用户在控制台输入信息的环境变量
local env = setmetatable({ }, { __index = function(tab1, varName)
    local ret =  this.getWatchedVariable( varName, _G.LuaPanda.curStackId , false);
    return ret;
end});

-----------------------------------------------------------------------------
-- 流程
-----------------------------------------------------------------------------

-- 启动调试器
-- @host adapter端ip, 默认127.0.0.1
-- @port adapter端port ,默认8818
function this.start(host, port)
    this.printToConsole("Debugger start", 1);
    host = tostring(host or "127.0.0.1") ;
    port = tonumber(port) or 8818;

    if sock ~= nil then
        this.printToConsole("[debugger warning]调试器已经启动，请不要再次调用start()");
        return;
    end

    --尝试初次连接
    this.changeRunState(runState.DISCONNECT);
    this.reGetSock();
    connectHost = host;
    connectPort = port;
    local sockSuccess = sock and sock:connect(connectHost, connectPort);
    if sockSuccess ~= nil then
        this.connectSuccess();
    else
        this.changeHookState(hookState.DISCONNECT_HOOK);
    end
end

-- 连接成功，开始初始化
function this.connectSuccess()
    this.changeRunState(runState.WAIT_CMD);
    this.printToConsole("connectSuccess");
    --设置初始状态
    local ret = this.debugger_wait_msg();

    --获取debugger文件路径
    if DebuggerFileName == "" then
        local info = debug.getinfo(1, "S")
        for k,v in pairs(info) do
            if k == "source" then
                DebuggerFileName = v;
                if hookLib ~= nil then
                    hookLib.sync_debugger_path(DebuggerFileName);
                end
            end
        end
    end
    if DebuggerToolsName == "" then 
        DebuggerToolsName = tools.getFileSource();
        if hookLib ~= nil then
            hookLib.sync_tools_path(DebuggerToolsName);
        end
    end

    if ret == false then
        this.printToVSCode("[debugger error]初始化未完成, 建立连接但接收初始化消息失败。请更换端口重试", 2);
        return;
    end
    this.printToVSCode("初始化完成")

    this.changeHookState(hookState.ALL_HOOK);
    if hookLib == nil then
        --协程调试
        if coroutineCreate == nil and type(coroutine.create) == "function" then
            this.printToConsole("change coroutine.create");
            coroutineCreate = coroutine.create;
            coroutine.create = function(...)
                local co =  coroutineCreate(...)
                table.insert(coroutinePool,  co);
                --运行状态下，创建协程即启动hook
                this.changeCoroutineHookState();
                return co;
            end
        else
            this.printToConsole("restart coroutine");
            this.changeCoroutineHookState();
        end
    end

end

--断开连接
function this.disconnect()
    this.changeHookState( hookState.DISCONNECT_HOOK );
    stopConnectTime = os.time();
    this.changeRunState(runState.DISCONNECT);

    if sock ~= nil then
        sock:close();
    end
    this.reGetSock();
end

-----------------------------------------------------------------------------
-- 调试器通用方法
-----------------------------------------------------------------------------
-- 返回断点信息
function this.getBreaks()
    return breaks;
end

-- 返回路径相关信息
-- cwd:配置的工程路径  |  info["source"]:通过 debug.getinfo 获得执行文件的路径  |  format：格式化后的文件路径
function this.getCWD()
    local ly = this.getSpecificFunctionStackLevel(lastRunFunction.func);
    if type(ly) ~= "number" then
        ly = 2;
    end
    local runSource = lastRunFunction["source"];
    if runSource == nil then
        runSource = this.getPath(hookLib.get_last_source());
    end
    local info = debug.getinfo(ly, "S");
    return "cwd:      "..cwd .."\ngetinfo:  ".. info["source"] .. "\nformat:   " .. tostring(runSource) ;
end

--返回版本号等配置
function this.getInfo()
    local retStr = "Lua Ver:" .. _VERSION .." | Debugger Ver:"..tostring(debuggerVer);
    if hookLib ~= nil then
        retStr = retStr.. " | hookLib Ver:" .. tostring(hookLib.sync_getLibVersion());
    end
    retStr = retStr .." | supportREPL:".. tostring(isUseLoadstring);
    return retStr;
end

--判断是否在协程中
function this.isInMain()
    return isInMainThread;
end

--添加路径，尝试引用库。完成后把cpath还原，返回引用结果true/false
-- @libName 库名
-- path lib的cpath路径
function this.tryRequireClib(libName , libPath)
    this.printToVSCode("tryRequireClib search : [" .. libName .. "] in "..libPath);

    local save_cpath = package.cpath;
    package.cpath = libPath;
    if pcall(function() hookLib = require(libName)  end) then
        package.cpath = save_cpath;
        this.printToVSCode("tryRequireClib success : [" .. libName .. "] in "..libPath);
        return true;
    end
    package.cpath = save_cpath;
    return false
end
------------------------字符串处理-------------------------
-- 倒序查找字符串 a.b/c查找/ , 返回4
-- @return 未找到目标串返回nil. 否则返回倒序找到的字串位置
function this.revFindString(str, subStr)
    local revStr = string.reverse(str);
    local _, idx = string.find(revStr, subStr)
    if idx == nil then return nil end;
    return string.len(revStr) - idx + 1;
end

-- 反序裁剪字符串 如:print(subString("a.b/c", "/"))输出c
-- @return 未找到目标串返回nil. 否则返回被裁剪后的字符串
function this.revSubString(str, subStr)
    local idx = this.revFindString(str, subStr)
    if idx == nil then return nil end;
    return string.sub(str, idx + 1, str.length)
end

-- 把字符串按reps分割成并放入table
-- @str 目标串
-- @reps 分割符
function this.stringSplit( str, separator )
    local retStrTable = {}
    string.gsub(str, '[^' .. separator ..']+', function ( word )
        table.insert(retStrTable, word)
    end)
    return retStrTable;
end

-- 保存CallbackId(通信序列号)
function this.setCallbackId( id )
    if id ~= nil and  id ~= "0" then
        recCallbackId = tostring(id);
    end
end

-- 读取CallbackId(通信序列号)。读取后记录值将被置空
function this.getCallbackId()
    if recCallbackId == nil then
        recCallbackId = "0";
    end
    local id = recCallbackId;
    recCallbackId = "0";
    return id;
end

-- reference from https://www.lua.org/pil/20.1.html
function this.trim (s)
    return (string.gsub(s, "^%s*(.-)%s*$", "%1"))
end

--返回table中成员数量(数字key和非数字key之和)
-- @t 目标table
-- @return 元素数量
function this.getTableMemberNum(t)
    if type(t) ~= "table" then
        this.printToVSCode("[debugger Error] getTableMemberNum get "..tostring(type(t)), 2)
        return nil;
    end
    local retNum = 0;
    for k,v in pairs(t) do
        retNum = retNum + 1;
    end
    return retNum;
end

-- 生成一个消息Table
function this.getMsgTable(cmd ,callbackId)
    callbackId = callbackId or 0;
    local msgTable = {};
    msgTable["cmd"] = cmd;
    msgTable["callbackId"] = callbackId;
    msgTable["info"] = {};
    return msgTable;
end

------------------------日志打印相关-------------------------
-- 把日志打印在VSCode端
-- @str: 日志内容
-- @printLevel: all(0)/info(1)/error(2)
-- @type: 0:vscode console  1:vscode tip
function this.printToVSCode(str, printLevel, type)
    type = type or 0;
    printLevel = printLevel or 0;
    if logLevel > printLevel then
        return;
    end 

    local sendTab = {};
    sendTab["callbackId"] = "0";
    if type == 0 then
        sendTab["cmd"] = "log";
    else
        sendTab["cmd"] =  "tip";
    end
    sendTab["info"] = {};
    sendTab["info"]["logInfo"] = tostring(str);
    this.sendMsg(sendTab);
end

-- 把日志打印在控制台
-- @str: 日志内容
-- @printLevel: all(0)/info(1)/error(2)
function this.printToConsole(str, printLevel)
    printLevel = printLevel or 0;
    if logLevel > printLevel then
        return;
    end
    print(str);
end
)[========]";