R"[========](
-----------------------------------------------------------------------------
-- 提升兼容性方法
-----------------------------------------------------------------------------
--生成平台无关的路径。
--return:nil(error)/path
function this.genUnifiedPath(path)
    if path == "" or path == nil then
        return nil;
    end
    --大小写不敏感时，路径全部转为小写
    if pathCaseSensitivity == 0 then
        path = string.lower(path);
    end
    --统一路径全部替换成/
    path = string.gsub(path, [[\]], "/");
    --处理 /../   /./
    local pathTab = this.stringSplit(path, '/');
    local newPathTab = {};
    for k, v in ipairs(pathTab) do
        if v == '.'  then   
            --continue
        elseif v == ".." and #newPathTab >= 1 and newPathTab[#newPathTab]:sub(2,2) ~= ':' then
            --newPathTab有元素，最后一项不是X:
            table.remove(newPathTab);
        else
            table.insert(newPathTab, v);
        end
    end
    --重新拼合后如果是mac路径第一位是/
    local newpath = table.concat(newPathTab, '/');
    if path:sub(1,1) == '/' then 
        newpath = '/'.. newpath;
    end
    return newpath;
end

function this.getPathFromCache(source)
    return  pathFormatCache[source];
end

function this.setPathToCache(source, dest)
    pathFormatCache[source] = dest;
end
-----------------------------------------------------------------------------
-- 内存相关
-----------------------------------------------------------------------------
function this.sendLuaMemory()
    local luaMem = collectgarbage("count");
    local sendTab = {};
    sendTab["callbackId"] = "0";
    sendTab["cmd"] = "refreshLuaMemory";
    sendTab["info"] = {};
    sendTab["info"]["memInfo"] = tostring(luaMem);
    this.sendMsg(sendTab);
end

-----------------------------------------------------------------------------
-- 网络相关方法
-----------------------------------------------------------------------------
--刷新socket
function this.reGetSock()
    if sock~= nil then
        pcall(function() sock:close() end);
    end
    sock = lua_extension and lua_extension.luasocket and lua_extension.luasocket().tcp();
    if sock == nil then
       if pcall(function() sock =  require("socket.core").tcp(); end) then
            this.printToConsole("reGetSock success");
            sock:settimeout(0.05);
       else
            this.printToConsole("reGetSock fail");
       end
    end
end

-- 定时(以函数return为时机) 进行attach连接
function this.reConnect()
    if currentHookState == hookState.DISCONNECT_HOOK then
        if sock == nil then
            this.printToConsole("[debugger error] have no find luascoket!", 2);
            return 1;
        end

        if os.time() - stopConnectTime < attachInterval then
            this.printToConsole("Reconnect time less than 1s");
            this.printToConsole("os.time:".. os.time() .. " | stopConnectTime:" ..stopConnectTime);
            return 1; 
        end

        local sockSuccess = sock and sock:connect(connectHost, connectPort);
        if sockSuccess ~= nil then
            this.connectSuccess();
        else
            this.reGetSock();
            stopConnectTime = os.time();
        end
        return 1;
    end
    return 0;
end

-- 向adapter发消息
-- @sendTab 消息体table
function this.sendMsg( sendTab )
    local sendStr = json.encode(sendTab);
    if currentRunState == runState.DISCONNECT then
        this.printToConsole("[debugger error] disconnect but want sendMsg:" .. sendStr, 2);
        this.disconnect();
        return;
    end

    local succ,err = sock:send(sendStr..TCPSplitChar.."\n");
    if succ == nil then
        if err == "closed" then
            this.disconnect();
        end
    end
end

-- 处理 收到的消息
-- @dataStr 接收的消息json
function this.dataProcess( dataStr )
    this.printToVSCode("debugger get:"..dataStr);
    local dataTable = json.decode(dataStr);
    if dataTable == nil then
        this.printToVSCode("[error] Json is error", 2);
        return;
    end

    if dataTable.callbackId ~= "0" then
        this.setCallbackId(dataTable.callbackId);
    end

    if dataTable.cmd == "continue" then
        this.changeRunState(runState.RUN);
        local msgTab = this.getMsgTable("continue", this.getCallbackId());
        this.sendMsg(msgTab);

    elseif dataTable.cmd == "stopOnStep" then
        this.changeRunState(runState.STEPOVER);
        local msgTab = this.getMsgTable("stopOnStep", this.getCallbackId());
        this.sendMsg(msgTab);

    elseif dataTable.cmd == "stopOnStepIn" then
        this.changeRunState(runState.STEPIN);
        local msgTab = this.getMsgTable("stopOnStepIn", this.getCallbackId());
        this.sendMsg(msgTab);

    elseif dataTable.cmd == "stopOnStepOut" then
        this.changeRunState(runState.STEPOUT);
        local msgTab = this.getMsgTable("stopOnStepOut", this.getCallbackId());
        this.sendMsg(msgTab);

    elseif dataTable.cmd == "setBreakPoint" then
        this.printToVSCode("dataTable.cmd == setBreakPoint");
        local bkPath = dataTable.info.path;
        bkPath = this.genUnifiedPath(bkPath);
        this.printToVSCode("setBreakPoint path:"..tostring(bkPath));
        breaks[bkPath] = dataTable.info.bks;
        --save
        for k, v in pairs(breaks) do
            if next(v) == nil then
                breaks[k] = nil;
            end
        end
        --sync breaks to c
        if hookLib ~= nil then
            hookLib.sync_breakpoints();
        end

        if currentRunState ~= runState.WAIT_CMD then
            if hookLib == nil then
                local fileBP, G_BP =this.checkHasBreakpoint(lastRunFilePath);
                if fileBP == false then
                    if G_BP == true then
                        this.changeHookState(hookState.MID_HOOK);
                    else
                        this.changeHookState(hookState.LITE_HOOK);
                    end
                else
                    this.changeHookState(hookState.ALL_HOOK);
                end
            end
        else
            local msgTab = this.getMsgTable("setBreakPoint", this.getCallbackId());
            this.sendMsg(msgTab);
            return;
        end
        --其他时机收到breaks消息
        local msgTab = this.getMsgTable("setBreakPoint", this.getCallbackId());
        this.sendMsg(msgTab);
        this.debugger_wait_msg();

    elseif dataTable.cmd == "getVariable" then
        --仅在停止时处理消息，其他时刻收到此消息，丢弃
        if currentRunState == runState.STOP_ON_ENTRY or
        currentRunState == runState.HIT_BREAKPOINT or
        currentRunState == runState.STEPOVER_STOP or
        currentRunState == runState.STEPIN_STOP or
        currentRunState == runState.STEPOUT_STOP then
            --发送变量给游戏，并保持之前的状态,等待再次接收数据
            --dataTable.info.varRef  10000~20000局部变量
            --                       20000~30000全局变量
            --                       30000~     upvalue
            -- 1000~2000局部变量的查询，2000~3000全局，3000~4000upvalue
            local msgTab = this.getMsgTable("getVariable", this.getCallbackId());
            local varRefNum = tonumber(dataTable.info.varRef);
            if varRefNum < 10000 then
                --查询变量
                local varTable = this.getVariableRef(dataTable.info.varRef, true);
                msgTab.info = varTable;
            elseif varRefNum >= 10000 and varRefNum < 20000 then
                --局部变量
                if dataTable.info.stackId ~= nil and tonumber(dataTable.info.stackId) > 1 then
                    this.curStackId = tonumber(dataTable.info.stackId);
                    -- local stackId = this.getCurrentFunctionStackLevel() + this.curStackId - 1; --去除偏移量
                    local stackId = this.getSpecificFunctionStackLevel(currentCallStack[this.curStackId - 1].func) + 1; --去除偏移量
                    local varTable = this.getVariable(stackId, true);
                    msgTab.info = varTable;
                end

            elseif varRefNum >= 20000 and varRefNum < 30000 then
                --全局变量
                local varTable = this.getGlobalVariable();
                msgTab.info = varTable;
            elseif varRefNum >= 30000 then
                --upValue
                if dataTable.info.stackId ~= nil and tonumber(dataTable.info.stackId) > 1 then
                    local stackId = tonumber(dataTable.info.stackId);
                    local varTable = this.getUpValueVariable(currentCallStack[stackId - 1 ].func, true);
                    msgTab.info = varTable;
                end
            end
            this.sendMsg(msgTab);
            this.debugger_wait_msg();
        end
    elseif dataTable.cmd == "initSuccess" then
        --初始化会传过来一些变量，这里记录这些变量
        luaFileExtension = dataTable.info.luaFileExtension
        local TempFilePath = dataTable.info.TempFilePath;
        if TempFilePath:sub(-1, -1) == [[\]] or TempFilePath:sub(-1, -1) == [[/]] then
            TempFilePath = TempFilePath:sub(1, -2);
        end

        TempFilePath_luaString = TempFilePath;

        cwd = this.genUnifiedPath(dataTable.info.cwd);
        logLevel = tonumber(dataTable.info.logLevel) or 1;

        OSType = dataTable.info.OSType;
        clibPath = dataTable.info.clibPath;

        if  tostring(dataTable.info.pathCaseSensitivity) == "false" then
            pathCaseSensitivity =  0;
        else
            pathCaseSensitivity =  1;
        end

        --查找c++的hook库是否存在
        if tostring(dataTable.info.useHighSpeedModule) == "true" then
            local clibExt, platform;
            if OSType == "Darwin" then clibExt = "/?.so;"; platform = "mac";
            else clibExt = "/?.dll;"; platform = "win";   end

            local lua_ver;
            if _VERSION == "Lua 5.1" then 
                lua_ver = "501";
            else
                lua_ver = "503";
            end

            local x86Path = clibPath.. platform .."/x86/".. lua_ver .. clibExt;
            local x64Path = clibPath.. platform .."/x86_64/".. lua_ver .. clibExt;

            if luapanda_chook ~= nil then
                hookLib = luapanda_chook;
                HOOK_LEVEL = 3;
            else
                if this.tryRequireClib("libpdebug", x64Path) or this.tryRequireClib("libpdebug", x86Path) then
                    HOOK_LEVEL = 3;
                end
            end
        end
        --设置是否stopOnEntry
        local msgTab = this.getMsgTable("initSuccess", this.getCallbackId());
        --回传是否使用了lib，是否有loadstring函数
        local isUseHookLib = 0;
        if hookLib ~= nil then
            isUseHookLib = 1;
            --同步数据给c hook
            hookLib.sync_config(logLevel, pathCaseSensitivity);
            hookLib.sync_tempfile_path(TempFilePath_luaString)
            hookLib.sync_cwd(cwd);
            hookLib.sync_file_ext(luaFileExtension);
        end
        --detect LoadString
        isUseLoadstring = 0;
        if debugger_loadString ~= nil and type(debugger_loadString) == "function" then
            if(pcall(debugger_loadString("return 0"))) then
                isUseLoadstring = 1;
            end
        end
        local tab = { debuggerVer = tostring(debuggerVer) , UseHookLib = tostring(isUseHookLib) , UseLoadstring = tostring(isUseLoadstring) };
        msgTab.info  = tab;
        this.sendMsg(msgTab);
        --上面getBK中会判断当前状态是否WAIT_CMD, 所以最后再切换状态。
        if dataTable.info.stopOnEntry == "true" then
            this.changeRunState(runState.STOP_ON_ENTRY);   --停止在STOP_ON_ENTRY再接收breaks消息
        else
            this.debugger_wait_msg(1);  --等待1s bk消息 如果收到或超时(没有断点)就开始运行
            this.changeRunState(runState.RUN);
        end

    elseif dataTable.cmd == "getWatchedVariable" then
        local msgTab = this.getMsgTable("getWatchedVariable", this.getCallbackId());
        local stackId = tonumber(dataTable.info.stackId);
        --loadstring系统函数, watch插件加载
        if isUseLoadstring == 1 then
            --使用loadstring
            this.curStackId = stackId;
            local retValue = this.processWatchedExp(dataTable.info);
            msgTab.info = retValue
            this.sendMsg(msgTab);
            this.debugger_wait_msg();
            return;
        else
            --旧的查找方式
            local wv =  this.getWatchedVariable(dataTable.info.varName, stackId, true);
            if wv ~= nil then
                msgTab.info = wv;
            end
            this.sendMsg(msgTab);
            this.debugger_wait_msg();
        end
    elseif dataTable.cmd == "stopRun" then
        --停止hook，已不在处理任何断点信息，也就不会产生日志等。发送消息后等待前端主动断开连接
        local msgTab = this.getMsgTable("stopRun", this.getCallbackId());
        this.sendMsg(msgTab);
        this.disconnect();
    elseif "LuaGarbageCollect" == dataTable.cmd then
        this.printToVSCode("collect garbage!");
        collectgarbage("collect");
        --回收后刷一下内存
        this.sendLuaMemory();
        this.debugger_wait_msg();
    elseif "runREPLExpression" == dataTable.cmd then
        this.curStackId = tonumber(dataTable.info.stackId);
        local retValue = this.processExp(dataTable.info);
        local msgTab = this.getMsgTable("runREPLExpression", this.getCallbackId());
        msgTab.info = retValue
        this.sendMsg(msgTab);
        this.debugger_wait_msg();
    else

    end
end
)[========]";