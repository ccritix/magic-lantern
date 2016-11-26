--[[---------------------------------------------------------------------------
Functions for writing supplemental xmp metadata
]]

GUISTATE_QR = 3

xmp = {}
xmp.__index = xmp

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

xmp.property_format = [[
    %s="%s"
]]

xmp.lens_name = { name = "aux:Lens" }
xmp.focal_length = { name = "exif:FocalLength", format = "%d/1"}
xmp.aperture = { name = "exif:FNumber", format = "%d/10" }

function xmp.get_sidecar_filename()
    return string.format("DCIM/%03dCANON/%s%04d.XMP", dryos.shooting_card.folder_number, dryos.prefix, dryos.shooting_card.file_number)
end

function xmp.create()
    local x = {}
    setmetatable(x, xmp)
    x.properties = {}
    x:enable()
    return x
end

function xmp:enable()
    local this = self
    function property.GUI_STATE:handler(value)
        if value == GUISTATE_QR then
            this:write()
        end
    end
end

function xmp:disable()
    property.GUI_STATE.handler = nil
end

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