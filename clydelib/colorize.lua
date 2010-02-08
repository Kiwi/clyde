local pairs = pairs
local tostring = tostring
local setmetatable = setmetatable

module(..., package.seeall)

local colormt = {}

function colormt:__tostring()
    return self.value
end

function colormt:__concat(other)
    return tostring(self) .. tostring(other)
end

function colormt:__call(s)
    return self .. s .. _M.reset
end

colormt.__metatable = {}

local function makecolor(value)
    return setmetatable({ value = value }, colormt)
end

local colors = {
    ['reset'] =     '\27[0m',
    ['clear'] =     '\27[0m',
    ['bright'] =    '\27[1m',
    ['dim'] =       '\27[2m',
    ['under'] =     '\27[4m',
    ['blink'] =     '\27[5m',
    ['reverse'] =   '\27[7m',
    ['hidden'] =    '\27[8m',

    ['bla'] =       '\27[30m',
    ['blab'] =      '\27[30;1m',
    ['red'] =       '\27[31m',
    ['redb'] =      '\27[31;1m',
    ['gre'] =       '\27[32m',
    ['greb'] =      '\27[32;1m',
    ['yel'] =       '\27[33m',
    ['yelb'] =      '\27[33;1m',
    ['blu'] =       '\27[34m',
    ['blub'] =      '\27[34;1m',
    ['mag'] =       '\27[35m',
    ['magb'] =      '\27[35;1m',
    ['cya'] =       '\27[36m',
    ['cyab'] =      '\27[36;1m',
    ['whi'] =       '\27[37m',
    ['whib'] =      '\27[37;1m',

    ['onbla'] =     '\27[40m',
    ['onblab'] =    '\27[40;1m',
    ['onred'] =     '\27[41m',
    ['onredb'] =    '\27[41;1m',
    ['ongre'] =     '\27[42m',
    ['ongreb'] =    '\27[42;1m',
    ['onyel'] =     '\27[43m',
    ['onyelb'] =    '\27[43;1m',
    ['onblu'] =     '\27[44m',
    ['onblub'] =    '\27[44;1m',
    ['onmag'] =     '\27[45m',
    ['onmagb'] =    '\27[45;1m',
    ['oncya'] =     '\27[46m',
    ['oncyab'] =    '\27[46;1m',
    ['onwhi'] =     '\27[47m',
    ['onwhib'] =    '\27[47;1m',

}

function enable()
    for k, v in pairs(colors) do
        _M[k] = makecolor(v)
    end
end

function disable()
    for k, v in pairs(colors) do
        _M[k] = makecolor("")
    end
end

enable()
