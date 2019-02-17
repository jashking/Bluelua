R"[========](
function this.real_hook_process(info)
    local jumpFlag = false;
    local event = info.event;

    --如果当前行在Debugger中，不做处理
    local matchRes = ((info.source == DebuggerFileName) or (info.source == DebuggerToolsName));
    if matchRes == true and debugMode == false then
        return;
    end

    --即使MID hook在C中, 或者是Run或者单步时也接收消息
    if currentRunState == runState.RUN or
    currentRunState == runState.STEPOVER or
    currentRunState == runState.STEPIN or
    currentRunState == runState.STEPOUT then
        local ti = os.time();
        if ti - receiveMsgTimer > 1 then
            this.debugger_wait_msg(0);
            receiveMsgTimer = ti;
        end
    end

    --不处理C函数
    if info.source == "=[C]" then
        this.printToVSCode("current method is C");
        return;
    end

    --lua 代码段的处理，目前暂不调试代码段。
    if info.short_src:match("%[string \"")  then
            --当shortSrc中出现[string时]。要检查一下source, 区别是路径还是代码段. 方法是看路径中有没有\t \n ;
            if info.source:match("[\n;=]") then
                --是代码段，调过
                this.printToVSCode("hook jump Code String!");
                jumpFlag = true;
            end
    end

    --标准路径处理
    if jumpFlag == false then
        info.source = this.getPath(info);
    end
    --本次执行的函数和上次执行的函数作对比，防止在一行停留两次
    if lastRunFunction["currentline"] == info["currentline"] and lastRunFunction["source"] == info["source"] and lastRunFunction["func"] == info["func"] and lastRunFunction["event"] == event then
        this.printToVSCode("run twice");
    end
    --记录最后一次调用信息
    if jumpFlag == false then
        lastRunFunction = info;
        lastRunFunction["event"] = event;
        lastRunFilePath = info.source;
    end
    --输出函数信息到前台
    if logLevel == 0 and jumpFlag == false then
        local logTable = {"[lua hook] event:", tostring(event), " currentRunState:",tostring(currentRunState)," currentHookState:",tostring(currentHookState)," jumpFlag:", tostring(jumpFlag)};
        local logInfo = event;
        for k,v in pairs(info) do
            table.insert(logTable, tostring(k));
            table.insert(logTable, ":");
            table.insert(logTable, tostring(v));
            table.insert(logTable, " ");
        end
        local logString = table.concat(logTable);
        this.printToVSCode(logString);
    end

    --仅在line时做断点判断。进了断点之后不再进入本次STEP类型的判断，用Aflag做标记
    local isHit = false;
    if tostring(event) == "line" and jumpFlag == false then
        if currentRunState == runState.RUN or currentRunState == runState.STEPOVER or currentRunState == runState.STEPIN or currentRunState == runState.STEPOUT then
            --断点判断
            isHit = this.isHitBreakpoint(info) or hitBP;
            if isHit == true then
                this.printToVSCode(" + HitBreakpoint true");
                hitBP = false; --hitBP是断点硬性命中标记
                --计数器清0
                stepOverCounter = 0;
                stepOutCounter = 0;
                this.changeRunState(runState.HIT_BREAKPOINT);
                --发消息并等待
                this.SendMsgWithStack("stopOnBreakpoint");
            end
        end
    end

    if  isHit == true then
        return;
    end

    if currentRunState == runState.STEPOVER then
        -- line stepOverCounter!= 0 不作操作
        -- line stepOverCounter == 0 停止
        if event == "line" and stepOverCounter <= 0 and jumpFlag == false then
            stepOverCounter = 0;
            this.changeRunState(runState.STEPOVER_STOP)
            this.SendMsgWithStack("stopOnStep");
        elseif event == "return" or event == "tail return" then
            --5.1中是tail return
            if stepOverCounter ~= 0 then
                stepOverCounter = stepOverCounter - 1;
            end
        elseif event == "call" then
            stepOverCounter = stepOverCounter + 1;
        end
    elseif currentRunState == runState.STOP_ON_ENTRY then
        --在Lua入口点处直接停住
        if event == "line" and jumpFlag == false then
            --初始化内存分析的变量
            -- MemProfiler.getSystemVar();
            --这里要判断一下是Lua的入口点，否则停到
            this.SendMsgWithStack("stopOnEntry");
        end
    elseif currentRunState == runState.STEPIN then
        if event == "line" and jumpFlag == false then
            this.changeRunState(runState.STEPIN_STOP)
            this.SendMsgWithStack("stopOnStepIn");
        end
    elseif currentRunState == runState.STEPOUT then
        --line 不做操作
        --in 计数器+1
        --out 计数器-1
        if jumpFlag == false then
            if stepOutCounter <= -1 then
                stepOutCounter = 0;
                this.changeRunState(runState.STEPOUT_STOP)
                this.SendMsgWithStack("stopOnStepOut");
            end
        end

        if event == "return" or event == "tail return" then
            stepOutCounter = stepOutCounter - 1;
        elseif event == "call" then
            stepOutCounter = stepOutCounter + 1;
        end
    end

    --在RUN时检查并改变状态
    if hookLib == nil then
        if currentRunState == runState.RUN and jumpFlag == false and currentHookState ~= hookState.DISCONNECT_HOOK then
            local fileBP, G_BP =this.checkHasBreakpoint(lastRunFilePath);
            if fileBP == false then
                --文件无断点
                if G_BP == true then
                    this.changeHookState(hookState.MID_HOOK);
                else
                    this.changeHookState(hookState.LITE_HOOK);
                end
            else
                --文件有断点
                --判断函数内是否有断点
                local funHasBP = this.checkfuncHasBreakpoint(lastRunFunction.linedefined, lastRunFunction.lastlinedefined, lastRunFilePath);
                if  funHasBP then
                    --函数定义范围内
                    this.changeHookState(hookState.ALL_HOOK);
                else
                    this.changeHookState(hookState.MID_HOOK);
                end
            end

            --MID_HOOK状态下，return需要在下一次hook检查文件（return时，还是当前文件，检查文件时状态无法转换）
            if  (event == "return" or event == "tail return") and currentHookState == hookState.MID_HOOK then
                this.changeHookState(hookState.ALL_HOOK);
            end
        end
    end
end

-- 向Vscode发送标准通知消息，cmdStr是消息类型
-- @cmdStr  命令字
function this.SendMsgWithStack(cmdStr)
    local msgTab = this.getMsgTable(cmdStr);
    local userFuncLevel = 0;
    msgTab["stack"] , userFuncLevel= this.getStackTable();
    if userFuncLevel ~= 0 then
        lastRunFunction["func"] = debug.getinfo( (userFuncLevel - 1) , 'f').func;
    end
    this.sendMsg(msgTab);
    this.debugger_wait_msg();
end

-- hook状态改变
-- @s 目标状态
function this.changeHookState( s )
    if hookLib == nil and currentHookState == s then
        return;
    end

    this.printToConsole("change hook state :"..s)
    if s ~= hookState.DISCONNECT_HOOK then
        this.printToVSCode("change hook state : "..s)
    end

    currentHookState = s;
    if s == hookState.DISCONNECT_HOOK then
        --为了实现通用attach模式，require即开始hook，利用r作为时机发起连接
        if openAttachMode == true then
            if hookLib then hookLib.lua_set_hookstate(hookState.DISCONNECT_HOOK); else debug.sethook(this.debug_hook, "r", 1000000); end
        else
            if hookLib then hookLib.endHook(); else debug.sethook(); end
        end
    elseif s == hookState.LITE_HOOK then
        if hookLib then hookLib.lua_set_hookstate(hookState.LITE_HOOK); else debug.sethook(this.debug_hook, "r"); end
    elseif s == hookState.MID_HOOK then
        if hookLib then hookLib.lua_set_hookstate(hookState.MID_HOOK); else debug.sethook(this.debug_hook, "rc"); end
    elseif s == hookState.ALL_HOOK then
        if hookLib then hookLib.lua_set_hookstate(hookState.ALL_HOOK); else debug.sethook(this.debug_hook, "lrc");end
    end
    --coroutine
    if hookLib == nil then      
        this.changeCoroutineHookState();
    end
end

-- 运行状态机，状态变更
-- @s 目标状态
-- @isFromHooklib 1:从libc库中发来的状态改变 | 0:lua发来的状态改变
function this.changeRunState(s , isFromHooklib)
    local msgFrom;
    if isFromHooklib == 1 then
        msgFrom = "libc";
    else
        msgFrom = "lua";
    end

    --WAIT_CMD状态会等待接收消息，以下两个状态下不能发消息
    this.printToConsole("changeRunState :"..s.. " | from:"..msgFrom);
    if s ~= runState.DISCONNECT and s ~= runState.WAIT_CMD then
        this.printToVSCode("changeRunState :"..s.." | from:"..msgFrom);
    end

    if hookLib ~= nil and isFromHooklib ~= 1 then
        hookLib.lua_set_runstate(s);
    end
    currentRunState = s;
    --状态切换时，清除记录栈信息的状态
    currentCallStack = {};
    variableRefTab = {};
    variableRefIdx = 1;
end

-- 修改协程状态
-- @s hook标志位
function this.changeCoroutineHookState(s)
    s = s or currentHookState;
    this.printToConsole("change [Coroutine] HookState: "..tostring(s));
    for k ,co in pairs(coroutinePool) do
        if coroutine.status(co) == "dead" then
            table.remove(coroutinePool, k)
        else
            if s == hookState.DISCONNECT_HOOK then
                if openAttachMode == true then 
                    debug.sethook(co, this.debug_hook, "r", 1000000);
                else 
                    debug.sethook(co, this.debug_hook, ""); 
                end
            elseif s == hookState.LITE_HOOK then debug.sethook(co , this.debug_hook, "r");
            elseif s == hookState.MID_HOOK then debug.sethook(co , this.debug_hook, "rc"); 
            elseif s == hookState.ALL_HOOK then debug.sethook(co , this.debug_hook, "lrc");
            end
        end
    end
end
-------------------------变量处理相关-----------------------------

--清空REPL的evn环境
function this.clearEnv()
    if this.getTableMemberNum(env) > 0 then
        --清空env table
        env = setmetatable({}, getmetatable(env));
    end
end

--返回REPL的evn环境
function this.showEnv()
    return env;
end

-- 用户观察table的查找函数。用tableVarName作为key去查逐层级查找realVar是否匹配
-- @tableVarName 是用户观察的变量名，已经按层级被解析成table。比如用户输出a.b.c，tableVarName是 a = { b = { c } }
-- @realVar 是待查询 table
-- @return  返回查到的table。没查到返回nil
function this.findTableVar( tableVarName,  realVar)
    if type(tableVarName) ~= "table" or type(realVar) ~= "table" then
        return nil;
    end

    local layer = 2;
    local curVar = realVar;
    local jumpOutFlag = false;
    repeat
        if tableVarName[layer] ~= nil then
            --这里优先展示数字key，比如a{"1" = "aa", [1] = "bb"} 会展示[1]的值
            local tmpCurVar = nil;
            xpcall(function() tmpCurVar = curVar[tonumber(tableVarName[layer])]; end , function() tmpCurVar = nil end );
            if tmpCurVar == nil then
                xpcall(function() curVar = curVar[tostring(tableVarName[layer])]; end , function() curVar = nil end );
            else
                curVar = tmpCurVar;
            end
            layer = layer + 1;
            if curVar == nil then
                return nil;
            end
        else
            --找到
            jumpOutFlag = true;
        end
    until(jumpOutFlag == true)
    return curVar;
end

-- 根据传入信息生成返回的变量信息
-- @variableName    变量名
-- @variableIns        变量实例
-- @return              包含变量信息的格式化table
function this.createWatchedVariableInfo(variableName, variableIns)
    local var = {};
    var.name = variableName;
    var.type = tostring(type(variableIns));
    xpcall(function() var.value = tostring(variableIns) end , function() var.value = tostring(type(variableIns)) .. " [value can't trans to string]" end );
    var.variablesReference = "0";  --这个地方必须用“0”， 以免variableRefTab[0]出错

    if var.type == "table" or var.type == "function" then
        var.variablesReference = variableRefIdx;
        variableRefTab[variableRefIdx] = variableIns;
        variableRefIdx = variableRefIdx + 1;
        if var.type == "table" then
            local memberNum = this.getTableMemberNum(variableIns);
            var.value = memberNum .." Members ".. var.value;
        end
    end
    return var;
end
)[========]";