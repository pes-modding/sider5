#define UNICODE

//#include "stdafx.h"
#include <time.h>
#include <stdio.h>
#include <windows.h>
#include <list>
#include <string>
#include <unordered_map>
#include "imageutil.h"
#include "sider.h"
#include "utf8.h"
#include "common.h"
#include "patterns.h"
#include "memlib.h"

#include "lua.hpp"
#include "lauxlib.h"
#include "lualib.h"

#ifndef LUA_OK
#define LUA_OK 0
#endif

#define DBG(n) if (_config->_debug & n)

#define smaller(a,b) ((a<b)?a:b)

// "sdr5" magic string
#define MAGIC 0x35726473

//#define KNOWN_FILENAME "common\\etc\\pesdb\\Coach.bin"
#define KNOWN_FILENAME "Fox\\Scripts\\Gr\\init.lua"
char _file_to_lookup[0x80];
size_t _file_to_lookup_size = 0;

using namespace std;

CRITICAL_SECTION _cs;
lua_State *L = NULL;

struct FILE_HANDLE_INFO {
    HANDLE handle;
    DWORD size;
    DWORD sizeHigh;
    DWORD currentOffset;
    DWORD currentOffsetHigh;
    DWORD padding[4];
};

struct FILE_LOAD_INFO {
    BYTE *vtable;
    FILE_HANDLE_INFO *file_handle_info;
    DWORD dw0[2];
    LONGLONG two;
    DWORD dw1[4];
    char *cpk_filename;
    LONGLONG cpk_filesize;
    LONGLONG filesize;
    DWORD dw2[2];
    LONGLONG offset_in_cpk;
    DWORD total_bytes_to_read;
    DWORD max_bytes_to_read;
    DWORD bytes_to_read;
    DWORD bytes_read_so_far;
    DWORD dw3[2];
    LONGLONG buffer_size;
    BYTE *buffer;
    BYTE *buffer2;
};

struct READ_STRUCT {
    BYTE b0[0xa0];
    LONGLONG filesize;
    FILE_HANDLE_INFO *fileinfo;
    union {
        struct {
            DWORD low;
            DWORD high;
        } parts;
        LONGLONG full;
    } offset;
    BYTE b1[0x20];
    char filename[0x80];
};

struct BUFFER_INFO {
    LONGLONG data0;
    BYTE *someptr;
    LONGLONG data1;
    BYTE *buffer;
    BYTE *buffer2;
    BYTE b0[0x1c0];
    char *filename;
};

struct FILE_INFO {
    DWORD size;
    DWORD size_uncompressed;
    LONGLONG offset_in_cpk;
};

struct STAD_STRUCT {
    DWORD stadium;
    DWORD timeofday;
    DWORD weather;
    DWORD season;
};

struct MATCH_INFO_STRUCT {
    DWORD dw0;
    DWORD dw1;
    WORD match_id;
    WORD tournament_id_encoded;
    BYTE match_leg;
    BYTE unknown0[3];
    BYTE match_info;
    BYTE unknown1[3];
    DWORD unknown2[2];
    BYTE match_time;
    BYTE unknown3[3];
    DWORD unknown4[4];
    BYTE db0x03;
    BYTE db0x17;
    BYTE stadium_choice;
    BYTE unknown5;
    DWORD unknown6[3];
    DWORD weather_effects;
    DWORD unknown7[10];
    struct STAD_STRUCT stad;
    DWORD home_team_encoded;
};
// home team encoded-id offset: 0x104
// home team name offset:       0x108
// away team encoded-id offset: 0x624
// away team name offset:       0x628

MATCH_INFO_STRUCT *_main_mi = NULL;

int get_context_field_int(const char *name);
void set_context_field_boolean(const char *name, bool value);
void set_context_field_int(const char *name, int value);
void set_context_field_nil(const char *name);
void clear_context();

const char *_context_fields[] = {
    "match_id", "match_info", "match_leg", "match_time",
    "away_team", "home_team", "stadium_choice", "stadium",
    "weather", "weather_effects", "timeofday", "season",
    "tournament_id",
};
size_t _context_fields_count = sizeof(_context_fields)/sizeof(const char *);

typedef unordered_map<string,wstring*> lookup_cache_t;
lookup_cache_t _lookup_cache;

//typedef LONGLONG (*pfn_alloc_mem_t)(BUFFER_INFO *bi, LONGLONG size);
//pfn_alloc_mem_t _org_alloc_mem;

extern "C" BOOL sider_read_file(
    HANDLE       hFile,
    LPVOID       lpBuffer,
    DWORD        nNumberOfBytesToRead,
    LPDWORD      lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped,
    struct READ_STRUCT *rs);

extern "C" void sider_get_size(char *filename, struct FILE_INFO *fi);

extern "C" BOOL sider_read_file_hk(
    HANDLE       hFile,
    LPVOID       lpBuffer,
    DWORD        nNumberOfBytesToRead,
    LPDWORD      lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped);

extern "C" void sider_get_size_hk();

extern "C" void sider_extend_cpk_hk();

extern "C" void sider_mem_copy(BYTE *dst, LONGLONG dst_len, BYTE *src, LONGLONG src_len, struct READ_STRUCT *rs);

extern "C" void sider_mem_copy_hk();

extern "C" void sider_lookup_file(LONGLONG p1, LONGLONG p2, char *filename);

extern "C" void sider_lookup_file_hk();

extern "C" void sider_set_team_id(DWORD *dest, DWORD *team_id_encoded, DWORD offset);

extern "C" void sider_set_team_id_hk();

extern "C" void sider_set_settings(STAD_STRUCT *dest_ss, STAD_STRUCT *src_ss);

extern "C" void sider_set_settings_hk();

extern "C" WORD sider_trophy_check(WORD tournament_id);

extern "C" WORD sider_trophy_check_hk(WORD tournament_id);

extern "C" void sider_context_reset();

extern "C" void sider_context_reset_hk();

extern "C" void sider_free_select_hk();

extern "C" void sider_free_select(BYTE *controller_restriction);

static DWORD dwThreadId;
static DWORD hookingThreadId = 0;
static HMODULE myHDLL;
static HHOOK handle;

bool _is_game(false);
bool _is_sider(false);
bool _is_edit_mode(false);
HANDLE _mh = NULL;

typedef struct {
    void *value;
    __time64_t expires;
} cache_map_value_t;

typedef unordered_map<string,cache_map_value_t> cache_map_t;

class cache_t {
    cache_map_t _map;
    int _ttl_sec;
    CRITICAL_SECTION *_kcs;
public:
    cache_t(CRITICAL_SECTION *cs, int ttl_sec) :
        _ttl_sec(ttl_sec), _kcs(cs) {}
    ~cache_t() {
        log_(L"cache: size:%d\n", _map.size());
    }
    bool lookup(char *filename, void **res) {
        EnterCriticalSection(_kcs);
        cache_map_t::iterator i = _map.find(filename);
        if (i != _map.end()) {
            __time64_t ltime;
            _time64(&ltime);
            //logu_("key_cache::lookup: %s %llu > %llu\n", filename, i->second.expires, ltime);
            if (i->second.expires > ltime) {
                // hit
                *res = i->second.value;
                //logu_("lookup FOUND: (%08x) %s\n", i->first, filename);
                LeaveCriticalSection(_kcs);
                return true;
            }
            else {
                // hit, but expired value, so: miss
                //logu_("lookup FALSE MATCH: (%08x) %s\n", i->first, filename);
                _map.erase(i);
            }
        }
        else {
            // miss
        }
        *res = NULL;
        LeaveCriticalSection(_kcs);
        return false;
    }
    void put(char *filename, void *value) {
        EnterCriticalSection(_kcs);
        __time64_t ltime;
        _time64(&ltime);
        cache_map_value_t v;
        v.value = value;
        v.expires = ltime + _ttl_sec;
        /*
        logu_("key_cache::put: key = %s\n", filename);
        if (v.key) {
            log_(L"key_cache::put: %s, %llu\n", v.key->c_str(), v.expires);
        }
        else {
            log_(L"key_cache::put: NULL, %llu\n", v.expires);
        }
        */
        pair<cache_map_t::iterator,bool> res = _map.insert(
            pair<string,cache_map_value_t>(filename, v));
        if (!res.second) {
            // replace existing
            //logu_("REPLACED for: %s\n", filename);
            res.first->second.value = v.value;
            res.first->second.expires = v.expires;
        }
        LeaveCriticalSection(_kcs);
    }
};

cache_t *_key_cache(NULL);
cache_t *_rewrite_cache(NULL);

// optimization: count for all registered handlers of "livecpk_rewrite" event
// if it is 0, then no need to call do_rewrite
int _rewrite_count(0);

struct module_t {
    lookup_cache_t *cache;
    lua_State* L;
    int evt_trophy_check;
    int evt_lcpk_make_key;
    int evt_lcpk_get_filepath;
    int evt_lcpk_rewrite;
    int evt_set_teams;
    /*
    int evt_set_tid;
    */
    int evt_set_match_time;
    /*
    int evt_set_stadium_choice;
    */
    int evt_set_stadium;
    int evt_set_conditions;
    int evt_after_set_conditions;
    /*
    int evt_set_stadium_for_replay;
    int evt_set_conditions_for_replay;
    int evt_after_set_conditions_for_replay;
    int evt_get_ball_name;
    int evt_get_stadium_name;
    int evt_enter_edit_mode;
    int evt_exit_edit_mode;
    int evt_enter_replay_gallery;
    int evt_exit_replay_gallery;
    */
};
list<module_t*> _modules;
module_t* _curr_m;

wchar_t module_filename[MAX_PATH];
wchar_t dll_log[MAX_PATH];
wchar_t dll_ini[MAX_PATH];
wchar_t sider_dir[MAX_PATH];

static void string_strip_quotes(wstring& s)
{
    static const wchar_t* chars = L" \t\n\r\"'";
    int e = s.find_last_not_of(chars);
    s.erase(e + 1);
    int b = s.find_first_not_of(chars);
    s.erase(0,b);
}

class config_t {
public:
    int _debug;
    bool _livecpk_enabled;
    bool _lookup_cache_enabled;
    bool _lua_enabled;
    bool _luajit_extensions_enabled;
    list<wstring> _lua_extra_globals;
    int _dll_mapping_option;
    int _key_cache_ttl_sec;
    int _rewrite_cache_ttl_sec;
    wstring _section_name;
    list<wstring> _cpk_roots;
    list<wstring> _exe_names;
    list<wstring> _module_names;
    bool _close_sider_on_exit;
    bool _start_minimized;
    bool _free_side_select;
    int _num_minutes;
    BYTE *_hp_at_read_file;
    BYTE *_hp_at_get_size;
    BYTE *_hp_at_extend_cpk;
    BYTE *_hp_at_mem_copy;
    BYTE *_hp_at_lookup_file;
    BYTE *_hp_at_set_team_id;
    BYTE *_hp_at_set_settings;
    BYTE *_hp_at_trophy_check;
    BYTE *_hp_at_context_reset;

    BYTE *_hp_at_set_min_time;
    BYTE *_hp_at_set_max_time;
    BYTE *_hp_at_set_minutes;
    BYTE *_hp_at_sider;

    ~config_t() {}
    config_t(const wstring& section_name, const wchar_t* config_ini) :
                 _section_name(section_name),
                 _debug(0),
                 _livecpk_enabled(false),
                 _lookup_cache_enabled(true),
                 _lua_enabled(true),
                 _luajit_extensions_enabled(false),
                 _close_sider_on_exit(false),
                 _start_minimized(false),
                 _free_side_select(false),
                 _key_cache_ttl_sec(10),
                 _rewrite_cache_ttl_sec(10),
                 _hp_at_read_file(NULL),
                 _hp_at_get_size(NULL),
                 _hp_at_extend_cpk(NULL),
                 _hp_at_mem_copy(NULL),
                 _hp_at_lookup_file(NULL),
                 _hp_at_set_team_id(NULL),
                 _hp_at_set_settings(NULL),
                 _hp_at_trophy_check(NULL),
                 _hp_at_context_reset(NULL),
                 _hp_at_set_min_time(NULL),
                 _hp_at_set_max_time(NULL),
                 _hp_at_set_minutes(NULL),
                 _hp_at_sider(NULL),
                 _num_minutes(0)
    {
        wchar_t settings[32767];
        RtlZeroMemory(settings, sizeof(settings));
        GetPrivateProfileSection(_section_name.c_str(),
            settings, sizeof(settings)/sizeof(wchar_t), config_ini);

        wchar_t* p = settings;
        while (*p) {
            wstring pair(p);
            wstring key(pair.substr(0, pair.find(L"=")));
            wstring value(pair.substr(pair.find(L"=")+1));
            string_strip_quotes(value);

            if (wcscmp(L"exe.name", key.c_str())==0) {
                _exe_names.push_back(value);
            }
            else if (wcscmp(L"lua.module", key.c_str())==0) {
                _module_names.push_back(value);
            }
            else if (wcscmp(L"lua.extra-globals", key.c_str())==0) {
                bool done(false);
                int start = 0, end = 0;
                while (!done) {
                    end = value.find(L",", start);
                    done = (end == string::npos);

                    wstring name((done) ?
                        value.substr(start) :
                        value.substr(start, end - start));
                    string_strip_quotes(name);
                    if (!name.empty()) {
                        _lua_extra_globals.push_back(name);
                    }
                    start = end + 1;
                }
            }
            else if (wcscmp(L"cpk.root", key.c_str())==0) {
                if (value[value.size()-1] != L'\\') {
                    value += L'\\';
                }
                // handle relative roots
                if (value[0]==L'.') {
                    wstring rel(value);
                    value = sider_dir;
                    value += rel;
                }
                _cpk_roots.push_back(value);
            }

            p += wcslen(p) + 1;
        }

        _debug = GetPrivateProfileInt(_section_name.c_str(),
            L"debug", _debug,
            config_ini);

        _close_sider_on_exit = GetPrivateProfileInt(_section_name.c_str(),
            L"close.on.exit", _close_sider_on_exit,
            config_ini);

        _start_minimized = GetPrivateProfileInt(_section_name.c_str(),
            L"start.minimized", _start_minimized,
            config_ini);

        _free_side_select = GetPrivateProfileInt(_section_name.c_str(),
            L"free.side.select", _free_side_select,
            config_ini);

        _livecpk_enabled = GetPrivateProfileInt(_section_name.c_str(),
            L"livecpk.enabled", _livecpk_enabled,
            config_ini);

        _lookup_cache_enabled = GetPrivateProfileInt(_section_name.c_str(),
            L"lookup-cache.enabled", _lookup_cache_enabled,
            config_ini);

        _lua_enabled = GetPrivateProfileInt(_section_name.c_str(),
            L"lua.enabled", _lua_enabled,
            config_ini);

        _luajit_extensions_enabled = GetPrivateProfileInt(_section_name.c_str(),
            L"luajit.ext.enabled", _luajit_extensions_enabled,
            config_ini);

        _key_cache_ttl_sec = GetPrivateProfileInt(_section_name.c_str(),
            L"key-cache.ttl-sec", _key_cache_ttl_sec,
            config_ini);

        _rewrite_cache_ttl_sec = GetPrivateProfileInt(_section_name.c_str(),
            L"rewrite-cache.ttl-sec", _rewrite_cache_ttl_sec,
            config_ini);

        _num_minutes = GetPrivateProfileInt(_section_name.c_str(),
            L"match.minutes", _num_minutes,
            config_ini);
    }
};

config_t* _config;

bool init_paths() {
    wchar_t *p;

    // prep log filename
    memset(dll_log, 0, sizeof(dll_log));
    if (GetModuleFileName(myHDLL, dll_log, MAX_PATH)==0) {
        return FALSE;
    }
    p = wcsrchr(dll_log, L'.');
    wcscpy(p, L".log");

    // prep ini filename
    memset(dll_ini, 0, sizeof(dll_ini));
    wcscpy(dll_ini, dll_log);
    p = wcsrchr(dll_ini, L'.');
    wcscpy(p, L".ini");

    // prep sider dir
    memset(sider_dir, 0, sizeof(sider_dir));
    wcscpy(sider_dir, dll_log);
    p = wcsrchr(sider_dir, L'\\');
    *(p+1) = L'\0';

    return true;
}

static int sider_log(lua_State *L) {
    const char *s = luaL_checkstring(L, -1);
    lua_getfield(L, lua_upvalueindex(1), "_FILE");
    const char *fname = lua_tostring(L, -1);
    logu_("[%s] %s\n", fname, s);
    lua_pop(L, 2);
    return 0;
}

void read_configuration(config_t*& config)
{
    wchar_t names[1024];
    size_t names_len = sizeof(names)/sizeof(wchar_t);
    GetPrivateProfileSectionNames(names, names_len, dll_ini);

    wchar_t *p = names;
    while (p && *p) {
        wstring name(p);
        if (name == L"sider") {
            config = new config_t(name, dll_ini);
            break;
        }
        p += wcslen(p) + 1;
    }
}

static bool skip_process(wchar_t* name)
{
    wchar_t *filename = wcsrchr(name, L'\\');
    if (filename) {
        if (wcsicmp(filename, L"\\explorer.exe") == 0) {
            return true;
        }
        if (wcsicmp(filename, L"\\steam.exe") == 0) {
            return true;
        }
        if (wcsicmp(filename, L"\\steamwebhelper.exe") == 0) {
            return true;
        }
    }
    return false;
}

static bool is_sider(wchar_t* name)
{
    wchar_t *filename = wcsrchr(name, L'\\');
    if (filename) {
        if (wcsicmp(filename, L"\\sider.exe") == 0) {
            return true;
        }
    }
    return false;
}

static bool write_mapping_info(config_t *config)
{
    // determine the size needed
    DWORD size = sizeof(wchar_t);
    list<wstring>::iterator it;
    for (it = _config->_exe_names.begin();
            it != _config->_exe_names.end();
            it++) {
        size += sizeof(wchar_t) * (it->size() + 1);
    }

    _mh = CreateFileMapping(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_COMMIT,
        0, size, SIDER_FM);
    if (!_mh) {
        log_(L"W: CreateFileMapping FAILED: %d\n", GetLastError());
        return false;
    }
    wchar_t *mem = (wchar_t*)MapViewOfFile(_mh, FILE_MAP_WRITE, 0, 0, 0);
    if (!mem) {
        log_(L"W: MapViewOfFile FAILED: %d\n", GetLastError());
        CloseHandle(_mh);
        return false;
    }

    memset(mem, 0, size);
    for (it = config->_exe_names.begin();
            it != _config->_exe_names.end();
            it++) {
        wcscpy(mem, it->c_str());
        mem += it->size() + 1;
    }
    return true;
}

static bool is_pes(wchar_t* name, wstring** match)
{
    HANDLE h = OpenFileMapping(FILE_MAP_READ, FALSE, SIDER_FM);
    if (!h) {
        int err = GetLastError();
        wchar_t *t = new wchar_t[MAX_PATH];
        GetModuleFileName(NULL, t, MAX_PATH);
        log_(L"R: OpenFileMapping FAILED (for %s): %d\n", t, err);
        delete t;
        return false;
    }
    BYTE *patterns = (BYTE*)MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
    if (!patterns) {
        int err= GetLastError();
        wchar_t *t = new wchar_t[MAX_PATH];
        GetModuleFileName(NULL, t, MAX_PATH);
        log_(L"R: MapViewOfFile FAILED (for %s): %d\n", t, err);
        delete t;
        CloseHandle(h);
        return false;
    }

    bool result = false;
    wchar_t *filename = wcsrchr(name, L'\\');
    if (filename) {
        wchar_t *s = (wchar_t*)patterns;
        while (*s != L'\0') {
            if (wcsicmp(filename, s) == 0) {
                *match = new wstring(s);
                result = true;
                break;
            }
            s = s + wcslen(s) + 1;
        }
    }
    UnmapViewOfFile(h);
    CloseHandle(h);
    return result;
}

wstring* _have_live_file(char *file_name)
{
    wchar_t unicode_filename[512];
    memset(unicode_filename, 0, sizeof(unicode_filename));
    Utf8::fUtf8ToUnicode(unicode_filename, file_name);

    wchar_t fn[512];
    for (list<wstring>::iterator it = _config->_cpk_roots.begin();
            it != _config->_cpk_roots.end();
            it++) {
        memset(fn, 0, sizeof(fn));
        wcscpy(fn, it->c_str());
        wchar_t *p = (unicode_filename[0] == L'\\') ? unicode_filename + 1 : unicode_filename;
        wcscat(fn, p);

        HANDLE handle;
        handle = CreateFileW(fn,           // file to open
                           GENERIC_READ,          // open for reading
                           FILE_SHARE_READ,       // share for reading
                           NULL,                  // default security
                           OPEN_EXISTING,         // existing file only
                           FILE_ATTRIBUTE_NORMAL,  // normal file
                           NULL);                 // no attr. template

        if (handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
            return new wstring(fn);
        }
    }

    return NULL;
}

wstring* have_live_file(char *file_name)
{
    //logu_("have_live_file: %p --> %s\n", (DWORD)file_name, file_name);
    if (!_config->_lookup_cache_enabled) {
        // no cache
        return _have_live_file(file_name);
    }
    unordered_map<string,wstring*>::iterator it;
    it = _lookup_cache.find(string(file_name));
    if (it != _lookup_cache.end()) {
        return it->second;
    }
    else {
        //logu_("_lookup_cache MISS for (%s)\n", file_name);
        wstring* res = _have_live_file(file_name);
        _lookup_cache.insert(pair<string,wstring*>(string(file_name),res));
        return res;
    }
}

bool file_exists(wstring *fullpath, LONGLONG *size)
{
    HANDLE handle = CreateFileW(
        fullpath->c_str(),     // file to open
        GENERIC_READ,          // open for reading
        FILE_SHARE_READ,       // share for reading
        NULL,                  // default security
        OPEN_EXISTING,         // existing file only
        FILE_ATTRIBUTE_NORMAL,  // normal file
        NULL);                 // no attr. template

    if (handle != INVALID_HANDLE_VALUE)
    {
        if (size != NULL) {
            DWORD *p = (DWORD*)size;
            *size = GetFileSize(handle, p+1);
        }
        CloseHandle(handle);
        return true;
    }
    return false;
}

void clear_context_fields(const char **names, size_t num_items)
{
    if (_config->_lua_enabled) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(L, 1); // ctx
        for (int i=0; i<num_items; i++) {
            lua_pushnil(L);
            lua_setfield(L, -2, names[i]);
        }
        lua_pop(L, 1);
        LeaveCriticalSection(&_cs);
    }
}

int get_context_field_int(const char *name, int default_value)
{
    int value = default_value;
    if (_config->_lua_enabled) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(L, 1); // ctx
        lua_getfield(L, -1, name);
        if (lua_isnumber(L, -1)) {
            value = luaL_checkinteger(L, -1);
        }
        lua_pop(L, 2);
        LeaveCriticalSection(&_cs);
    }
    return value;
}

void set_context_field_int(const char *name, int value)
{
    if (_config->_lua_enabled) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(L, 1); // ctx
        lua_pushinteger(L, value);
        lua_setfield(L, -2, name);
        lua_pop(L, 1);
        LeaveCriticalSection(&_cs);
    }
}

void set_context_field_nil(const char *name)
{
    if (_config->_lua_enabled) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(L, 1); // ctx
        lua_pushnil(L);
        lua_setfield(L, -2, name);
        lua_pop(L, 1);
        LeaveCriticalSection(&_cs);
    }
}

void set_context_field_boolean(const char *name, bool value)
{
    if (_config->_lua_enabled) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(L, 1); // ctx
        lua_pushboolean(L, (value)?1:0);
        lua_setfield(L, -2, name);
        lua_pop(L, 1);
        LeaveCriticalSection(&_cs);
    }
}

void set_match_info(MATCH_INFO_STRUCT* mis)
{
    int match_id = (int)(mis->match_id);
    int match_leg = (int)(mis->match_leg);
    int match_info = (int)(mis->match_info);

    if (match_id != 0 && (match_leg == 0 || match_leg == 1)) {
        set_context_field_int("match_leg", match_leg+1);
    }
    else {
        set_context_field_nil("match_leg");
    }
    set_context_field_int("match_id", match_id);
    if (match_info < 128) {
        set_context_field_int("match_info", match_info);
    }
}

bool module_trophy_rewrite(module_t *m, WORD tournament_id, WORD *new_tid)
{
    *new_tid = tournament_id;
    bool assigned(false);
    if (m->evt_trophy_check != 0) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(m->L, m->evt_trophy_check);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        lua_pushinteger(L, tournament_id);
        if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR: %s\n",
                GetCurrentThreadId(), err);
        }
        else if (lua_isnumber(L, -1)) {
            *new_tid = (WORD)luaL_checkint(L, -1);
            assigned = true;
        }
        lua_pop(L, 1);
        LeaveCriticalSection(&_cs);
    }
    return assigned;
}

bool module_set_match_time(module_t *m, DWORD *num_minutes)
{
    bool res(false);
    if (m->evt_set_match_time != 0) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(m->L, m->evt_set_match_time);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        lua_pushinteger(L, *num_minutes);
        if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
        }
        else if (lua_isnumber(L, -1)) {
            int value = luaL_checkinteger(L, -1);
            *num_minutes = value;
            res = true;
        }
        lua_pop(L, 1);
        LeaveCriticalSection(&_cs);
    }
    return res;
}

bool module_set_stadium(module_t *m, MATCH_INFO_STRUCT *mi)
{
    bool res(false);
    if (m->evt_set_stadium != 0) {
        EnterCriticalSection(&_cs);
        STAD_STRUCT *ss = &(mi->stad);
        lua_pushvalue(m->L, m->evt_set_stadium);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        lua_newtable(L);
        lua_pushinteger(L, ss->stadium);
        lua_setfield(L, -2, "stadium");
        lua_pushinteger(L, ss->timeofday);
        lua_setfield(L, -2, "timeofday");
        lua_pushinteger(L, ss->weather);
        lua_setfield(L, -2, "weather");
        lua_pushinteger(L, mi->weather_effects);
        lua_setfield(L, -2, "weather_effects");
        lua_pushinteger(L, ss->season);
        lua_setfield(L, -2, "season");
        if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
        }
        else if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "stadium");
            if (lua_isnumber(L, -1)) {
                ss->stadium = luaL_checkinteger(L, -1);
            }
            lua_pop(L, 1);
            res = true;
        }
        else if (lua_isnumber(L, -1)) {
            ss->stadium = luaL_checkinteger(L, -1);
            lua_pop(L, 1);
            res = true;
        }
        lua_pop(L, 1);
        LeaveCriticalSection(&_cs);
    }
    return res;
}

bool module_set_conditions(module_t *m, MATCH_INFO_STRUCT *mi)
{
    bool res(false);
    if (m->evt_set_conditions != 0) {
        EnterCriticalSection(&_cs);
        STAD_STRUCT *ss = &(mi->stad);
        lua_pushvalue(m->L, m->evt_set_conditions);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        lua_newtable(L);
        lua_pushinteger(L, ss->stadium);
        lua_setfield(L, -2, "stadium");
        lua_pushinteger(L, ss->timeofday);
        lua_setfield(L, -2, "timeofday");
        lua_pushinteger(L, ss->weather);
        lua_setfield(L, -2, "weather");
        lua_pushinteger(L, mi->weather_effects);
        lua_setfield(L, -2, "weather_effects");
        lua_pushinteger(L, ss->season);
        lua_setfield(L, -2, "season");
        if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
        }
        else if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "timeofday");
            if (lua_isnumber(L, -1)) {
                ss->timeofday = luaL_checkinteger(L, -1);
            }
            lua_pop(L, 1);
            lua_getfield(L, -1, "weather");
            if (lua_isnumber(L, -1)) {
                ss->weather = luaL_checkinteger(L, -1);
            }
            lua_pop(L, 1);
            lua_getfield(L, -1, "weather_effects");
            if (lua_isnumber(L, -1)) {
                mi->weather_effects = luaL_checkinteger(L, -1);
            }
            lua_pop(L, 1);
            lua_getfield(L, -1, "season");
            if (lua_isnumber(L, -1)) {
                ss->season = luaL_checkinteger(L, -1);
            }
            lua_pop(L, 1);
            res = true;
        }
        lua_pop(L, 1);
        LeaveCriticalSection(&_cs);
    }
    return res;
}

void module_after_set_conditions(module_t *m)
{
    if (m->evt_after_set_conditions != 0) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(m->L, m->evt_after_set_conditions);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
        }
        LeaveCriticalSection(&_cs);
    }
}

void module_set_teams(module_t *m, DWORD home, DWORD away)
{
    if (m->evt_set_teams != 0) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(m->L, m->evt_set_teams);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        lua_pushinteger(L, home);
        lua_pushinteger(L, away);
        if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
            lua_pop(L, 1);
        }
        LeaveCriticalSection(&_cs);
    }
}

char *module_rewrite(module_t *m, const char *file_name)
{
    char *res(NULL);
    EnterCriticalSection(&_cs);
    lua_pushvalue(m->L, m->evt_lcpk_rewrite);
    lua_xmove(m->L, L, 1);
    // push params
    lua_pushvalue(L, 1); // ctx
    lua_pushstring(L, file_name);
    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        const char *err = luaL_checkstring(L, -1);
        logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
    }
    else if (lua_isstring(L, -1)) {
        const char *s = luaL_checkstring(L, -1);
        res = strdup(s);
    }
    lua_pop(L, 1);
    LeaveCriticalSection(&_cs);
    return res;
}

void module_make_key(module_t *m, const char *file_name, char *key, size_t key_maxsize)
{
    key[0] = '\0';
    size_t maxlen = key_maxsize-1;
    if (m->evt_lcpk_make_key != 0) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(m->L, m->evt_lcpk_make_key);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        lua_pushstring(L, file_name);
        if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
            // fallback to filename
            strncat(key, file_name, maxlen);
        }
        else if (lua_isstring(L, -1)) {
            const char *s = luaL_checkstring(L, -1);
            strncat(key, s, maxlen);
        }
        else {
            // fallback to filename
            strncat(key, file_name, maxlen);
        }
        lua_pop(L, 1);
        LeaveCriticalSection(&_cs);
    }
    else {
        // default to filename
        strncat(key, file_name, maxlen);
    }
}

wstring *module_get_filepath(module_t *m, const char *file_name, char *key)
{
    wstring *res = NULL;
    if (m->evt_lcpk_get_filepath != 0) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(m->L, m->evt_lcpk_get_filepath);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        lua_pushstring(L, file_name);
        lua_pushstring(L, (key[0]=='\0') ? NULL : key);
        if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR: %s\n",
                GetCurrentThreadId(), err);
        }
        else if (lua_isstring(L, -1)) {
            const char *s = luaL_checkstring(L, -1);
            wchar_t *ws = Utf8::utf8ToUnicode((BYTE*)s);
            res = new wstring(ws);
            Utf8::free(ws);
        }
        lua_pop(L, 1);
        LeaveCriticalSection(&_cs);

        // verify that file exists
        if (res && !file_exists(res, NULL)) {
            delete res;
            res = NULL;
        }
    }
    return res;
}

bool do_rewrite(char *file_name)
{
    char key[512];
    char *res = NULL;

    if (_config->_rewrite_cache_ttl_sec) {
        if (_rewrite_cache->lookup(file_name, (void**)&res)) {
            // rewrite-cache: for performance
            if (res) {
                strcpy(file_name, res);
            }
            return res != NULL;
        }
    }

    list<module_t*>::iterator i;
    for (i = _modules.begin(); i != _modules.end(); i++) {
        module_t *m = *i;
        if (m->evt_lcpk_rewrite != 0) {
            res = module_rewrite(m, file_name);
            if (res) {
                if (_config->_rewrite_cache_ttl_sec) _rewrite_cache->put(file_name, res);
                strcpy(file_name, res);
                return true;
            }
        }
    }

    if (_config->_rewrite_cache_ttl_sec) _rewrite_cache->put(file_name, res);
    return false;
}

wstring* have_content(char *file_name)
{
    char key[512];
    wstring *res = NULL;
    if (_config->_key_cache_ttl_sec) {
        if (_key_cache->lookup(file_name, (void**)&res)) {
            // key-cache: for performance
            return res;
        }
    }
    list<module_t*>::iterator i;
    //logu_("have_content: %p --> %s\n", (DWORD)file_name, file_name);
    for (i = _modules.begin(); i != _modules.end(); i++) {
        module_t *m = *i;
        if (!m->evt_lcpk_make_key && !m->evt_lcpk_get_filepath) {
            // neither of callbacks is defined --> nothing to do
            continue;
        }

        module_make_key(m, file_name, key, sizeof(key));
               
        if (_config->_lookup_cache_enabled) {
            unordered_map<string,wstring*>::iterator j;
            j = m->cache->find(key);
            if (j != m->cache->end()) {
                if (j->second != NULL) {
                    if (_config->_key_cache_ttl_sec) _key_cache->put(file_name, j->second);
                    return j->second;
                }
                // this module does not have the file:
                // move on to next module
                continue;
            }
            else {
                wstring *res = module_get_filepath(m, file_name, key);

                // cache the lookup result
                m->cache->insert(pair<string,wstring*>(key, res));
                if (res) {
                    // we have a file: stop and return
                    if (_config->_key_cache_ttl_sec) _key_cache->put(file_name, res);
                    return res;
                }
            }
        }
        else {
            // no cache: SLOW! ONLY use for troubleshooting
            wstring *res = module_get_filepath(m, file_name, key);
            if (res) {
                // we have a file: stop and return
                if (_config->_key_cache_ttl_sec) _key_cache->put(file_name, res);
                return res;
            }
        }
    }
    if (_config->_key_cache_ttl_sec) _key_cache->put(file_name, NULL);
    return NULL;
}

__declspec(dllexport) bool start_minimized()
{
    return _config && _config->_start_minimized;
}

inline char *get_tailname(char *filename)
{
    char *tail = filename + strlen(filename) + 1;
    if (*(DWORD*)tail == MAGIC) {
        return tail+5;
    }
    return filename;
}

void sider_get_size(char *filename, struct FILE_INFO *fi)
{
    char *fname = get_tailname(filename);
    if (fname[0]=='\0') {
        // no tail name: nothing to do
        return;
    }
    DBG(4) logu_("get_size:: tailname: %s\n", fname);

    wstring *fn;
    if (_config->_lua_enabled && _rewrite_count > 0) do_rewrite(fname);
    fn = (_config->_lua_enabled) ? have_content(fname) : NULL;
    fn = (fn) ? fn : have_live_file(fname);
    if (fn != NULL) {
        DBG(4) log_(L"get_size:: livecpk file found: %s\n", fn->c_str());
        HANDLE handle = CreateFileW(fn->c_str(),  // file to open
                           GENERIC_READ,          // open for reading
                           FILE_SHARE_READ,       // share for reading
                           NULL,                  // default security
                           OPEN_EXISTING,         // existing file only
                           FILE_ATTRIBUTE_NORMAL, // normal file
                           NULL);                 // no attr. template

        if (handle != INVALID_HANDLE_VALUE)
        {
            DWORD sz = GetFileSize(handle, NULL);
            DBG(4) log_(L"get_size:: livecpk file size: %x\n", sz);
            CloseHandle(handle);
            fi->size = sz;
            fi->size_uncompressed = sz;
            //fi->offset_in_cpk = 0;

            // restore the tail name
            strcpy(filename, fname);
        }
    }
}

BOOL sider_read_file(
    HANDLE       hFile,
    LPVOID       lpBuffer,
    DWORD        nNumberOfBytesToRead,
    LPDWORD      lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped,
    struct READ_STRUCT *rs)
{
    BOOL result;
    HANDLE orgHandle = hFile;
    DWORD orgBytesToRead = nNumberOfBytesToRead;
    HANDLE handle = INVALID_HANDLE_VALUE;
    wstring *filename = NULL;

    //log_(L"rs (R12) = %p\n", rs);
    if (rs) {
        if (_config->_lua_enabled && _rewrite_count > 0) do_rewrite(rs->filename);
        DBG(1) logu_("read_file:: rs->filesize: %llx, rs->offset: %llx, rs->filename: %s\n",
            rs->filesize, rs->offset.full, rs->filename);

        BYTE* p = (BYTE*)rs;
        FILE_LOAD_INFO *fli = *((FILE_LOAD_INFO **)(p - 0x18));

        wstring *fn;
        fn = (_config->_lua_enabled) ? have_content(rs->filename) : NULL;
        fn = (fn) ? fn : have_live_file(rs->filename);
        if (fn != NULL) {
            DBG(3) log_(L"read_file:: livecpk file found: %s\n", fn->c_str());
            handle = CreateFileW(fn->c_str(),         // file to open
                               GENERIC_READ,          // open for reading
                               FILE_SHARE_READ,       // share for reading
                               NULL,                  // default security
                               OPEN_EXISTING,         // existing file only
                               FILE_ATTRIBUTE_NORMAL, // normal file
                               NULL);                 // no attr. template

            if (handle != INVALID_HANDLE_VALUE)
            {
                DWORD sz = GetFileSize(handle, NULL);

                // replace file handle
                orgHandle = hFile;
                hFile = handle;

                // set correct offset
                LONG offsetHigh = rs->offset.parts.high;
                SetFilePointer(hFile, rs->offset.parts.low, &offsetHigh, FILE_BEGIN);
                rs->offset.parts.high = offsetHigh;
                LONGLONG offset = rs->offset.full;

                if (fli) {
                    // adjust offset for multi-part reads
                    SetFilePointer(hFile, fli->bytes_read_so_far, NULL, FILE_CURRENT);
                    offset = offset + fli->bytes_read_so_far;

                    // trace file read info
                    DBG(4) {
                        logu_("read_file:: fli->total_bytes_to_read: %x\n", fli->total_bytes_to_read);
                        logu_("read_file:: fli->max_bytes_to_read: %x\n", fli->max_bytes_to_read);
                        logu_("read_file:: fli->bytes_to_read: %x\n", fli->bytes_to_read);
                        logu_("read_file:: fli->bytes_read_so_far: %x\n", fli->bytes_read_so_far);
                        logu_("read_file:: fli->filesize: %llx\n", fli->filesize);
                        logu_("read_file:: fli->buffer_size: %llx\n", fli->buffer_size);
                        logu_("read_file:: fli->cpk_filename: %s\n", fli->cpk_filename);
                        logu_("read_file:: fli->offset_in_cpk: %llx\n", fli->offset_in_cpk);
                    }
                }

                DBG(3) log_(L"read_file:: livecpk file offset: %llx\n", offset);
            }
        }
    }

    result = ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    //log_(L"ReadFile(%x, %p, %x, %x, %p)\n",
    //    hFile, lpBuffer, nNumberOfBytesToRead, *lpNumberOfBytesRead, lpOverlapped);

    if (handle != INVALID_HANDLE_VALUE) {
        DBG(3) log_(L"read_file:: called ReadFile(%x, %p, %x, %x, %p)\n",
            hFile, lpBuffer, nNumberOfBytesToRead, *lpNumberOfBytesRead, lpOverlapped);
        CloseHandle(handle);

        if (orgBytesToRead > *lpNumberOfBytesRead) {
            //log_(L"file-size adjustment: actually read = %x, reporting as read = %x\n",
            //    *lpNumberOfBytesRead, orgBytesToRead);
        }

        // fake a read from cpk
        if (orgBytesToRead > *lpNumberOfBytesRead) {
            *lpNumberOfBytesRead = orgBytesToRead;
        }
        //SetFilePointer(orgHandle, *lpNumberOfBytesRead, 0, FILE_CURRENT);
    }

    return result;
}

void sider_mem_copy(BYTE *dst, LONGLONG dst_len, BYTE *src, LONGLONG src_len, struct READ_STRUCT *rs)
{
    HANDLE handle = INVALID_HANDLE_VALUE;
    wstring *filename = NULL;

    // do the original copy operation
    memcpy_s(dst, dst_len, src, src_len);

    if (rs) {
        if (_config->_lua_enabled && _rewrite_count > 0) do_rewrite(rs->filename);
        DBG(1) logu_("mem_copy:: rs->filesize: %llx, rs->offset: %llx, rs->filename: %s\n",
            rs->filesize, rs->offset.full, rs->filename);

        BYTE* p = (BYTE*)rs;
        FILE_LOAD_INFO *fli = *((FILE_LOAD_INFO **)(p - 0x18));

        wstring *fn;
        fn = (_config->_lua_enabled) ? have_content(rs->filename) : NULL;
        fn = (fn) ? fn : have_live_file(rs->filename);
        if (fn != NULL) {
            DBG(3) log_(L"mem_copy:: livecpk file found: %s\n", fn->c_str());
            handle = CreateFileW(fn->c_str(),         // file to open
                               GENERIC_READ,          // open for reading
                               FILE_SHARE_READ,       // share for reading
                               NULL,                  // default security
                               OPEN_EXISTING,         // existing file only
                               FILE_ATTRIBUTE_NORMAL, // normal file
                               NULL);                 // no attr. template

            if (handle != INVALID_HANDLE_VALUE)
            {
                DWORD sz = GetFileSize(handle, NULL);

                // set correct offset
                LONG offsetHigh = rs->offset.parts.high;
                SetFilePointer(handle, rs->offset.parts.low, &offsetHigh, FILE_BEGIN);
                rs->offset.parts.high = offsetHigh;
                LONGLONG offset = rs->offset.full;

                if (fli) {
                    // adjust offset for multi-part reads
                    SetFilePointer(handle, fli->bytes_read_so_far, NULL, FILE_CURRENT);
                    offset = offset + fli->bytes_read_so_far;

                    // trace file read info
                    DBG(4) {
                        logu_("mem_copy:: fli->total_bytes_to_read: %x\n", fli->total_bytes_to_read);
                        logu_("mem_copy:: fli->max_bytes_to_read: %x\n", fli->max_bytes_to_read);
                        logu_("mem_copy:: fli->bytes_to_read: %x\n", fli->bytes_to_read);
                        logu_("mem_copy:: fli->bytes_read_so_far: %x\n", fli->bytes_read_so_far);
                        logu_("mem_copy:: fli->filesize: %llx\n", fli->filesize);
                        logu_("mem_copy:: fli->buffer_size: %llx\n", fli->buffer_size);
                        logu_("mem_copy:: fli->cpk_filename: %s\n", fli->cpk_filename);
                        logu_("mem_copy:: fli->offset_in_cpk: %llx\n", fli->offset_in_cpk);
                    }
                }

                DBG(3) log_(L"mem_copy:: livecpk file offset: %llx\n", offset);

                // read data from file to destination buffer
                DWORD numberOfBytesRead = 0;
                BOOL result = ReadFile(handle, dst, src_len, &numberOfBytesRead, NULL);

                DBG(3) log_(L"mem_copy:: called ReadFile(%x, %p, %x, %x, %p)\n",
                    handle, dst, dst_len, &numberOfBytesRead, NULL);
                CloseHandle(handle);
            }
        }
    }
}

void sider_lookup_file(LONGLONG p1, LONGLONG p2, char *filename)
{
    // quick check if we already modified this path
    size_t len = strlen(filename);
    char *p = filename + len + 1;
    if (*(DWORD*)p == MAGIC) {
        // already did this.
        return;
    }
    //DBG(8) logu_("lookup_file:: looking for: %s\n", filename);

    wstring *fn;
    if (_config->_lua_enabled && _rewrite_count > 0) {
        if (do_rewrite(filename)) {
            len = strlen(filename);
            p = filename + len + 1;
        }
    }
    fn = (_config->_lua_enabled) ? have_content(filename) : NULL;
    fn = (fn) ? fn : have_live_file(filename);
    if (fn) {
        DBG(4) logu_("lookup_file:: found livecpk file for: %s\n", filename);

        // trick: pick a filename that we know exists
        // put our filename after it, separated by MAGIC marker
        char temp[0x200];
        memcpy(temp, filename, len+1);
        memcpy(filename, _file_to_lookup, _file_to_lookup_size);
        memcpy(filename + _file_to_lookup_size, temp, len+1);
    }
    else {
        // not found. But still mark it with magic
        // so that we do not search again
        *(DWORD*)p = MAGIC;
        *(p+4) = '\0';
        *(p+5) = '\0';
    }
}

DWORD decode_team_id(DWORD team_id_encoded)
{
    return (team_id_encoded >> 0x0e) & 0xffff;
}

void sider_set_team_id(DWORD *dest, DWORD *team_id_encoded, DWORD offset)
{
    bool is_home = (offset == 0);
    if (is_home) {
        logu_("setting HOME team: %d\n", decode_team_id(*team_id_encoded));
    }
    else {
        logu_("setting AWAY team: %d\n", decode_team_id(*team_id_encoded));
    }

    BYTE *p = (BYTE*)dest - 0x118;
    p = (is_home) ? p : p - 0x5ec;
    MATCH_INFO_STRUCT *mi = (MATCH_INFO_STRUCT*)p;
    //logu_("mi: %p\n", mi);
    //logu_("mi->dw0: 0x%x\n", mi->dw0);
    if (!is_home) {
        logu_("tournament_id: %d\n", mi->tournament_id_encoded);
    }

    if (_config->_lua_enabled) {
        if (is_home) {
            clear_context_fields(_context_fields, _context_fields_count);
        }
        else {
            set_context_field_int("tournament_id", mi->tournament_id_encoded);
            set_context_field_int("match_time", mi->match_time);
            set_context_field_int("stadium_choice", mi->stadium_choice);
            set_match_info(mi);

            DWORD home = decode_team_id(*(DWORD*)((BYTE*)dest - 0x5ec));
            DWORD away = decode_team_id(*team_id_encoded);

            set_context_field_int("home_team", home);
            set_context_field_int("away_team", away);

            // lua call-backs
            list<module_t*>::iterator i;
            for (i = _modules.begin(); i != _modules.end(); i++) {
                module_t *m = *i;
                module_set_teams(m, home, away);
            }
        }
    }
}

void sider_set_settings(STAD_STRUCT *dest_ss, STAD_STRUCT *src_ss)
{
    MATCH_INFO_STRUCT *mi = (MATCH_INFO_STRUCT*)((BYTE*)dest_ss - 0x6c);
    bool ok = mi && (mi->db0x03 == 0x03) && (mi->db0x17 == 0x17 || mi->db0x17 == 0x12);
    if (!ok) {
        // safety check
        return;
    }

    logu_("tournament_id: %d\n", mi->tournament_id_encoded);

    if (_config->_lua_enabled) {
        // update match info
        //set_match_info(mi);

        // lua callbacks
        list<module_t*>::iterator i;
        for (i = _modules.begin(); i != _modules.end(); i++) {
            module_t *m = *i;
            DWORD num_minutes = mi->match_time;
            if (module_set_match_time(m, &num_minutes)) {
                mi->match_time = num_minutes;
                set_context_field_int("match_time", mi->match_time);
            }
        }
        for (i = _modules.begin(); i != _modules.end(); i++) {
            module_t *m = *i;
            if (module_set_stadium(m, mi)) {
                // sync the thumbnail
                mi->stadium_choice = mi->stad.stadium;
                break;
            }
        }
        for (i = _modules.begin(); i != _modules.end(); i++) {
            module_t *m = *i;
            if (module_set_conditions(m, mi)) {
                break;
            }
        }

        set_context_field_int("stadium", dest_ss->stadium);
        set_context_field_int("timeofday", dest_ss->timeofday);
        set_context_field_int("weather", dest_ss->weather);
        set_context_field_int("weather_effects", mi->weather_effects);
        set_context_field_int("season", dest_ss->season);

        // clear stadium_choice in context
        set_context_field_nil("stadium_choice");
        //_had_stadium_choice = false;

        for (i = _modules.begin(); i != _modules.end(); i++) {
            module_t *m = *i;
            module_after_set_conditions(m);
        }
    }

    logu_("settings now:: stadium=%d, timeofday=%d, weather=%d, season=%d\n",
        dest_ss->stadium, dest_ss->timeofday, dest_ss->weather, dest_ss->season);
}

WORD sider_trophy_check(WORD tournament_id)
{
    WORD tid = tournament_id;
    if (_config->_lua_enabled) {
        // lua callbacks
        list<module_t*>::iterator i;
        for (i = _modules.begin(); i != _modules.end(); i++) {
            module_t *m = *i;
            WORD new_tid = tid;
            if (module_trophy_rewrite(m, tid, &new_tid)) {
                tid = new_tid;
                break;
            }
        }
    }
    logu_("trophy check:: for trophy scenes tournament_id: %d --> %d\n", tournament_id, tid);
    return tid;
}

void sider_context_reset()
{
    clear_context_fields(_context_fields, _context_fields_count);
    logu_("context reset\n");
}

void sider_free_select(BYTE *controller_restriction)
{
    *controller_restriction = 0;
}

BYTE* get_target_location(BYTE *call_location)
{
    if (call_location) {
        BYTE* bptr = call_location;
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(bptr, 8, newProtection, &protection)) {
            // get memory location where call target addr is stored
            // format of indirect call is like this:
            // call [addr] : FF 15 <4-byte-offset>
            DWORD* ptr = (DWORD*)(call_location + 2);
            return call_location + 6 + ptr[0];
        }
    }
    return NULL;
}

void hook_indirect_call(BYTE *loc, BYTE *p) {
    if (!loc) {
        return;
    }
    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    BYTE *addr_loc = get_target_location(loc);
    log_(L"loc: %p, addr_loc: %p\n", loc, addr_loc);
    if (VirtualProtect(addr_loc, 8, newProtection, &protection)) {
        BYTE** v = (BYTE**)addr_loc;
        *v = p;
        log_(L"hook_indirect_call: hooked at %p (target: %p)\n", loc, p);
    }
}

void hook_call(BYTE *loc, BYTE *p, size_t nops) {
    if (!loc) {
        return;
    }
    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect(loc, 16, newProtection, &protection)) {
        memcpy(loc, "\x48\xb8", 2);
        memcpy(loc+2, &p, sizeof(BYTE*));  // mov rax,<target_addr>
        memcpy(loc+10, "\xff\xd0", 2);      // call rax
        if (nops) {
            memset(loc+12, '\x90', nops);  // nop ;one of more nops for padding
        }
        log_(L"hook_call: hooked at %p (target: %p)\n", loc, p);
    }
}

void hook_call_rcx(BYTE *loc, BYTE *p, size_t nops) {
    if (!loc) {
        return;
    }
    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect(loc, 16, newProtection, &protection)) {
        memcpy(loc, "\x48\xb9", 2);
        memcpy(loc+2, &p, sizeof(BYTE*));  // mov rcx,<target_addr>
        memcpy(loc+10, "\xff\xd1", 2);      // call rcx
        if (nops) {
            memset(loc+12, '\x90', nops);  // nop ;one of more nops for padding
        }
        log_(L"hook_call_rcx: hooked at %p (target: %p)\n", loc, p);
    }
}

void hook_call_with_tail(BYTE *loc, BYTE *p, BYTE *tail, size_t tail_size) {
    if (!loc) {
        return;
    }
    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect(loc, 16, newProtection, &protection)) {
        memcpy(loc, "\x48\xb8", 2);
        memcpy(loc+2, &p, sizeof(BYTE*));  // mov rax,<target_addr>
        memcpy(loc+10, "\xff\xd0", 2);      // call rax
        memcpy(loc+12, tail, tail_size);  // tail code
        log_(L"hook_call_with_tail: hooked at %p (target: %p)\n", loc, p);
    }
}

void hook_call_with_head_and_tail(BYTE *loc, BYTE *p, BYTE *head, size_t head_size, BYTE *tail, size_t tail_size) {
    if (!loc) {
        return;
    }
    DWORD protection = 0 ;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect(loc, 64, newProtection, &protection)) {
        memcpy(loc, head, head_size);   // head code
        memcpy(loc+head_size, "\x48\xb8", 2);
        memcpy(loc+head_size+2, &p, sizeof(BYTE*));  // mov rax,<target_addr>
        memcpy(loc+head_size+10, "\xff\xd0", 2);     // call rax
        memcpy(loc+head_size+12, tail, tail_size);   // tail rax
        log_(L"hook_call_with_head: hooked at %p (target: %p)\n", loc, p);
    }
}

static int sider_context_register(lua_State *L)
{
    const char *event_key = luaL_checkstring(L, 1);
    if (!lua_isfunction(L, 2)) {
        lua_pushstring(L, "second argument must be a function");
        return lua_error(L);
    }
    else if (strcmp(event_key, "trophy_rewrite")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_trophy_check = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "livecpk_make_key")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_lcpk_make_key = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "livecpk_get_filepath")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_lcpk_get_filepath = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "livecpk_rewrite")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_lcpk_rewrite = lua_gettop(_curr_m->L);
        _rewrite_count++;
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "set_teams")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_set_teams = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    /*
    else if (strcmp(event_key, "set_tournament_id")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_set_tid = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    */
    else if (strcmp(event_key, "set_match_time")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_set_match_time = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    /*
    else if (strcmp(event_key, "set_stadium_choice")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_set_stadium_choice = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    */
    else if (strcmp(event_key, "set_stadium")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_set_stadium = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "set_conditions")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_set_conditions = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "after_set_conditions")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_after_set_conditions = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    /*
    else if (strcmp(event_key, "set_stadium_for_replay")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_set_stadium_for_replay = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "set_conditions_for_replay")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_set_conditions_for_replay = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "after_set_conditions_for_replay")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_after_set_conditions_for_replay = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "get_ball_name")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_get_ball_name = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "get_stadium_name")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_get_stadium_name = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "enter_edit_mode")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_enter_edit_mode = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "exit_edit_mode")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_exit_edit_mode = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "enter_replay_gallery")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_enter_replay_gallery = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "exit_replay_gallery")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_exit_replay_gallery = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    */
    else {
        logu_("WARN: trying to register for unknown event: \"%s\"\n",
            event_key);
    }
    lua_pop(L, 2);
    return 0;
}


static void push_context_table(lua_State *L)
{
    lua_newtable(L);

    char *sdir = (char*)Utf8::unicodeToUtf8(sider_dir);
    lua_pushstring(L, sdir);
    Utf8::free(sdir);
    lua_setfield(L, -2, "sider_dir");

    lua_pushcfunction(L, sider_context_register);
    lua_setfield(L, -2, "register");
}

static void push_env_table(lua_State *L, const wchar_t *script_name)
{
    char *sandbox[] = {
        "assert", "table", "pairs", "ipairs",
        "string", "math", "tonumber", "tostring",
        "unpack", "error", "_VERSION", "type", "io",
    };

    lua_newtable(L);
    for (int i=0; i<sizeof(sandbox)/sizeof(char*); i++) {
        lua_pushstring(L, sandbox[i]);
        lua_getglobal(L, sandbox[i]);
        lua_settable(L, -3);
    }
    /* DISABLING FOR NOW, as this is a SECURITY issue
    // extra globals
    for (list<wstring>::iterator i = _config->_lua_extra_globals.begin();
            i != _config->_lua_extra_globals.end();
            i++) {
        char *name = (char*)Utf8::unicodeToUtf8(i->c_str());
        lua_pushstring(L, name);
        lua_getglobal(L, name);
        if (lua_isnil(L, -1)) {
            logu_("WARNING: Unknown Lua global: %s. Skipping it\n",
                name);
            lua_pop(L, 2);
        }
        else {
            lua_settable(L, -3);
        }
        Utf8::free(name);
    }
    */

    // stripped-down os library: with only time, clock, and date
    char *os_names[] = { "time", "clock", "date" };
    lua_newtable(L);
    lua_getglobal(L, "os");
    for (int i=0; i<sizeof(os_names)/sizeof(char*); i++) {
        lua_getfield(L, -1, os_names[i]);
        lua_setfield(L, -3, os_names[i]);
    }
    lua_pop(L, 1);
    lua_setfield(L, -2, "os");

    lua_pushstring(L, "log");
    lua_pushvalue(L, -2);  // upvalue for sider_log C-function
    lua_pushcclosure(L, sider_log, 1);
    lua_settable(L, -3);
    lua_pushstring(L, "_FILE");
    char *sname = (char*)Utf8::unicodeToUtf8(script_name);
    lua_pushstring(L, sname);
    Utf8::free(sname);
    lua_settable(L, -3);

    // memory lib
    lua_pushvalue(L, 2);
    lua_setfield(L, -2, "memory");

    /*
    // gameplay lib
    init_gameplay_lib(L);

    // gfx lib
    init_gfx_lib(L);
    */

    // set _G
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "_G");

    // load some LuaJIT extenstions
    if (_config->_luajit_extensions_enabled) {
        char *ext[] = { "ffi", "bit" };
        for (int i=0; i<sizeof(ext)/sizeof(char*); i++) {
            lua_getglobal(L, "require");
            lua_pushstring(L, ext[i]);
            if (lua_pcall(L, 1, 1, 0) != 0) {
                const char *err = luaL_checkstring(L, -1);
                logu_("Problem loading LuaJIT module (%s): %s\n. "
                      "Skipping it.\n", ext[i], err);
                lua_pop(L, 1);
                continue;
            }
            else {
                lua_setfield(L, -2, ext[i]);
            }
        }
    }
}

void init_lua_support()
{
    if (_config->_lua_enabled) {
        log_(L"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
        log_(L"Initializing Lua module system:\n");

        // load and initialize lua modules
        L = luaL_newstate();
        luaL_openlibs(L);

        // prepare context table
        push_context_table(L);

        // memory library
        init_memlib(L);

        // load registered modules
        for (list<wstring>::iterator it = _config->_module_names.begin();
                it != _config->_module_names.end();
                it++) {
            log_(L"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

            // Use Win32 API to read the script into a buffer:
            // we do not want any nasty surprises with filename encodings
            wstring script_file(sider_dir);
            script_file += L"modules\\";
            script_file += it->c_str();

            log_(L"Loading module: %s ...\n", it->c_str());

            DWORD size = 0;
            HANDLE handle;
            handle = CreateFileW(
                script_file.c_str(),   // file to open
                GENERIC_READ,          // open for reading
                FILE_SHARE_READ,       // share for reading
                NULL,                  // default security
                OPEN_EXISTING,         // existing file only
                FILE_ATTRIBUTE_NORMAL, // normal file
                NULL);                 // no attr. template

            if (handle == INVALID_HANDLE_VALUE)
            {
                log_(L"PROBLEM: Unable to open file: %s\n",
                    script_file.c_str());
                continue;
            }

            size = GetFileSize(handle, NULL);
            BYTE *buf = new BYTE[size+1];
            memset(buf, 0, size+1);
            DWORD bytesRead = 0;
            if (!ReadFile(handle, buf, size, &bytesRead, NULL)) {
                log_(L"PROBLEM: ReadFile error for lua module: %s\n",
                    it->c_str());
                CloseHandle(handle);
                continue;
            }
            CloseHandle(handle);
            // script is now in memory

            char *mfilename = (char*)Utf8::unicodeToUtf8(it->c_str());
            string mfile(mfilename);
            Utf8::free(mfilename);
            int r = luaL_loadbuffer(L, (const char*)buf, size, mfile.c_str());
            delete buf;
            if (r != 0) {
                const char *err = lua_tostring(L, -1);
                logu_("Lua module loading problem: %s. "
                      "Skipping it\n", err);
                lua_pop(L, 1);
                continue;
            }

            // set environment
            push_env_table(L, it->c_str());
            lua_setfenv(L, -2);

            // run the module
            if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
                const char *err = lua_tostring(L, -1);
                logu_("Lua module initializing problem: %s. "
                      "Skipping it\n", err);
                lua_pop(L, 1);
                continue;
            }

            // check that module chunk is correctly constructed:
            // it must return a table
            if (!lua_istable(L, -1)) {
                logu_("PROBLEM: Lua module (%s) must return a table. "
                      "Skipping it\n", mfile.c_str());
                lua_pop(L, 1);
                continue;
            }

            // now we have module table on the stack
            // run its "init" method, with a context object
            lua_getfield(L, -1, "init");
            if (!lua_isfunction(L, -1)) {
                logu_("PROBLEM: Lua module (%s) does not "
                      "have \"init\" function. Skipping it.\n",
                      mfile.c_str());
                lua_pop(L, 1);
                continue;
            }

            module_t *m = new module_t();
            memset(m, 0, sizeof(module_t));
            m->cache = new lookup_cache_t();
            m->L = luaL_newstate();
            _curr_m = m;

            lua_pushvalue(L, 1); // ctx
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                const char *err = lua_tostring(L, -1);
                logu_("PROBLEM: Lua module (%s) \"init\" function "
                      "returned an error: %s\n", mfile.c_str(), err);
                logu_("Module (%s) is NOT activated\n", mfile.c_str());
                lua_pop(L, 1);
                // pop the module table too, since we are not using it
                lua_pop(L, 1);
            }
            else {
                logu_("OK: Lua module initialized: %s\n", mfile.c_str());
                //logu_("gettop: %d\n", lua_gettop(L));

                // add to list of loaded modules
                _modules.push_back(m);
            }
        }
        log_(L"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
        log_(L"Lua module system initialized.\n");
        log_(L"Active modules: %d\n", _modules.size());
        log_(L"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    }
}

bool _install_func(IMAGE_SECTION_HEADER *h);

DWORD install_func(LPVOID thread_param) {
    log_(L"DLL attaching to (%s).\n", module_filename);
    log_(L"Mapped into PES.\n");
    logu_("UTF-8 check: ленинградское время ноль часов ноль минут.\n");

    _is_game = true;
    _is_edit_mode = false;

    // initialize filename replacement trick
    strncpy(_file_to_lookup, KNOWN_FILENAME, sizeof(_file_to_lookup)-1);
    *(DWORD*)(_file_to_lookup + strlen(_file_to_lookup) + 1) = MAGIC;
    _file_to_lookup_size = strlen(_file_to_lookup) + 1 + 4 + 1;

    InitializeCriticalSection(&_cs);
    _key_cache = new cache_t(&_cs, _config->_key_cache_ttl_sec);
    _rewrite_cache = new cache_t(&_cs, _config->_rewrite_cache_ttl_sec);

    log_(L"debug = %d\n", _config->_debug);
    //if (_config->_game_speed) {
    //    log_(L"game.speed = %0.3f\n", *(_config->_game_speed));
    //}
    log_(L"livecpk.enabled = %d\n", _config->_livecpk_enabled);
    log_(L"lookup-cache.enabled = %d\n", _config->_lookup_cache_enabled);
    log_(L"lua.enabled = %d\n", _config->_lua_enabled);
    log_(L"luajit.ext.enabled = %d\n", _config->_luajit_extensions_enabled);
    //log_(L"address-cache.enabled = %d\n", (int)(!_config->_ac_off));
    log_(L"key-cache.ttl-sec = %d\n", _config->_key_cache_ttl_sec);
    log_(L"start.minimized = %d\n", _config->_start_minimized);
    log_(L"free.side.select = %d\n", _config->_free_side_select);
    log_(L"close.on.exit = %d\n", _config->_close_sider_on_exit);
    log_(L"match.minutes = %d\n", _config->_num_minutes);

    for (list<wstring>::iterator it = _config->_cpk_roots.begin();
            it != _config->_cpk_roots.end();
            it++) {
        log_(L"Using cpk.root: %s\n", it->c_str());
    }

    for (int i=0;; i++) {
        IMAGE_SECTION_HEADER *h = GetSectionHeaderByOrdinal(i);
        if (!h) {
            break;
        }

        char name[16];
        memset(name, 0, sizeof(name));
        memcpy(name, h->Name, 8);
        logu_("Examining code section: %s\n", name);
        if (h->Misc.VirtualSize < 0x10) {
            log_(L"Section too small: %s (%p). Skipping\n", name, h->Misc.VirtualSize);
            continue;
        }

        if (_install_func(h)) {
            init_lua_support();
            break;
        }
    }
    log_(L"Sider initialization complete.\n");
    return 0;
}

bool all_found(config_t *cfg) {
    bool all(true);
    if (cfg->_livecpk_enabled) {
        all = all && (
            cfg->_hp_at_read_file > 0 &&
            cfg->_hp_at_get_size > 0 &&
            cfg->_hp_at_extend_cpk > 0 &&
            cfg->_hp_at_mem_copy > 0 &&
            cfg->_hp_at_lookup_file > 0
        );
    }
    if (cfg->_lua_enabled) {
        all = all && (
            cfg->_hp_at_set_team_id > 0 &&
            cfg->_hp_at_set_settings > 0 &&
            cfg->_hp_at_trophy_check > 0 &&
            cfg->_hp_at_context_reset > 0
        );
    }
    if (cfg->_num_minutes > 0) {
        all = all && (
            cfg->_hp_at_set_min_time > 0 &&
            cfg->_hp_at_set_max_time > 0 &&
            cfg->_hp_at_set_minutes > 0
        );
    }
    if (cfg->_free_side_select) {
        all = all && (
            cfg->_hp_at_sider > 0
        );
    }
    return all;
}

bool _install_func(IMAGE_SECTION_HEADER *h) {
    BYTE* base = (BYTE*)GetModuleHandle(NULL);
    base += h->VirtualAddress;
    log_(L"Searching range: %p : %p (size: %p)\n",
        base, base + h->Misc.VirtualSize, h->Misc.VirtualSize);
    bool result(false);

#define NUM_PATTERNS 13
    BYTE *frag[NUM_PATTERNS];
    frag[0] = lcpk_pattern_at_read_file;
    frag[1] = lcpk_pattern_at_get_size;
    frag[2] = lcpk_pattern_at_write_cpk_filesize;
    frag[3] = lcpk_pattern_at_mem_copy;
    frag[4] = lcpk_pattern_at_lookup_file;
    frag[5] = pattern_set_team_id;
    frag[6] = pattern_set_settings;
    frag[7] = pattern_trophy_check;
    frag[8] = pattern_context_reset;
    frag[9] = pattern_set_min_time;
    frag[10] = pattern_set_max_time;
    frag[11] = pattern_set_minutes;
    frag[12] = pattern_sider;
    size_t frag_len[NUM_PATTERNS];
    frag_len[0] = _config->_livecpk_enabled ? sizeof(lcpk_pattern_at_read_file)-1 : 0;
    frag_len[1] = _config->_livecpk_enabled ? sizeof(lcpk_pattern_at_get_size)-1 : 0;
    frag_len[2] = _config->_livecpk_enabled ? sizeof(lcpk_pattern_at_write_cpk_filesize)-1 : 0;
    frag_len[3] = _config->_livecpk_enabled ? sizeof(lcpk_pattern_at_mem_copy)-1 : 0;
    frag_len[4] = _config->_livecpk_enabled ? sizeof(lcpk_pattern_at_lookup_file)-1 : 0;
    frag_len[5] = _config->_lua_enabled ? sizeof(pattern_set_team_id)-1 : 0;
    frag_len[6] = _config->_lua_enabled ? sizeof(pattern_set_settings)-1 : 0;
    frag_len[7] = _config->_lua_enabled ? sizeof(pattern_trophy_check)-1 : 0;
    frag_len[8] = _config->_lua_enabled ? sizeof(pattern_context_reset)-1 : 0;
    frag_len[9] = (_config->_num_minutes > 0) ? sizeof(pattern_set_min_time)-1 : 0;
    frag_len[10] = (_config->_num_minutes > 0) ? sizeof(pattern_set_max_time)-1 : 0;
    frag_len[11] = (_config->_num_minutes > 0) ? sizeof(pattern_set_minutes)-1 : 0;
    frag_len[12] = _config->_free_side_select ? sizeof(pattern_sider)-1 : 0;
    int offs[NUM_PATTERNS];
    offs[0] = lcpk_offs_at_read_file;
    offs[1] = lcpk_offs_at_get_size;
    offs[2] = lcpk_offs_at_write_cpk_filesize;
    offs[3] = lcpk_offs_at_mem_copy;
    offs[4] = lcpk_offs_at_lookup_file;
    offs[5] = offs_set_team_id;
    offs[6] = offs_set_settings;
    offs[7] = offs_trophy_check;
    offs[8] = offs_context_reset;
    offs[9] = offs_set_min_time;
    offs[10] = offs_set_max_time;
    offs[11] = offs_set_minutes;
    offs[12] = offs_sider;
    BYTE **addrs[NUM_PATTERNS];
    addrs[0] = &_config->_hp_at_read_file;
    addrs[1] = &_config->_hp_at_get_size;
    addrs[2] = &_config->_hp_at_extend_cpk;
    addrs[3] = &_config->_hp_at_mem_copy;
    addrs[4] = &_config->_hp_at_lookup_file;
    addrs[5] = &_config->_hp_at_set_team_id;
    addrs[6] = &_config->_hp_at_set_settings;
    addrs[7] = &_config->_hp_at_trophy_check;
    addrs[8] = &_config->_hp_at_context_reset;
    addrs[9] = &_config->_hp_at_set_min_time;
    addrs[10] = &_config->_hp_at_set_max_time;
    addrs[11] = &_config->_hp_at_set_minutes;
    addrs[12] = &_config->_hp_at_sider;

    for (int j=0; j<NUM_PATTERNS; j++) {
        if (frag_len[j]==0) {
            // empty frag
            continue;
        }
        if (*(addrs[j])) {
            // already found
            continue;
        }
        BYTE *p = find_code_frag(base, h->Misc.VirtualSize,
            frag[j], frag_len[j]);
        if (!p) {
            continue;
        }
        log_(L"Found pattern %i of %i\n", j+1, NUM_PATTERNS);
        *(addrs[j]) = p + offs[j];
    }

    if (all_found(_config)) {
        result = true;

        // hooks
        if (_config->_livecpk_enabled) {
            log_(L"sider_read_file_hk: %p\n", sider_read_file_hk);
            log_(L"sider_get_size_hk: %p\n", sider_get_size_hk);
            log_(L"sider_extend_cpk_hk: %p\n", sider_extend_cpk_hk);
            log_(L"sider_mem_copy_hk: %p\n", sider_mem_copy_hk);
            log_(L"sider_lookup_file: %p\n", sider_lookup_file_hk);

            hook_indirect_call(_config->_hp_at_read_file, (BYTE*)sider_read_file_hk);
            hook_call(_config->_hp_at_get_size, (BYTE*)sider_get_size_hk, 0);
            hook_call(_config->_hp_at_extend_cpk, (BYTE*)sider_extend_cpk_hk, 1);
            hook_call(_config->_hp_at_mem_copy, (BYTE*)sider_mem_copy_hk, 0);
            hook_call_rcx(_config->_hp_at_lookup_file, (BYTE*)sider_lookup_file_hk, 3);
        }

        if (_config->_lua_enabled) {
            log_(L"sider_set_team_id: %p\n", sider_set_team_id_hk);
            log_(L"sider_set_settings: %p\n", sider_set_settings_hk);
            log_(L"sider_trophy_check: %p\n", sider_trophy_check_hk);
            log_(L"sider_context_reset: %p\n", sider_context_reset_hk);

            hook_call_with_tail(_config->_hp_at_set_team_id, (BYTE*)sider_set_team_id_hk,
                (BYTE*)pattern_set_team_id_tail, sizeof(pattern_set_team_id_tail)-1);
            hook_call(_config->_hp_at_set_settings, (BYTE*)sider_set_settings_hk, 0);
            hook_call_with_head_and_tail(_config->_hp_at_trophy_check, (BYTE*)sider_trophy_check_hk,
                (BYTE*)pattern_trophy_check_head, sizeof(pattern_trophy_check_head)-1,
                (BYTE*)pattern_trophy_check_tail, sizeof(pattern_trophy_check_tail)-1);
            hook_call(_config->_hp_at_context_reset, (BYTE*)sider_context_reset_hk, 6);
        }

        if (_config->_free_side_select) {
            hook_call_rcx(_config->_hp_at_sider, (BYTE*)sider_free_select_hk, 0);
        }

        if (_config->_num_minutes != 0) {
            patch_at_location(_config->_hp_at_set_min_time, "\x90\x90\x90\x90\x90\x90\x90", 7);
            patch_at_location(_config->_hp_at_set_max_time, "\x90\x90\x90\x90\x90\x90\x90", 7);

            if (_config->_num_minutes < 1) {
                _config->_num_minutes = 1;
            }
            else if (_config->_num_minutes > 255) {
                _config->_num_minutes = 255;
            }
            logu_("Setting match minutes to: %d\n", _config->_num_minutes);
            patch_set_minutes[3] = _config->_num_minutes;
            patch_at_location(_config->_hp_at_set_minutes, patch_set_minutes, sizeof(patch_set_minutes)-1);
        }
    }

    return result;
}

INT APIENTRY DllMain(HMODULE hDLL, DWORD Reason, LPVOID Reserved)
{
    wstring *match = NULL;
    INT result = FALSE;
    HWND main_hwnd;

    switch(Reason) {
        case DLL_PROCESS_ATTACH:
            myHDLL = hDLL;
            memset(module_filename, 0, sizeof(module_filename));
            if (GetModuleFileName(NULL, module_filename, MAX_PATH)==0) {
                return FALSE;
            }
            if (!init_paths()) {
                return FALSE;
            }
            //log_(L"DLL_PROCESS_ATTACH: %s\n", module_filename);
            if (skip_process(module_filename)) {
                return FALSE;
            }

            if (is_sider(module_filename)) {
                _is_sider = true;
                read_configuration(_config);
                if (!write_mapping_info(_config)) {
                    return FALSE;
                }
                return TRUE;
            }

            if (is_pes(module_filename, &match)) {
                read_configuration(_config);

                wstring version;
                get_module_version(hDLL, version);
                open_log_(L"============================\n");
                log_(L"Sider DLL: version %s\n", version.c_str());
                log_(L"Filename match: %s\n", match->c_str());

                install_func(NULL);

                delete match;
                return TRUE;
            }

            return result;
            break;

        case DLL_PROCESS_DETACH:
            //log_(L"DLL_PROCESS_DETACH: %s\n", module_filename);

            if (_is_sider) {
                UnmapViewOfFile(_mh);
                CloseHandle(_mh);
            }

            if (_is_game) {
                if (_key_cache) { delete _key_cache; }
                if (_rewrite_cache) { delete _rewrite_cache; }
                log_(L"DLL detaching from (%s).\n", module_filename);
                log_(L"Unmapping from PES.\n");

                if (L) { lua_close(L); }
                DeleteCriticalSection(&_cs);

                // tell sider.exe to close
                if (_config->_close_sider_on_exit) {
                    main_hwnd = FindWindow(SIDERCLS, NULL);
                    if (main_hwnd) {
                        PostMessage(main_hwnd, SIDER_MSG_EXIT, 0, 0);
                        log_(L"Posted message for sider.exe to quit\n");
                    }
                }
                close_log_();
            }
            break;

        case DLL_THREAD_ATTACH:
            //log_(L"DLL_THREAD_ATTACH: %s\n", module_filename);
            break;

        case DLL_THREAD_DETACH:
            //log_(L"DLL_THREAD_DETACH: %s\n", module_filename);
            break;

    }

    return TRUE;
}

LRESULT CALLBACK meconnect(int code, WPARAM wParam, LPARAM lParam)
{
    if (hookingThreadId == GetCurrentThreadId()) {
        log_(L"called in hooking thread!\n");
    }
    return CallNextHookEx(handle, code, wParam, lParam);
}

void setHook()
{
    handle = SetWindowsHookEx(WH_CBT, meconnect, myHDLL, 0);
    log_(L"------------------------\n");
    log_(L"handle = %p\n", handle);
}

void unsetHook()
{
    UnhookWindowsHookEx(handle);
}
