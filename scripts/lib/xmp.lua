--[[---------------------------------------------------------------------------
Helper for writing supplemental xmp metadata

This module is a lua script (@{xmp.lua}), you need to explicitly load it
with `require('xmp')`

@module xmp
]]

GUISTATE_QR = 3

xmp = {}
xmp.properties = {}

xmp.template = [[
<x:xmpmeta xmlns:x="adobe:ns:meta/" x:xmptk="Adobe XMP Core 5.6-c011 79.156380, 2014/05/21-23:38:37        ">
 <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
  <rdf:Description rdf:about=""
    xmlns:exif="http://ns.adobe.com/exif/1.0/"
    xmlns:aux="http://ns.adobe.com/exif/1.0/aux/"
    exif:ExifVersion="0230"
    %s>
  </rdf:Description>
 </rdf:RDF>
</x:xmpmeta>
]]

xmp.property_format = [[%s="%s"
    ]]

-- predefined xmp properties
xmp.lens_name = { name = "aux:Lens" }
xmp.focal_length = { name = "exif:FocalLength", format = "%d/1"}
xmp.aperture = { name = "exif:FNumber", format = "%d/10" }

function xmp.get_sidecar_filename()
    return string.format("DCIM/%03dCANON/%s%04d.XMP", dryos.shooting_card.folder_number, dryos.prefix, dryos.shooting_card.file_number)
end

--[[---------------------------------------------------------------------------
Starts listening for pictures being taken to write xmp metadata.
@function start
]]
function xmp:start()
    local this = self
    function property.GUI_STATE:handler(value)
        if value == GUISTATE_QR then
            this:write()
        end
    end
end

--[[---------------------------------------------------------------------------
Stops listening for pictures being taken.
@function start
]]
function xmp:stop()
    property.GUI_STATE.handler = nil
end

--[[---------------------------------------------------------------------------
Adds a new metadata property to record in the xmp file
@param property the property to add, should be a table with a name and optionally a string format
@param value the value of the property or a function that returns the value
@function add_property
]]
function xmp:add_property(property, value)
    if property.format ~= nil then
        if type(value) == "function" then
            self.properties[property.name] = function() 
                return string.format(property.format, value())
            end
        else
            self.properties[property.name] = string.format(property.format, value)
        end
    else
        self.properties[property.name] = value
    end
end

--[[---------------------------------------------------------------------------
Writes the xmp metdata to file
@param filename optional - the filename to write to, leave blank to use the sidecar filename for the most recently taken photo
@function write
]]
function xmp:write(filename)
    local f = io.open(filename or self.get_sidecar_filename(), "w+")
    local str = ""
    for k,v in pairs(self.properties) do
        if type(v) == "function" then
            str = str..string.format(self.property_format, k, v())
        else
            str = str..string.format(self.property_format, k, v)
        end
    end
    f:write(string.format(self.template, str))
    f:close()
end