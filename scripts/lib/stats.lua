-- from http://github.com/corinadima/wordcomp/blob/master/utils/stats.lua
-- From http://lua-users.org/wiki/SimpleStats
-- Small stats library                      --
----------------------------------------------
-- Version History --
-- 1.0 First written.

-- Tables supplied as arguments are not changed.

-- Quartile computing - from http://en.wikipedia.org/wiki/Quartile
-- Method 1
-- Use the median to divide the ordered data set into two halves. Do not include the median in either half.
-- The lower quartile value is the median of the lower half of the data. The upper quartile value is the median of the upper half of the data.
-- This rule is employed by the TI-83 calculator boxplot and "1-Var Stats" functions.

-- Table to hold statistical functions

stats={}


-- Get the median of a table.
function getMedian(sortedTable)
  local median
  local lowerSplit = {}
  local upperSplit = {}
  -- If we have an even number of table elements or odd.
  if math.fmod(#sortedTable,2) == 0 then
    -- return mean value of middle two elements
    median = ( sortedTable[#sortedTable/2] + sortedTable[(#sortedTable/2)+1] ) / 2
    for i = 1, #sortedTable do
      if (i <= #sortedTable/2) then
        table.insert(lowerSplit, sortedTable[i])
      else 
        table.insert(upperSplit, sortedTable[i])
      end
    end
  else
    -- return middle element
    median =  sortedTable[math.ceil(#sortedTable/2)]
    for i = 1, #sortedTable do
      if (i < math.ceil(#sortedTable/2)) then
        table.insert(lowerSplit, sortedTable[i])
      elseif (i > math.ceil(#sortedTable/2)) then
        table.insert(upperSplit, sortedTable[i])
      end
    end
  end

  return median, lowerSplit, upperSplit  
end

  -- Method 1 from https://en.wikipedia.org/wiki/Quartile:
  -- Use the median to divide the ordered data set into two halves. Do not include the median in either half.
  -- The lower quartile value is the median of the lower half of the data. The upper quartile value is the median of the upper half of the data.
function stats.quartiles(t)
  local temp={}

  -- deep copy table so that when we sort it, the original is unchanged
  -- also weed out any non numbers
  for k,v in pairs(t) do
    if type(v) == 'number' then
      table.insert( temp, v )
    end
  end

  table.sort( temp )

  local median, lowerSplit, upperSplit = getMedian(temp)
  local q1, _, _ = getMedian(lowerSplit)
  local q3, _, _ = getMedian(upperSplit)

  return q1, median, q3, temp
end

function stats.test()
  local t1 = {6, 7, 15, 36, 39, 40, 41, 42, 43, 47, 49}
  print(t1)
  print(stats.quartiles(t1))
  local t2 = {7, 15, 36, 39, 40, 41}
  print(t2)
  print(stats.quartiles(t2))
end

return stats
