module(..., package.seeall)
---downloading AUR stuff---
local lfs = require "lfs"
local socket = require "socket"

    function receive (connection)
        print("receive")
      connection:settimeout(0)   -- do not block
      local s, status = connection:receive(2^10)
      if status == "timeout" then
        coroutine.yield(connection)
      end
      return s, status
    end

--[[
    function download (host, file)
        print("download")
      local c = assert(socket.connect(host, 80))
      local count = 0    -- counts number of bytes read
      c:send("GET " .. file .. " HTTP/1.1\r\n")
      c:send("User-Agent: curl/7.19.7 (x86_64-unknown-linux-gnu) libcurl/7.19.7 OpenSSL/0.9.8l zlib/1.2.3.3\r\n")
      c:send("Host: ".. host .. "\r\n")
      --c:send("Host: aur.archlinux.org\r\n")
      c:send("Accept: */*\r\n\r\n")
      while true do
        local s, status = receive(c)
        count = count + #s
        --print(s)
        if status == "closed" then break end
      end
      c:close()
      print(file, count)
    end
--]]
local http = require "socket.http"
local ltn12 = require "ltn12"
--socket.http.TIMEOUT = 0

   ---[[
function download(host, file)
    local filename = file:match(".+/(.+)$")
    local foldername = file:match(".+/(.+)/.+$")
    --print(filename, foldername)
    lfs.mkdir("/tmp/clyde")
    lfs.mkdir("/tmp/clyde/"..foldername)
    local f, err = io.open("/tmp/clyde/"..foldername.. "/" ..filename, "w")
    --print(f, err)
    local received = 0
    --local size = 20000000
    local r, c, h = http.request {
  method = "HEAD",
  url = "http://" ..host..file
  }

  local size = h['content-length']

    http.request{url = "http://" .. host .. file,
        --sink = ltn12.sink.file(f),
        sink = ltn12.sink.chain(function(chunk)
                --local chunk = chunk or ""
                if not chunk then return chunk end
                received = received + #chunk
                local progress = received / size * 100
                local hashes = string.rep("#", progress / 2)
                local endpipe = string.rep(" ", 50 - #hashes).."|"
                io.write(string.format("Downloading %s %d%%\n%s%s\27[1A\r", filename, progress, hashes, endpipe))

            return chunk end,
            ltn12.sink.file(f)
            ),
        --[[
        sink = (function()
            local totalsize = 2000
                    --local f1, err = io.open("clydelib/sync.lua"):seek("end")
                    --local f1, err = io.open("/tmp/clyde/"..foldername.."/"..filename, "rb")
                    --print(f1, err)
                    --local size = f1:seek("end")
                    --f1:close()
                    --local f1 = f:seek("end")
                    --io.write(size)
                    local received = received or 0
                    -- received = received + size or 0
                    local progress = received / (totalsize) * 100
                    local hashes = string.rep("#", progress / 2)
                    local endpipe = string.rep(" ", 50 - #hashes) .. "|"
                   -- io.write(string.format("Downloading %s %d%%\n%s%s\27[1A\r", file, progress, hashes, endpipe))
                    return ltn12.sink.file(f)
                end)
                --]]
            --create = function() local sock = socket.tcp(); sock:settimeout(0); return sock end

}
--print("downloading")
io.write("\n\n")
end
--]]

    threads = {}    -- list of all live threads
    function get (host, file)
      -- create coroutine
      local co = coroutine.create(function ()
        download(host, file)
      end)
      -- insert it in the list
      table.insert(threads, co)
    end


    function dispatcher ()
      while true do
        local n = #threads
        if n == 0 then break end   -- no more threads to run
        local connections = {}
        for i=1,n do
          local status, res = coroutine.resume(threads[i])
          --print("status, res", status, res)
          if not res then    -- thread finished its task?
            table.remove(threads, i)
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
--    print("HI")


    
    --host = "aur.archlinux.org"
--    host = "mirrors.kernel.org"

--    get(host, "/archlinux/community/os/x86_64/jre-6u17-1-x86_64.pkg.tar.gz")
    --get(host, "/archlinux/extra/os/x86_64/ghc-6.10.4-1-x86_64.pkg.tar.gz")
--    get(host, "/archlinux/extra/os/x86_64/hugs98-200609-3-x86_64.pkg.tar.gz")
--    get(host, "/archlinux/extra/os/x86_64/lua-5.1.4-4-x86_64.pkg.tar.gz")
    --[[
    get(host, "/packages/2gis-novosibirsk/2gis-novosibirsk/PKGBUILD")
    get(host, "/packages/clamz/clamz.tar.gz")
    get(host, "/packages/ace/ace/PKGBUILD")
    get(host, "/packages/neverball-svn/neverball-svn.tar.gz")
    get(host, "/packages/lcurses/lcurses.tar.gz")
    get(host, "/packages/wxlua/wxlua.tar.gz")
    get(host, "/packages/toluapp/toluapp.tar.gz")
    get(host, "/packages/thrift/thrift.tar.gz")
    get(host, "/packages/nhc98/nhc98.tar.gz")
    get(host, "/packages/bin32-jre/bin32-jre.tar.gz")
    get(host, "/packages/jre-dev/jre-dev.tar.gz")
    get(host, "/packages/openjdk7/openjdk7.tar.gz")
    --]]
    
    --download("aur.archlinux.org", "/packages/ace/ace/PKGBUILD")
    --host = "www.w3.org"
    --get(host, "/TR/REC-html32.html")
    --download(host, "/TR/REC-html32.html")

--    dispatcher()
