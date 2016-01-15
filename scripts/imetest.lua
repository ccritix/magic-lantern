menu.new
{
    parent = "Debug",
    name = "IME Test",
    select = function(self)
      task.create(function()
        console.show()
        console.write("Starting IME...\n")
        local text = ime.gets("IME TEST!")
        console.write("IME result..\n")
        if text == nil then
          console.write("User Canceled!")
        else
          console.write(text)
        end
      end)
    end
}
