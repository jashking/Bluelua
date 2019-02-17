R"[========](
-- 按照local -> upvalue -> _G 顺序查找观察变量
-- @varName 用户输入的变量名
-- @stackId 调用栈层级(仅在固定栈层查找)
-- @isFormatVariable    是否把变量格式化为VSCode接收的形式
-- @return 查到返回信息，查不到返回nil
function this.getWatchedVariable( varName , stackId , isFormatVariable )
    this.printToConsole("getWatchedVariable | varName:" .. varName .. " stackId:".. stackId .." isFormatVariable:" .. tostring(isFormatVariable) )
    if tostring(varName) == nil or tostring(varName) == "" then
        return nil;
    end
    --orgname 记录原名字. 用来处理a.b.c的形式
    local orgname = varName;
    --支持a.b.c形式。切割varName
    local tableVarName = {};
    if varName:match('%.') then
        tableVarName = this.stringSplit(varName , '%.');
        if type(tableVarName) ~= "table" or #tableVarName < 1 then
            return nil;
        end
        varName = tableVarName[1];
    end
    --用来返回，带有查到变量的table
    local varTab = {};
    local ly = this.getSpecificFunctionStackLevel(currentCallStack[stackId - 1].func) + 1;

    local layerVarTab = this.getVariable(ly, isFormatVariable);
    local upTable = this.getUpValueVariable(currentCallStack[stackId - 1 ].func, isFormatVariable);
    local travelTab = {};
    table.insert(travelTab, layerVarTab);
    table.insert(travelTab, upTable);
    for _, layerVarTab in ipairs(travelTab) do
        for i,realVar in ipairs(layerVarTab) do
            if realVar.name == varName then
                if #tableVarName > 0 and type(realVar) == "table" then
                    --处理a.b.c的table类型
                    local findRes = this.findTableVar(tableVarName,  variableRefTab[realVar.variablesReference]);
                    if findRes ~= nil then
                        --命中
                        if isFormatVariable then
                            local var = this.createWatchedVariableInfo( orgname , findRes );
                            table.insert(varTab, var);
                            return varTab;
                        else
                            return findRes.value;
                        end
                    end
                else
                    --命中
                    if isFormatVariable then
                        table.insert(varTab, realVar);
                        return varTab;
                    else
                        return realVar.value;
                    end
                end
            end
        end
    end

    --在全局变量_G中查找
    if _G[varName] ~= nil then
        --命中
        if #tableVarName > 0 and type(_G[varName]) == "table" then
            local findRes = this.findTableVar(tableVarName,  _G[varName]);
            if findRes ~= nil then
                if isFormatVariable then
                    local var = this.createWatchedVariableInfo( orgname , findRes );
                    table.insert(varTab, var);
                    return varTab;
                else
                    return findRes;
                end
            end
        else
            if isFormatVariable then
                local var = this.createWatchedVariableInfo( varName , _G[varName] );
                table.insert(varTab, var);
                return varTab;
            else
                return _G[varName];
            end
        end
    end
    this.printToConsole("getWatchedVariable not find variable");
    return nil;
end

-- 查询引用变量
-- @refStr 变量记录id(variableRefTab索引)
-- @return 格式化的变量信息table
function this.getVariableRef( refStr )
    local varRef = tonumber(refStr);
    local varTab = {};

    if tostring(type(variableRefTab[varRef])) == "table" then
        for n,v in pairs(variableRefTab[varRef]) do
            local var = {};
            var.name = tostring(n);
            var.type = tostring(type(v));
            xpcall(function() var.value = tostring(v) end , function() var.value = tostring(type(v)) .. " [value can't trans to string]" end );
            var.variablesReference = "0";
            if var.type == "table" or var.type == "function" then
                var.variablesReference = variableRefIdx;
                variableRefTab[variableRefIdx] = v;
                variableRefIdx = variableRefIdx + 1;
                if var.type == "table" then
                    local memberNum = this.getTableMemberNum(v);
                    var.value = memberNum .." Members ".. var.value
                end
            end
            table.insert(varTab, var);
        end
        --获取一下mtTable
        local mtTab = getmetatable(variableRefTab[varRef]);
        if mtTab ~= nil and type(mtTab) == "table" then
            local var = {};
            var.name = "_Metatable_";
            var.type = tostring(type(mtTab));
            xpcall(function() var.value = "元表 "..tostring(mtTab); end , function() var.value = "元表 [value can't trans to string]" end );
            var.variablesReference = variableRefIdx;
            variableRefTab[variableRefIdx] = mtTab;
            variableRefIdx = variableRefIdx + 1;
            table.insert(varTab, var);
        end
    elseif tostring(type(variableRefTab[varRef])) == "function" then
        --取upvalue
        varTab = this.getUpValueVariable(variableRefTab[varRef], true);
    elseif tostring(type(variableRefTab[varRef])) == "userdata" then
        --取mt table
        local udMtTable = getmetatable(variableRefTab[varRef]);
        if udMtTable ~= nil and type(udMtTable) == "table" then
            local var = {};
            var.name = "_Metatable_";
            var.type = tostring(type(udMtTable));
            xpcall(function() var.value = "元表 "..tostring(udMtTable); end , function() var.value = "元表 [value can't trans to string]" end );
            var.variablesReference = variableRefIdx;
            variableRefTab[variableRefIdx] = udMtTable;
            variableRefIdx = variableRefIdx + 1;
            table.insert(varTab, var);
        end
    end
    return varTab;
end

-- 获取全局变量。方法和内存管理中获取全局变量的方法一样
-- @return 格式化的信息, 若未找到返回空table
function this.getGlobalVariable( ... )
    --成本比较高，这里只能遍历_G中的所有变量，并去除系统变量，再返回给客户端
    local varTab = {};
    for k,v in pairs(_G) do
        local var = {};
        var.name = tostring(k);
        var.type = tostring(type(v));
        xpcall(function() var.value = tostring(v) end , function() var.value =  tostring(type(v)) .." [value can't trans to string]" end );
        var.variablesReference = "0";
        if var.type == "table" or var.type == "function" then
            var.variablesReference = variableRefIdx;
            variableRefTab[variableRefIdx] = v;
            variableRefIdx = variableRefIdx + 1;
            if var.type == "table" then
                local memberNum = this.getTableMemberNum(v);
                var.value = memberNum .." Members ".. var.value
            end

        end
        table.insert(varTab, var);
    end
    return varTab;
end

-- 获取upValues
-- @isFormatVariable  true返回[值]  true返回[格式化的数据]
function this.getUpValueVariable( checkFunc , isFormatVariable)
    local isGetValue = true;
    if isFormatVariable == true then
        isGetValue = false;
    end

    --通过Debug获取当前函数的Func
    checkFunc = checkFunc or lastRunFunction.func;

    local varTab = {};
    if checkFunc == nil then
        return varTab;
    end
    local i = 1
    repeat
        local n, v = debug.getupvalue(checkFunc, i)
        if n then

        local var = {};
        var.name = n;
        var.type = tostring(type(v));
        var.variablesReference = "0";

        if isGetValue == false then
            xpcall(function() var.value = tostring(v) end , function() var.value = tostring(type(v)) .. " [value can't trans to string]" end );
            if var.type == "table" or var.type == "function" then
                var.variablesReference = variableRefIdx;
                variableRefTab[variableRefIdx] = v;
                variableRefIdx = variableRefIdx + 1;
                if var.type == "table" then
                    local memberNum = this.getTableMemberNum(v);
                    var.value = memberNum .." Members ".. var.value
                end
            end
        else
            var.value = v;
        end

        table.insert(varTab, var);
        i = i + 1
        end
    until not n
    return varTab;
end

-- 获取局部变量 checkLayer是要查询的层级，如果不设置则查询当前层级
-- @isFormatVariable 是否取值，true:取值的tostring
function this.getVariable( checkLayer, isFormatVariable )
    local isGetValue = true;
    if isFormatVariable == true then
        isGetValue = false;
    end

    --local ly = checkLayer or this.getCurrentFunctionStackLevel();
    local ly = checkLayer or this.getSpecificFunctionStackLevel(lastRunFunction.func);
    -- print(debug.traceback("1"));
    -- print("getVariable ly"..ly);
    if ly == 0 then
        this.printToVSCode("[error]获取层次失败！", 2);
        return;
    end
    local varTab = {};
    local stacklayer = ly;
    --print("stacklayer"..stacklayer);
    local k = 1;
    repeat
        local n, v = debug.getlocal(stacklayer, k)
        if n == nil then
            break;
        end
        --print(n, v);
        --(*temporary)是系统变量，过滤掉
        if "(*temporary)" ~= tostring(n) then
            local var = {};
            var.name = n;
            var.type = tostring(type(v));
            var.variablesReference = "0";

            if isGetValue == false then
                xpcall(function() var.value = tostring(v) end , function() var.value = tostring(type(v)) .. " [value can't trans to string]" end );
                if var.type == "table" or var.type == "function" then
                    var.variablesReference = variableRefIdx;
                    variableRefTab[variableRefIdx] = v;
                    variableRefIdx = variableRefIdx + 1;
                    if var.type == "table" then
                        local memberNum = this.getTableMemberNum(v);
                        var.value = memberNum .." Members ".. var.value
                    end
                end
            else
                var.value = v;
            end
            table.insert(varTab, var);
        end
        k = k + 1
    until n == nil
    return varTab;
end

-- 执行表达式
function this.processExp(msgTable)
    local retString;
    if msgTable ~= nil then
        local expression = this.trim(tostring(msgTable.Expression));
        local isCmd = false;
        if isCmd == false then
            --如果输入的是一个变量，加上return
            if expression:find("p ", 1, true) == 1 then
                expression = expression:sub(3);
                expression = "return " .. expression;
            end
            if expression:match(" ") == nil then
                expression = "return " .. expression;
            end

            local f = debugger_loadString(expression);
            --判断结果，如果表达式错误会返回nil
            if type(f) == "function" then
                if _VERSION == "Lua 5.1" then
                    setfenv(f , env);
                else
                    debug.setupvalue(f, 1, env);
                end
                --表达式要有错误处理
                xpcall(function() retString = f() end , function() retString = "输入错误指令。\n + 请检查指令是否正确\n + 指令仅能在[暂停在断点时]输入, 请不要在程序持续运行时输入" end)
            else
                retString = "指令执行错误。\n + 请检查指令是否正确\n + 如果希望观察[变量的值]或[表达式的返回结果]，请在表达式前前面加\"p \"\n + 如果仅希望执行一段lua代码，直接输入即可"
            end
        end
    end

    local var = {};
    var.name = "Exp";
    var.type = tostring(type(retString));
    xpcall(function() var.value = tostring(retString) end , function(e) var.value = tostring(type(retString))  .. " [value can't trans to string] ".. e end );
    var.variablesReference = "0";
    if var.type == "table" or var.type == "function" then
        variableRefTab[variableRefIdx] = retString;
        var.variablesReference = variableRefIdx;
        variableRefIdx = variableRefIdx + 1;
        if var.type == "table" then
            local memberNum = this.getTableMemberNum(retString);
            var.value = memberNum .." Members ".. var.value;
        end
    end
    --string执行完毕后清空evn环境
    this.clearEnv();
    local retTab = {}
    table.insert(retTab ,var);
    return retTab;
end

--执行变量观察表达式
function this.processWatchedExp(msgTable)
    local retString;
    local expression = "return ".. tostring(msgTable.varName)
    this.printToConsole("processWatchedExp | expression: " .. expression);
    local f = debugger_loadString(expression);
    --判断结果，如果表达式错误会返回nil
    if type(f) == "function" then
        --表达式正确
        if _VERSION == "Lua 5.1" then
            setfenv(f , env);
        else
            debug.setupvalue(f, 1, env);
        end
        xpcall(function() retString = f() end , function() retString = "输入了错误的变量信息" end)
    else
        retString = "未能找到变量的值"
    end

    local var = {};
    var.name = msgTable.varName;
    var.type = tostring(type(retString));
    xpcall(function() var.value = tostring(retString) end , function() var.value = tostring(type(retString)) .. " [value can't trans to string]" end );
    var.variablesReference = "0";

    if var.type == "table" or var.type == "function" then
        variableRefTab[variableRefIdx] = retString;
        var.variablesReference = variableRefIdx;
        variableRefIdx = variableRefIdx + 1;
        if var.type == "table" then
            local memberNum = this.getTableMemberNum(retString);
            var.value = memberNum .." Members ".. var.value;
        end
    end

    local retTab = {}
    table.insert(retTab ,var);
    return retTab;
end

return this;

)[========]";