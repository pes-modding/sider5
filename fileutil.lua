local ffi = require('ffi')
local C = ffi.C

ffi.cdef [[
uint32_t GetShortPathNameW(void *lpszLongPath, void *lpszShortPath, uint32_t cchBuffer);
int MultiByteToWideChar(int CodePage, uint32_t dwFlags, char *cbMultiByByteStr, int cbMultiByte, void *lpWideCharStr, int cchWideChar);
int WideCharToMultiByte(int CodePage, uint32_t dwFlags, void *cbWideCharStr, int cchWideChar, char *cbMultiByteStr, int cbMultiByte, void *lpDefaultChar, void *lpUsedDefaultChar);
void *_wfopen(uint32_t *filename, uint32_t *mode);
void fclose(void *file);
]]

local m = {}

local _io_open = io.open

function m.open(filename, mode, ...)
    local cch = #filename
    local code_page = 65001  -- CP_UTF8

    local long_path = ffi.new(string.format('uint32_t[%s]',cch+1),{})
    local c_count = C.MultiByteToWideChar(code_page, 0, ffi.cast('char*',filename), cch, long_path, cch);
    if c_count == 0 then
        error('filename must be a valid UTF-8 string')
    end

    -- if we are openning the file for writing, we need to create it first
    -- before we can get a short pathname for it
    if mode and string.match(mode,'[aw+]') then
        local mode_lp = ffi.new('uint32_t[128]',{})
        C.MultiByteToWideChar(code_page, 0, ffi.cast('char*',mode), #mode, mode_lp, #mode);
        local f = C._wfopen(long_path, mode_lp)
        if not f then
            return nil, string.format('unable to open for writing: %s', filename)
        end
        C.fclose(f)
    end

    local short_path = ffi.new(string.format('uint32_t[%s]',c_count+2),{})
    c_count = C.GetShortPathNameW(long_path, short_path, c_count)
    if c_count == 0 then
        return nil, string.format('unable to open: %s', filename)
    end

    local new_utf8_str = ffi.new(string.format('char[%s]',c_count*2+1),{})
    local null_ptr = ffi.new('char*')
    c_count = C.WideCharToMultiByte(code_page, 0, short_path, c_count, new_utf8_str, c_count*2, null_ptr, null_ptr);
    if c_count == 0 then
        error(string.format('internal error when trying to open: %s', filename))
    end

    local short_filename = ffi.string(new_utf8_str)
    return _io_open(short_filename, mode, ...)
end

return m
