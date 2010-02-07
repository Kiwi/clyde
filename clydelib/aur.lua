module(..., package.seeall)
---downloading AUR stuff---
local lfs = require "lfs"
local socket = require "socket"
local http = require "socket.http"
local ltn12 = require "ltn12"

local zlib = require "zlib"
local yajl = require "yajl"

function download(host, file)
    local filename = file:match(".+/(.+)$")
    local foldername = file:match(".+/(.+)/.+$")
    lfs.mkdir("/tmp/clyde")
    lfs.mkdir("/tmp/clyde/"..foldername)
    local f, err = io.open("/tmp/clyde/"..foldername.. "/" ..filename, "w")
    local received = 0
    local r, c, h = http.request {
        method = "HEAD",
        url = "http://" ..host..file
    }

  local size = h['content-length']

    http.request{
        url = "http://" .. host .. file,
        sink = ltn12.sink.chain(function(chunk)
                if not chunk then return chunk end
                received = received + #chunk
                local progress = received / size * 100
                local hashes = string.rep("#", progress / 2)
                local endpipe = string.rep(" ", 50 - #hashes).."|"
                io.write(string.format("Downloading %s %d%%\n%s%s\27[1A\r", filename, progress, hashes, endpipe))

            return chunk end,
            ltn12.sink.file(f)
            ),
}
io.write("\n\n")
end

aurthreads = {}    -- list of all live threads
function get (host, file)
      -- create coroutine
    local co = coroutine.create(function ()
        download(host, file)
      end)
      -- insert it in the list
      table.insert(aurthreads, co)
end


function dispatcher ()
    while true do
        local n = #aurthreads
        if n == 0 then break end   -- no more threads to run
        local connections = {}
        for i=1,n do
            local status, res = coroutine.resume(aurthreads[i])
            if not res then    -- thread finished its task?
                table.remove(aurthreads, i)
                break
            else    -- timeout
                table.insert(connections, res)
            end
        end
        if #connections == n then
            socket.select(connections)
        end
    end
end

function getgzip(geturl)
    local sinktbl = {}
    local head = {
        ["Accept-Encoding"] = "gzip";
    }

    local r, e = http.request {
        url = geturl;
        sink = ltn12.sink.table(sinktbl);
        headers = head;
    }
    local inflated = zlib.inflate(table.concat(sinktbl))
    return inflated:read("*a")
end