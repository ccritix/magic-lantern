--[[---------------------------------------------------------------------------
Object-Oriented Programming Helpers
This module is a lua script (class.lua), you need to explicitly load it
with require('class')
@module class
]]

--make strict.lua happy
object = nil

--[[---------------------------------------------------------------------------
Creates a new OOP-style class, optionally inheriting from an existing class
@tparam[opt] table base_class the base class to inherit from
@treturn table the new class definition
@function class
@usage
foo = class()
function foo:print()
    print("Foo Print")
end
bar = class(foo)
function bar:__init(field)
    self.field = field
end
function bar:print()
    self:base().print(self)
    print(field)
end
local i = foo()
local j = bar("BAR!")
i:print()
j:print()
print(j:is(bar))
print(i:is(bar))
]]
function class(base_class)
    base_class = base_class or object
    local t = {}
    local mt = {}
    mt.__index = t
    local constructor = function(f,...)
        local i = {}
        setmetatable(i,mt)
        i:init(...)
        return i
    end
    
    --[[---------------------------------------------------------------------------
    @type object
    ]]
    
    --[[---------------------------------------------------------------------------
    Returns this instance's class definition table
    @treturn table
    @function class
    ]]
    function t:class()
        return t
    end
    
    --[[---------------------------------------------------------------------------
    Returns this instance's base class definition table
    @treturn table
    @function base
    ]]
    function t:base()
        return base_class
    end
    
    --[[---------------------------------------------------------------------------
    Returns whether or not this object is an instance of candidate_class
    @tparam table candidate_class
    @treturn boolean
    @function is
    ]]
    function t:is(candidate_class)
        local curr = t
        while curr ~= nil do
            if curr == candidate_class then return true
            else curr = curr.base() end
        end
        return false
    end
    if base_class then
        setmetatable(t, { __index = base_class, __call = constructor })
    else
        setmetatable(t, { __call = constructor })
    end
    return t
end

-- create a lowest level base class for all other types to based on
object = class()

--[[---------------------------------------------------------------------------
The default object constructor simply takes a table as an argument and copies
that table's fields to the new object. To override this behavior, define an
init function in your class. This is a private function that's not meant to 
be invoked directly, rather it is called by the actual constructor when a new 
instance is created
@tparam table p
@function init
]]
function object:init(p)
    if type(p) == "table" then
        for k,v in pairs(p) do
            self[k] = v
        end
    end
end