R"[========](
--接收消息
--这里维护一个队列
-- @timeoutSec 超时时间
-- @return  boolean 成功/失败
function this.receiveMessage( timeoutSec )
    timeoutSec = timeoutSec or MAX_TIMEOUT_SEC;
    sock:settimeout(timeoutSec);
    --如果队列中还有消息，直接取出来交给dataProcess处理
    if #recvMsgQueue > 0 then
        local saved_cmd = recvMsgQueue[1];
        table.remove(recvMsgQueue, 1);
        this.dataProcess(saved_cmd);
        return true;
    end

    if currentRunState == runState.DISCONNECT then
        this.disconnect();
        return false;
    end

    if sock == nil then
        print("[debugger error]接收信息失败  |  reason: socket == nil");
        return;
    end
    local response, err = sock:receive();
    if response == nil then
        if err == "closed" then
            print("[debugger error]接收信息失败  |  reason:"..err);
            this.disconnect();
        end
        return false;
    else

        --判断是否是一条消息，分拆
        local proc_response = string.sub(response, 1, -1 * (TCPSplitChar:len() + 1 ));
        local match_res = string.find(proc_response, TCPSplitChar);
        if match_res == nil then
            --单条
            this.dataProcess(proc_response);
        else
            --有粘包
            repeat
                --待处理命令
                local str1 = string.sub(proc_response, 1, match_res - 1);
                table.insert(recvMsgQueue, str1);
                --剩余匹配
                local str2 = string.sub(proc_response, match_res + TCPSplitChar:len() , -1);
                match_res = string.find(str2, TCPSplitChar);
            until not match_res
            this.receiveMessage();
        end
        return true;
    end
    return false;
end

--这里不用循环，在外面处理完消息会在调用回来
-- @timeoutSec 等待时间s
-- @entryFlag 入口标记，用来标识是从哪里调入的
function this.debugger_wait_msg(timeoutSec)
    timeoutSec = timeoutSec or MAX_TIMEOUT_SEC;

    if currentRunState == runState.WAIT_CMD then
        local ret = this.receiveMessage(timeoutSec);
        return ret;
    end

    if currentRunState == runState.STEPOVER or 
    currentRunState == runState.STEPIN or 
    currentRunState == runState.STEPOUT or 
    currentRunState == runState.RUN then
        this.receiveMessage(0);
        return
    end

    if currentRunState == runState.STEPOVER_STOP or 
    currentRunState == runState.STEPIN_STOP or 
    currentRunState == runState.STEPOUT_STOP or 
    currentRunState == runState.HIT_BREAKPOINT or
    currentRunState == runState.STOP_ON_ENTRY
    then
        this.sendLuaMemory();
        this.receiveMessage(MAX_TIMEOUT_SEC);
        return
    end
end

-----------------------------------------------------------------------------
-- 调试器核心方法
-----------------------------------------------------------------------------

------------------------堆栈管理-------------------------


--getStackTable需要建立stackTable，保存每层的lua函数实例(用来取upvalue)，保存函数展示层级和ly的关系(便于根据前端传来的stackId查局部变量)
-- @level 要获取的层级
function this.getStackTable( level )
    local i = level or HOOK_LEVEL;
    local stackTab = {};
    local userFuncSteakLevel = 0; --用户函数的steaklevel
    repeat
        local info = debug.getinfo(i, "SlLnf")
        if info == nil then
            break;
        end
        if info.source == "=[C]" then
            break;
        end

        local ss = {};
        ss.file = this.getPath(info);
        ss.name = "文件名"; --这里要做截取
        ss.line = tostring(info.currentline);
        --使用hookLib时，堆栈有偏移量，这里统一调用栈顶编号2
        local ssindex = i - 3;
        if hookLib ~= nil then
            ssindex = ssindex + 2;
        end
        ss.index = tostring(ssindex);
        table.insert(stackTab,ss);
        --把数据存入currentCallStack
        local callStackInfo = {};
        callStackInfo.name = ss.file;
        callStackInfo.line = ss.line;
        callStackInfo.func = info.func;     --保存的function
        callStackInfo.realLy = i;              --真实堆栈层i(仅debug时用)
        table.insert(currentCallStack, callStackInfo);

        --level赋值
        if userFuncSteakLevel == 0 then
            userFuncSteakLevel = i;
        end
        i = i + 1;
    until info == nil
    return stackTab, userFuncSteakLevel;
end

--这个方法是根据工程中的cwd和luaFileExtension修改
-- @info getInfo获取的包含调用信息table
function this.getPath( info )
    local filePath = info;
    if type(info) == "table" then
        filePath = info.source;
    end
    --尝试从Cache中获取路径
    local cachePath = this.getPathFromCache(filePath);
    if cachePath~= nil and type(cachePath) == "string" then
        return cachePath;
    end

    local originalPath = filePath;
    --判断@
    if filePath:sub(1,1) == '@' then
        filePath = filePath:sub(2);
    end

    --后缀处理
    if luaFileExtension ~= "" then
        local checkExtension = string.sub(filePath, -6, -1)
        local ExtensionMatchRes = this.revFindString(checkExtension, "%.");
        if ExtensionMatchRes ~= nil then
            --去后缀.这里的完整公式是(-1)*(6 - ExtensionMatchRes + 2)
            filePath = string.sub(filePath, 1 , -6 - 2 + ExtensionMatchRes);
        end
        filePath = filePath.."."..luaFileExtension;
    end

    --拼路径
    --若在Mac下以/开头，或者在Win下以*:开头，说明是绝对路径，不需要再拼。直接返回
    if filePath:sub(1,1) == [[/]]  then
        return this.genUnifiedPath(filePath);
    end

    if filePath:sub(1,2):match("%a:") then
        return this.genUnifiedPath(filePath);
    end

    --需要拼接
    local retPath = filePath;
    if cwd ~= "" then
        --这里做一次匹配,文件名不能出现符号，否则会匹配错误
        local matchRes = string.match(filePath, cwd);
        if matchRes == nil then
            retPath = cwd.."/"..filePath;
        end
        --检查一下是否出现/./
        retPath = this.genUnifiedPath(retPath);
    end

    --放入Cache中缓存
    this.setPathToCache(originalPath, retPath);
    return retPath;
end

--获取当前函数的堆栈层级
--原理是向上查找，遇到DebuggerFileName就调过。但是可能存在代码段和C导致不确定性。目前使用getSpecificFunctionStackLevel代替。
function this.getCurrentFunctionStackLevel()
    -- print(debug.traceback("===getCurrentFunctionStackLevel Stack trace==="))
    local funclayer = 2;
    repeat
        local info = debug.getinfo(funclayer, "S"); --通过name来判断
        if info ~= nil then
            local matchRes = ((info.source == DebuggerFileName) or (info.source == DebuggerToolsName));
            if matchRes == false then
                return (funclayer - 1);
            end
        end
        funclayer = funclayer + 1;
    until not info
    return 0;
end

--获取指定函数的堆栈层级
--通常用来获取最后一个用户函数的层级，用法是从currentCallStack取用户点击的栈，再使用本函数取对应层级。
-- @func 被获取层级的function
function this.getSpecificFunctionStackLevel( func )
    local funclayer = 2;
    repeat
        local info = debug.getinfo(funclayer, "f"); --通过name来判断
        if info ~= nil then
            if info.func == func then
                return (funclayer - 1);
            end
        end
        funclayer = funclayer + 1;
    until not info
    return 0;
end

--检查当前堆栈是否是Lua
-- @checkLayer 指定的栈层
function this.checkCurrentLayerisLua( checkLayer )
    local info = debug.getinfo(checkLayer, "S");
    if info == nil then
        return nil;
    end
    info.source = this.genUnifiedPath(info.source);
    -- print("info.source  " ..info.source )
    if info ~= nil then
        for k,v in pairs(info) do
            if k == "what" then
                if v == "C" then
                    return false;
                else
                    return true;
                end
            end
        end
    end
    return nil;
end

------------------------断点处理-------------------------
-- 参数info是当前堆栈信息
-- @info getInfo获取的当前调用信息
function this.isHitBreakpoint( info )
  --  tools.printTable(breaks,"breaks");

    local curPath = info.source;
    local curLine = tostring(info.currentline);
    if breaks[curPath] ~= nil then
        for k,v in ipairs(breaks[curPath]) do
            if tostring(v["line"]) == tostring(curLine) then
                return true;
            end
        end
    end
    return false;
end

--加入断点函数
function this.BP()
    this.printToConsole("BP()");
    if hookLib == nil then
        if currentHookState == hookState.DISCONNECT_HOOK then
            this.printToConsole("BP() but NO HOOK");
            return;
        end

        local co, isMain = coroutine.running();
        if _VERSION == "Lua 5.1" then
            if co == nil then
                isMain = true;
            else
                isMain = false;
            end
        end

        if isMain == true then
            this.printToConsole("BP() in main");
        else
            this.printToConsole("BP() in coroutine");
            debug.sethook(co, this.debug_hook, "lrc");
        end
        hitBP = true;
    else
        if hookLib.get_libhook_state() == hookState.DISCONNECT_HOOK then
            this.printToConsole("BP() but NO C HOOK");
            return;
        end

        --clib, set hitBP
        hookLib.sync_bp_hit(1);
    end
    this.changeHookState(hookState.ALL_HOOK);
    return true;
end

-- 检查当前文件中是否有断点
-- 如果填写参数fileName  返回fileName中有无断点， 全局有无断点
-- fileName为空，返回全局是否有断点
function this.checkHasBreakpoint(fileName)
    local hasBk = true;
    --有无全局断点
    if next(breaks) == nil then
        hasBk = false;
    else
        hasBk = true;
    end
    --当前文件中是否有断点
    if fileName ~= nil then
        return breaks[fileName] ~= nil, hasBk;
    else
        return hasBk;
    end
end

function this.checkfuncHasBreakpoint(sLine, eLine, fileName)
    if breaks[fileName] == nil then
        return false;
    end
    sLine = tonumber(sLine);
    eLine = tonumber(eLine);

    --起始行号>结束行号，或者sLine = eLine = 0
    if sLine >= eLine then
        return true;
    end

    if #breaks[fileName] <= 0 then
        return false;
    else
        for k,v in ipairs(breaks[fileName]) do
            if tonumber(v.line) > sLine and tonumber(v.line) <= eLine then
                return true;
            end
        end
    end
    return false;
end
------------------------HOOK模块-------------------------
-- 钩子函数
-- @event 执行状态(call,return,line)
-- @line    行号
function this.debug_hook(event, line)
    if this.reConnect() == 1 then return; end

    if logLevel == 0 then 
        local logTable = {"-----enter debug_hook-----\n", "event:", event, "  line:", tostring(line), " currentHookState:",currentHookState," currentRunState:", currentRunState};
        local logString = table.concat(logTable);
        this.printToVSCode(logString);
    end

    --litehook 仅非阻塞接收断点
    if currentHookState ==  hookState.LITE_HOOK then
        local ti = os.time();
        if ti - receiveMsgTimer > 1 then
            this.debugger_wait_msg(0);
            receiveMsgTimer = ti;
        end
        return;
    end

    --运行中
    local info;
    local co, isMain = coroutine.running();
    if _VERSION == "Lua 5.1" then
        if co == nil then
            isMain = true;
        else
            isMain = false;
        end
    end
    isInMainThread = isMain;
    if isMain == true then
        info = debug.getinfo(2, "Slf")
    else
        info = debug.getinfo(co, 2, "Slf")
    end
    info.event = event;
    this.real_hook_process(info);
end
)[========]";