-- automatically generated by the FlatBuffers compiler, do not modify

-- namespace: NamespaceB

local flatbuffers = require('flatbuffers')

local TableInNestedNS = {} -- the module
local TableInNestedNS_mt = {} -- the class metatable

function TableInNestedNS.New()
    local o = {}
    setmetatable(o, {__index = TableInNestedNS_mt})
    return o
end
function TableInNestedNS.GetRootAsTableInNestedNS(buf, offset)
    local n = flatbuffers.N.UOffsetT:Unpack(buf, offset)
    local o = TableInNestedNS.New()
    o:Init(buf, n + offset)
    return o
end
function TableInNestedNS_mt:Init(buf, pos)
    self.view = flatbuffers.view.New(buf, pos)
end
function TableInNestedNS_mt:Foo()
    local o = self.view:Offset(4)
    if o ~= 0 then
        return self.view:Get(flatbuffers.N.Int32, o + self.view.pos)
    end
    return 0
end
function TableInNestedNS.Start(builder) builder:StartObject(1) end
function TableInNestedNS.AddFoo(builder, foo) builder:PrependInt32Slot(0, foo, 0) end
function TableInNestedNS.End(builder) return builder:EndObject() end

return TableInNestedNS -- return the module