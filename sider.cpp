#define UNICODE

//#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <list>
#include <string>
#include <unordered_map>
#include "zlib.h"
#include "imageutil.h"
#include "sider.h"
#include "utf8.h"
#include "common.h"
#include "patterns.h"
#include "memlib.h"
#include "libz.h"

#define DIRECTINPUT_VERSION 0x0800
#define SAFE_RELEASE(x) if (x) { x->Release(); x = NULL; }

#include "d3d11.h"
#include "d3dcompiler.h"
#include "FW1FontWrapper.h"
#include "dinput.h"
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"

#include "lua.hpp"
#include "lauxlib.h"
#include "lualib.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

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
CRITICAL_SECTION _tcs;
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

struct STAD_INFO_STRUCT {
    DWORD unknown0;
    WORD id;
    WORD unknown1;
    char name[0xac];
};

struct TROPHY_TABLE_ENTRY {
    WORD tournament_id;
    WORD dw0;
    DWORD trophy_id;
};

#define TT_LEN 0x14b
TROPHY_TABLE_ENTRY _trophy_table[TT_LEN];
typedef unordered_map<WORD,DWORD> trophy_map_t;
trophy_map_t *_trophy_map;

WORD _tournament_id = 0xffff;
char _ball_name[256];
char _stadium_name[256];
int _stadium_choice_count = 0;
struct STAD_INFO_STRUCT _stadium_info;

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
void lua_reload_modified_modules();
static void push_env_table(lua_State *L, const wchar_t *script_name);

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

LRESULT CALLBACK sider_keyboard_proc(int code, WPARAM wParam, LPARAM lParam);

typedef HRESULT (*PFN_CreateDXGIFactory1)(REFIID riid, void **ppFactory);
typedef HRESULT (*PFN_IDXGIFactory1_CreateSwapChain)(IDXGIFactory1 *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain);
typedef HRESULT (*PFN_IDXGISwapChain_Present)(IDXGISwapChain *swapChain, UINT SyncInterval, UINT Flags);
PFN_CreateDXGIFactory1 _org_CreateDXGIFactory1;
PFN_IDXGIFactory1_CreateSwapChain _org_CreateSwapChain;
PFN_IDXGISwapChain_Present _org_Present;

HRESULT sider_CreateDXGIFactory1(REFIID riid, void **ppFactory);
HRESULT sider_CreateSwapChain(IDXGIFactory1 *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain);
HRESULT sider_Present(IDXGISwapChain *swapChain, UINT SyncInterval, UINT Flags);
BOOL sider_device_enum_callback(LPCDIDEVICEINSTANCE lppdi, LPVOID pvRef);
BOOL sider_device_enum_callback(LPCDIDEVICEINSTANCE lppdi, LPVOID pvRef);
BOOL sider_object_enum_callback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef);
void init_direct_input();

ID3D11Device *_device(NULL);
ID3D11DeviceContext *_device_context(NULL);
IDXGISwapChain *_swap_chain(NULL);
ID3D11Resource *g_texture(NULL);
ID3D11ShaderResourceView *g_textureView(NULL);
ID3D11SamplerState *g_pSamplerLinear(NULL);

struct overlay_image_t {
    bool to_clear;
    bool have;
    int source_width;
    int source_height;
    int width;
    int height;
    int vmargin;
    int hmargin;
    char *filepath;
};

struct layout_t {
    int image_width;
    int image_height;
    bool has_image_ar;
    bool has_image_hmargin;
    bool has_image_vmargin;
    float image_aspect_ratio;
    float image_hmargin;
    float image_vmargin;
};

overlay_image_t _overlay_image;

struct dx11_t {
    ID3D11Device *Device;
    ID3D11DeviceContext *Context;
    IDXGISwapChain *SwapChain;
    HWND Window;
    UINT Width;
    UINT Height;
};
dx11_t DX11;

IDirectInput8 *g_IDirectInput8;
IDirectInputDevice8 *g_IDirectInputDevice8;
GUID g_controller_guid_instance;
bool _has_controller(false);
bool _controller_prepped(false);
bool _controller_poll(false);
int _controller_poll_delay(0);
HANDLE _controller_poll_handle(INVALID_HANDLE_VALUE);

DIDATAFORMAT _data_format;
BYTE _prev_controller_buttons[64];
BYTE _controller_buttons[64];
list<DIDEVICEOBJECTINSTANCE> _di_objects;

struct di_input_t {
    DWORD dwOfs;
    DWORD dwType;
};

di_input_t _di_overlay_toggle1;
di_input_t _di_overlay_toggle2;
di_input_t _di_module_switch;

struct di_change_t {
    int state;
    DWORD dwType;
};

di_change_t *_di_changes;
size_t _di_changes_len = 0;

IFW1Factory *g_pFW1Factory;
IFW1FontWrapper *g_pFontWrapper;
float _font_size = 20.0f;

struct SimpleVertex {
    float x;
    float y;
    float z;
};

struct TexturedVertex {
    float x;
    float y;
    float z;
    float w;
    float tx;
    float ty;
    float tz;
    float tw;
};

SimpleVertex g_vertices[] =
{
    -1.0f, 1.0f, 0.5f,
    1.0f, 1.0f, 0.5f,
    1.0f, -1.0f, 0.5f,
    1.0f, -1.0f, 0.5f,
    -1.0f, -1.0f, 0.5f,
    -1.0f, 1.0f, 0.5f,
};

static const TexturedVertex g_texVertices[] =
{
    { -1.f, 1.f, 0.4f, 1.f, 0.f, 0.f, 0.f, 0.f },
    { 1.f, 1.f, 0.4f, 1.f, 1.f, 0.f, 0.f, 0.f },
    { 1.f, -1.f, 0.4f, 1.f, 1.f, 1.f, 0.f, 0.f },
    { 1.f, -1.f, 0.4f, 1.f, 1.f, 1.f, 0.f, 0.f },
    { -1.f, -1.f, 0.4f, 1.f, 0.f, 1.f, 0.f, 0.f },
    { -1.f, 1.f, 0.4f, 1.f, 0.f, 0.f, 0.f, 0.f },
};

ID3D11BlendState* g_pBlendState = NULL;
ID3D11InputLayout*          g_pInputLayout = NULL;
ID3D11InputLayout*          g_pTexInputLayout = NULL;
ID3D11Buffer*               g_pVertexBuffer = NULL;
ID3D11Buffer*               g_pTexVertexBuffer = NULL;
ID3D11RenderTargetView*     g_pRenderTargetView = NULL;
ID3D11VertexShader*         g_pVertexShader = NULL;
ID3D11VertexShader*         g_pTexVertexShader = NULL;
ID3D11PixelShader*          g_pPixelShader = NULL;
ID3D11PixelShader*          g_pTexPixelShader = NULL;
HRESULT hr = S_OK;

char* g_strVS =
    "void VS( in float4 posIn : POSITION,\n"
    "         out float4 posOut : SV_Position )\n"
    "{\n"
    "    // Output the vertex position, unchanged\n"
    "    posOut = posIn;\n"
    "}\n";

char* g_strTexVS =
    "struct VS_INPUT\n"
    "{\n"
    "    float4 Pos : POSITION;\n"
    "    float4 Tex : TEXCOORD0;\n"
    "};\n"
    "struct PS_INPUT\n"
    "{\n"
    "    float4 Pos : SV_POSITION;\n"
    "    float4 Tex : TEXCOORD0;\n"
    "};\n"
    "PS_INPUT VStex( VS_INPUT input )\n"
    "{\n"
    "    PS_INPUT output = (PS_INPUT)0;\n"
    "    output.Pos = input.Pos;\n"
    "    output.Tex = input.Tex;\n"
    "    return output;\n"
    "}\n";

char* g_strPS =
    "void PS( out float4 colorOut : SV_Target )\n"
    "{\n"
    "    colorOut = float4( %0.2f, %0.2f, %0.2f, %0.2f );\n"
    "}\n";

char *g_strTexPS =
    "Texture2D tx2D : register( t0 );\n"
    "SamplerState samLinear : register( s0 );\n"
    "\n"
    "struct PS_INPUT\n"
    "{\n"
    "    float4 Pos : SV_POSITION;\n"
    "    float4 Tex : TEXCOORD0;\n"
    "};\n"
    "\n"
    "float4 PStex( PS_INPUT input) : SV_Target\n"
    "{\n"
    "    float4 clr = tx2D.Sample( samLinear, input.Tex.xy );\n"
    "    clr.a = min(%0.3f, clr.a);\n"
    "    return clr;\n"
    "    //return float4(input.Tex.x,input.Tex.y,0.0f,0.4f);\n"
    "    //return float4(1.0f,0.0f,0.0f,0.4f);\n"
    "}\n";

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

extern "C" void sider_trophy_table(TROPHY_TABLE_ENTRY *tt);

extern "C" void sider_trophy_table_hk();

extern "C" char* sider_ball_name(char *ball_name);

extern "C" void sider_ball_name_hk();

extern "C" char* sider_stadium_name(STAD_INFO_STRUCT *stad_info);

extern "C" void sider_stadium_name_hk();

extern "C" STAD_INFO_STRUCT* sider_def_stadium_name(DWORD stadium_id);

extern "C" void sider_def_stadium_name_hk();

extern "C" void sider_set_stadium_choice(MATCH_INFO_STRUCT *mi, BYTE stadium_id);

extern "C" void sider_set_stadium_choice_hk();

static DWORD dwThreadId;
static DWORD hookingThreadId = 0;
static HMODULE myHDLL;
static HHOOK handle = 0;
static HHOOK kb_handle = 0;

bool _overlay_on(false);
bool _reload_1_down(false);
bool _reload_modified(false);
bool _is_game(false);
bool _is_sider(false);
bool _is_edit_mode(false);
bool _priority_set(false);
HANDLE _mh = NULL;

wstring _overlay_header;
wchar_t _overlay_text[4096];
wchar_t _current_overlay_text[4096] = L"hello world!";
char _overlay_utf8_text[4096];
char _overlay_utf8_image_path[2048];

#define DEFAULT_OVERLAY_TEXT_COLOR 0xc080ff80
#define DEFAULT_OVERLAY_BACKGROUND_COLOR 0x80102010
#define DEFAULT_OVERLAY_IMAGE_ALPHA_MAX 1.0f
#define DEFAULT_OVERLAY_FONT L"Arial"
#define DEFAULT_OVERLAY_FONT_SIZE 0
#define DEFAULT_OVERLAY_LOCATION 0
#define DEFAULT_OVERLAY_VKEY_TOGGLE 0x20
#define DEFAULT_OVERLAY_VKEY_NEXT_MODULE 0x31
#define DEFAULT_OVERLAY_GAMEPAD_POLL_INTERVAL_MSEC 50
#define DEFAULT_VKEY_RELOAD_1 0x10 //Shift
#define DEFAULT_VKEY_RELOAD_2 0x52 //R
#define DEFAULT_GAMEPAD_POLL_INTERVAL_MSEC 200

wchar_t module_filename[MAX_PATH];
wchar_t dll_log[MAX_PATH];
wchar_t dll_ini[MAX_PATH];
wchar_t gamepad_ini[MAX_PATH];
wchar_t sider_dir[MAX_PATH];

static void string_strip_quotes(wstring& s)
{
    static const wchar_t* chars = L" \t\n\r\"'";
    int e = s.find_last_not_of(chars);
    s.erase(e + 1);
    int b = s.find_first_not_of(chars);
    s.erase(0,b);
}

class lock_t {
public:
    CRITICAL_SECTION *_cs;
    lock_t(CRITICAL_SECTION *cs) : _cs(cs) {
        EnterCriticalSection(_cs);
    }
    ~lock_t() {
        LeaveCriticalSection(_cs);
    }
};

struct gamepad_input_mapping_t {
    DWORD dwType;
    int value;
    BYTE vkey;
};

class gamepad_config_t {
public:
    wstring _section_name;
    unordered_map<DWORD,gamepad_input_mapping_t> _map;

    ~gamepad_config_t() {}
    gamepad_config_t(const wstring& section_name, const wchar_t* gamepad_ini) :
            _section_name(section_name)
    {
        wchar_t settings[32767];
        RtlZeroMemory(settings, sizeof(settings));
        if (GetPrivateProfileSection(_section_name.c_str(),
            settings, sizeof(settings)/sizeof(wchar_t), gamepad_ini)==0) {
            // no ini-file, or no "[gamepad]" section
            return;
        }

        wchar_t* p = settings;
        while (*p) {
            wstring pair(p);
            wstring key(pair.substr(0, pair.find(L"=")));
            wstring value(pair.substr(pair.find(L"=")+1));
            string_strip_quotes(value);

            if (wcscmp(L"gamepad.input.mapping", key.c_str())==0) {
                gamepad_input_mapping_t gim;
                if (swscanf(value.c_str(), L"%x,%d,%x", &gim.dwType, &gim.value, &gim.vkey)==3) {
                    short trimmed_value = (short)gim.value;
                    DWORD key = (trimmed_value & 0xffff) | (gim.dwType << 16);
                    _map.insert(std::pair<DWORD,gamepad_input_mapping_t>(key,gim));
                }
            }

            p += wcslen(p) + 1;
        }
    }
    bool lookup(DWORD dwType, int value, BYTE* vkey) {
        short trimmed_value = (short)value;
        DWORD key = (trimmed_value & 0xffff) | (dwType << 16);
        unordered_map<DWORD,gamepad_input_mapping_t>::iterator it;
        it = _map.find(key);
        if (it != _map.end()) {
            *vkey = it->second.vkey;
            return true;
        }
        return false;
    }
};

class config_t {
public:
    int _debug;
    int _priority_class;
    bool _livecpk_enabled;
    bool _lookup_cache_enabled;
    bool _lua_enabled;
    bool _luajit_extensions_enabled;
    list<wstring> _lua_extra_globals;
    int _lua_gc_opt;
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
    bool _overlay_enabled;
    bool _overlay_on_from_start;
    bool _overlay_controlled_by_gamepad;
    float _overlay_gamepad_poll_interval_msec;
    float _gamepad_poll_interval_msec;
    wstring _overlay_font;
    DWORD _overlay_text_color;
    DWORD _overlay_background_color;
    float _overlay_image_alpha_max;
    int _overlay_location;
    int _overlay_font_size;
    int _overlay_vkey_toggle;
    int _overlay_vkey_next_module;
    int _vkey_reload_1;
    int _vkey_reload_2;
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
    BYTE *_hp_at_trophy_table;
    BYTE *_hp_at_ball_name;
    BYTE *_hp_at_stadium_name;
    BYTE *_hp_at_def_stadium_name;
    BYTE *_hp_at_set_stadium_choice;

    BYTE *_hp_at_set_min_time;
    BYTE *_hp_at_set_max_time;
    BYTE *_hp_at_set_minutes;
    BYTE *_hp_at_sider;

    BYTE *_hp_at_dxgi;

    bool _hook_set_team_id;
    bool _hook_set_settings;
    bool _hook_context_reset;
    bool _hook_trophy_check;
    bool _hook_trophy_table;

    ~config_t() {}
    config_t(const wstring& section_name, const wchar_t* config_ini) :
                 _section_name(section_name),
                 _debug(0),
                 _priority_class(0),
                 _livecpk_enabled(false),
                 _lookup_cache_enabled(true),
                 _lua_enabled(true),
                 _luajit_extensions_enabled(false),
                 _lua_gc_opt(LUA_GCSTEP),
                 _close_sider_on_exit(false),
                 _start_minimized(false),
                 _free_side_select(false),
                 _overlay_enabled(false),
                 _overlay_on_from_start(false),
                 _overlay_controlled_by_gamepad(false),
                 _overlay_font(DEFAULT_OVERLAY_FONT),
                 _overlay_text_color(DEFAULT_OVERLAY_TEXT_COLOR),
                 _overlay_background_color(DEFAULT_OVERLAY_BACKGROUND_COLOR),
                 _overlay_image_alpha_max(DEFAULT_OVERLAY_IMAGE_ALPHA_MAX),
                 _overlay_font_size(DEFAULT_OVERLAY_FONT_SIZE),
                 _overlay_location(DEFAULT_OVERLAY_LOCATION),
                 _overlay_vkey_toggle(DEFAULT_OVERLAY_VKEY_TOGGLE),
                 _overlay_vkey_next_module(DEFAULT_OVERLAY_VKEY_NEXT_MODULE),
                 _overlay_gamepad_poll_interval_msec(DEFAULT_OVERLAY_GAMEPAD_POLL_INTERVAL_MSEC),
                 _gamepad_poll_interval_msec(DEFAULT_GAMEPAD_POLL_INTERVAL_MSEC),
                 _vkey_reload_1(DEFAULT_VKEY_RELOAD_1),
                 _vkey_reload_2(DEFAULT_VKEY_RELOAD_2),
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
                 _hp_at_set_stadium_choice(NULL),
                 _hp_at_set_min_time(NULL),
                 _hp_at_set_max_time(NULL),
                 _hp_at_set_minutes(NULL),
                 _hp_at_sider(NULL),
                 _hp_at_trophy_table(NULL),
                 _hp_at_ball_name(NULL),
                 _hp_at_stadium_name(NULL),
                 _hp_at_def_stadium_name(NULL),
                 _hp_at_dxgi(NULL),
                 _hook_set_team_id(true),
                 _hook_set_settings(true),
                 _hook_context_reset(true),
                 _hook_trophy_check(true),
                 _hook_trophy_table(true),
                 _num_minutes(0)
    {
        wchar_t settings[32767];
        RtlZeroMemory(settings, sizeof(settings));
        if (GetPrivateProfileSection(_section_name.c_str(),
            settings, sizeof(settings)/sizeof(wchar_t), config_ini)==0) {
            // no ini-file, or no "[sider]" section
            return;
        }

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
            else if (wcscmp(L"overlay.font", key.c_str())==0) {
                _overlay_font = value;
            }
            else if (wcscmp(L"overlay.text-color", key.c_str())==0) {
                if (value.size() >= 8) {
                    DWORD c,v;
                    // red
                    if (swscanf(value.substr(0,2).c_str(), L"%x", &c)==1) { v = c; }
                    else if (swscanf(value.substr(0,2).c_str(), L"%X", &c)==1) { v = c; }
                    // green
                    if (swscanf(value.substr(2,2).c_str(), L"%x", &c)==1) { v = v | (c << 8); }
                    else if (swscanf(value.substr(2,2).c_str(), L"%X", &c)==1) { v = v | (c << 8); }
                    // blue
                    if (swscanf(value.substr(4,2).c_str(), L"%x", &c)==1) { v = v | (c << 16); }
                    else if (swscanf(value.substr(4,2).c_str(), L"%X", &c)==1) { v = v | (c << 16); }
                    // alpha
                    if (swscanf(value.substr(6,2).c_str(), L"%x", &c)==1) { v = v | (c << 24); }
                    else if (swscanf(value.substr(6,2).c_str(), L"%X", &c)==1) { v = v | (c << 24); }
                    _overlay_text_color = v;
                }
            }
            else if (wcscmp(L"overlay.background-color", key.c_str())==0) {
                if (value.size() >= 8) {
                    DWORD c,v;
                    // red
                    if (swscanf(value.substr(0,2).c_str(), L"%x", &c)==1) { v = c; }
                    else if (swscanf(value.substr(0,2).c_str(), L"%X", &c)==1) { v = c; }
                    // green
                    if (swscanf(value.substr(2,2).c_str(), L"%x", &c)==1) { v = v | (c << 8); }
                    else if (swscanf(value.substr(2,2).c_str(), L"%X", &c)==1) { v = v | (c << 8); }
                    // blue
                    if (swscanf(value.substr(4,2).c_str(), L"%x", &c)==1) { v = v | (c << 16); }
                    else if (swscanf(value.substr(4,2).c_str(), L"%X", &c)==1) { v = v | (c << 16); }
                    // alpha
                    if (swscanf(value.substr(6,2).c_str(), L"%x", &c)==1) { v = v | (c << 24); }
                    else if (swscanf(value.substr(6,2).c_str(), L"%X", &c)==1) { v = v | (c << 24); }
                    _overlay_background_color = v;
                }
            }
            else if (wcscmp(L"overlay.image-alpha-max", key.c_str())==0) {
                float alpha = 1.0f;
                if (swscanf(value.c_str(),L"%f",&alpha)==1) {
                    _overlay_image_alpha_max = min(1.0f, max(0.0f, alpha));
                }
            }
            else if (wcscmp(L"overlay.location", key.c_str())==0) {
                _overlay_location = 0;
                if (value == L"bottom") {
                    _overlay_location = 1;
                }
            }
            else if (wcscmp(L"overlay.vkey.toggle", key.c_str())==0) {
                int v;
                if (swscanf(value.c_str(), L"%x", &v)==1) {
                    _overlay_vkey_toggle = v;
                }
            }
            else if (wcscmp(L"overlay.vkey.next-module", key.c_str())==0) {
                int v;
                if (swscanf(value.c_str(), L"%x", &v)==1) {
                    _overlay_vkey_next_module = v;
                }
            }
            else if (wcscmp(L"vkey.reload-1", key.c_str())==0) {
                int v;
                if (swscanf(value.c_str(), L"%x", &v)==1) {
                    _vkey_reload_1 = v;
                }
            }
            else if (wcscmp(L"vkey.reload-2", key.c_str())==0) {
                int v;
                if (swscanf(value.c_str(), L"%x", &v)==1) {
                    _vkey_reload_2 = v;
                }
            }
            else if (wcscmp(L"game.priority.class", key.c_str())==0) {
                int v;
                if (value == L"above_normal") {
                    _priority_class = 0x8000;
                }
                else if (value == L"below_normal") {
                    _priority_class = 0x4000;
                }
                else if (value == L"high") {
                    _priority_class = 0x80;
                }
                else if (value == L"idle") {
                    _priority_class = 0x40;
                }
                else if (value == L"normal") {
                    _priority_class = 0x20;
                }
                else if (value == L"background_begin") {
                    _priority_class = 0x00100000;
                }
                else if (value == L"background_end") {
                    _priority_class = 0x00200000;
                }
                else if (value == L"realtime") {
                    _priority_class = 0x100;
                }
                else if (swscanf(value.c_str(), L"%x", &v)==1) {
                    _priority_class = v;
                }
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
            else if (wcscmp(L"lua.gc.opt", key.c_str())==0) {
                _lua_gc_opt = LUA_GCSTEP;
                if (value == L"collect") {
                    _lua_gc_opt = LUA_GCCOLLECT;
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

        _overlay_enabled = GetPrivateProfileInt(_section_name.c_str(),
            L"overlay.enabled", _overlay_enabled,
            config_ini);

        _overlay_on_from_start = GetPrivateProfileInt(_section_name.c_str(),
            L"overlay.on-from-start", _overlay_on_from_start,
            config_ini);

        _overlay_controlled_by_gamepad = GetPrivateProfileInt(_section_name.c_str(),
            L"overlay.gamepad.enabled", _overlay_controlled_by_gamepad,
            config_ini);

        _overlay_font_size = GetPrivateProfileInt(_section_name.c_str(),
            L"overlay.font-size", _overlay_font_size,
            config_ini);

        _overlay_gamepad_poll_interval_msec = GetPrivateProfileInt(_section_name.c_str(),
            L"overlay.gamepad.poll-interval-msec", _overlay_gamepad_poll_interval_msec,
            config_ini);

        _gamepad_poll_interval_msec = GetPrivateProfileInt(_section_name.c_str(),
            L"gamepad.poll-interval-msec", _gamepad_poll_interval_msec,
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

        _hook_set_team_id = GetPrivateProfileInt(_section_name.c_str(),
            L"hook.set-team-id", _hook_set_team_id,
            config_ini);

        _hook_set_settings = GetPrivateProfileInt(_section_name.c_str(),
            L"hook.set-settings", _hook_set_settings,
            config_ini);

        _hook_context_reset = GetPrivateProfileInt(_section_name.c_str(),
            L"hook.context-reset", _hook_context_reset,
            config_ini);

        _hook_trophy_table = GetPrivateProfileInt(_section_name.c_str(),
            L"hook.trophy-table", _hook_trophy_table,
            config_ini);

        _hook_trophy_check = GetPrivateProfileInt(_section_name.c_str(),
            L"hook.trophy-check", _hook_trophy_check,
            config_ini);
    }
};

config_t* _config;
gamepad_config_t* _gamepad_config;

typedef struct {
    void *value;
    uint64_t expires;
} cache_map_value_t;

/*
GetTickCount64 is pretty fast, almost as fast as GetTickCount, but does not
have a problem of wrap-around every 49 days. So we use it.

GetTickCount64:

00007FF818F261B0 | 8B 0C 25 04 00 FE 7F                 | mov ecx,dword ptr ds:[7FFE0004]  |
00007FF818F261B7 | B8 20 03 FE 7F                       | mov eax,7FFE0320                 |
00007FF818F261BC | 48 C1 E1 20                          | shl rcx,20                       |
00007FF818F261C0 | 48 8B 00                             | mov rax,qword ptr ds:[rax]       |
00007FF818F261C3 | 48 C1 E0 08                          | shl rax,8                        |
00007FF818F261C7 | 48 F7 E1                             | mul rcx                          |
00007FF818F261CA | 48 8B C2                             | mov rax,rdx                      |
00007FF818F261CD | C3                                   | ret                              |
*/

typedef unordered_map<string,cache_map_value_t> cache_map_t;

class cache_t {
    cache_map_t _map;
    uint64_t _ttl_msec;
    CRITICAL_SECTION *_kcs;
    int debug;
public:
    cache_t(CRITICAL_SECTION *cs, int ttl_sec) :
        _kcs(cs), _ttl_msec(ttl_sec * 1000) {
    }
    ~cache_t() {
        log_(L"cache: size:%d\n", _map.size());
    }
    bool lookup(char *filename, void **res) {
        lock_t lock(_kcs);
        cache_map_t::iterator i = _map.find(filename);
        if (i != _map.end()) {
            uint64_t ltime = GetTickCount64();
            //logu_("key_cache::lookup: %s %llu > %llu\n", filename, i->second.expires, ltime);
            if (i->second.expires > ltime) {
                // hit
                *res = i->second.value;
                //logu_("lookup FOUND: (%08x) %s\n", i->first, filename);
                return true;
            }
            else {
                // hit, but expired value, so: miss
                DBG(32) logu_("lookup EXPIRED MATCH: (%p|%llu <= %llu) %s\n", i->second.value, i->second.expires, ltime, filename);
                _map.erase(i);
            }
        }
        else {
            // miss
        }
        *res = NULL;
        return false;
    }
    void put(char *filename, void *value) {
        uint64_t ltime = GetTickCount64();
        cache_map_value_t v;
        v.value = value;
        v.expires = ltime + _ttl_msec;
        DBG(32) logu_("cache::put: %s --> (%p|%llu)\n", (filename)?filename:"(NULL)", v.value, v.expires);
        {
            lock_t lock(_kcs);
            pair<cache_map_t::iterator,bool> res = _map.insert(
                pair<string,cache_map_value_t>(filename, v));
            if (!res.second) {
                // replace existing
                //logu_("REPLACED for: %s\n", filename);
                res.first->second.value = v.value;
                res.first->second.expires = v.expires;
            }
        }
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
    wstring *filename;
    FILETIME last_modified;
    int stack_position;
    int evt_trophy_check;
    int evt_lcpk_make_key;
    int evt_lcpk_get_filepath;
    int evt_lcpk_rewrite;
    int evt_lcpk_read;
    int evt_set_teams;
    /*
    int evt_set_tid;
    */
    int evt_set_match_time;
    int evt_set_stadium_choice;
    int evt_set_stadium;
    int evt_set_conditions;
    int evt_after_set_conditions;
    /*
    int evt_set_stadium_for_replay;
    int evt_set_conditions_for_replay;
    int evt_after_set_conditions_for_replay;
    */
    int evt_get_ball_name;
    int evt_get_stadium_name;
    /*
    int evt_enter_edit_mode;
    int evt_exit_edit_mode;
    int evt_enter_replay_gallery;
    int evt_exit_replay_gallery;
    */
    int evt_overlay_on;
    int evt_key_down;
    int evt_gamepad_input;
};
list<module_t*> _modules;
module_t* _curr_m;
list<module_t*>::iterator _curr_overlay_m;

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

    // prep gamepad.ini filename
    memset(gamepad_ini, 0, sizeof(gamepad_ini));
    wcscpy(gamepad_ini, dll_log);
    p = wcsrchr(gamepad_ini, L'\\');
    wcscpy(p, L"\\gamepad.ini");

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
    config = new config_t(L"sider", dll_ini);
}

void read_gamepad_global_mapping(gamepad_config_t*& config)
{
    config = new gamepad_config_t(L"gamepad", gamepad_ini);
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

void set_controller_poll_delay() {
    _controller_poll_delay = (_overlay_on) ?
        _config->_overlay_gamepad_poll_interval_msec :
        _config->_gamepad_poll_interval_msec;
}

BOOL sider_device_enum_callback(LPCDIDEVICEINSTANCE lppdi, LPVOID pvRef)
{
    log_(L"controller: type: %x name: %s\n", lppdi->dwDevType, lppdi->tszInstanceName);
    g_controller_guid_instance = lppdi->guidInstance;
    _has_controller = true;
    return DIENUM_CONTINUE;
}

BOOL sider_object_enum_callback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef)
{
    log_(L"object: name: %s, type: 0x%x, ofs: 0x%x\n", lpddoi->tszName, lpddoi->dwType, lpddoi->dwOfs);
    DIDEVICEOBJECTINSTANCE ddoi;
    memcpy(&ddoi, lpddoi,  sizeof(ddoi));
    _di_objects.push_back(ddoi);
    // button 6
    if (ddoi.dwType == 0x604) {
        _di_overlay_toggle1.dwType = ddoi.dwType;
        _di_overlay_toggle1.dwOfs = ddoi.dwOfs;
        _di_module_switch.dwType = ddoi.dwType;
        _di_module_switch.dwOfs = ddoi.dwOfs;
    }
    // button 7
    if (ddoi.dwType == 0x704) {
        _di_overlay_toggle2.dwType = ddoi.dwType;
        _di_overlay_toggle2.dwOfs = ddoi.dwOfs;
    }
    return DIENUM_CONTINUE;
}

wstring* _have_live_file(char *file_name)
{
    wchar_t *unicode_filename = Utf8::utf8ToUnicode((const BYTE*)file_name);
    //wchar_t unicode_filename[512];
    //memset(unicode_filename, 0, sizeof(unicode_filename));
    //Utf8::fUtf8ToUnicode(unicode_filename, file_name);

    wchar_t fn[512];
    for (list<wstring>::iterator it = _config->_cpk_roots.begin();
            it != _config->_cpk_roots.end();
            it++) {
        fn[0] = L'\0';
        wcsncat(fn, it->c_str(), 512);
        wchar_t *p = (unicode_filename[0] == L'\\') ? unicode_filename + 1 : unicode_filename;
        wcsncat(fn, p, 512);

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
            Utf8::free(unicode_filename);
            return new wstring(fn);
        }
    }

    Utf8::free(unicode_filename);
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
    EnterCriticalSection(&_cs);
    it = _lookup_cache.find(string(file_name));
    if (it != _lookup_cache.end()) {
        LeaveCriticalSection(&_cs);
        return it->second;
    }
    else {
        //logu_("_lookup_cache MISS for (%s)\n", file_name);
        wstring* res = _have_live_file(file_name);
        _lookup_cache.insert(pair<string,wstring*>(string(file_name),res));
        LeaveCriticalSection(&_cs);
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

bool module_set_stadium_choice(module_t *m, BYTE stadium_id, BYTE *new_stadium_id)
{
    bool res(false);
    if (m->evt_set_stadium_choice != 0) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(m->L, m->evt_set_stadium_choice);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        lua_pushinteger(L, stadium_id);
        if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
        }
        else if (lua_isnumber(L, -1)) {
            *new_stadium_id = luaL_checkint(L, -1);
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
            lua_pop(L, 1);
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

char *module_ball_name(module_t *m, char *name)
{
    char *res = NULL;
    if (m->evt_get_ball_name != 0) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(m->L, m->evt_get_ball_name);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        lua_pushstring(L, name);
        if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
        }
        else if (lua_isstring(L, -1)) {
            const char *s = luaL_checkstring(L, -1);
            memset(_ball_name, 0, sizeof(_ball_name));
            strncpy(_ball_name, s, sizeof(_ball_name)-1);
            res = _ball_name;
        }
        lua_pop(L, 1);
        LeaveCriticalSection(&_cs);
    }
    return res;
}

char *module_stadium_name(module_t *m, char *name, BYTE stadium_id)
{
    char *res = NULL;
    if (m->evt_get_stadium_name != 0) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(m->L, m->evt_get_stadium_name);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        lua_pushstring(L, name);
        lua_pushinteger(L, stadium_id);
        if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
        }
        else if (lua_isstring(L, -1)) {
            const char *s = luaL_checkstring(L, -1);
            memset(_stadium_name, 0, sizeof(_stadium_name));
            strncpy(_stadium_name, s, sizeof(_stadium_name)-1);
            res = _stadium_name;
        }
        lua_pop(L, 1);
        LeaveCriticalSection(&_cs);
    }
    return res;
}

void module_overlay_on(module_t *m, char **text, char **image_path, struct layout_t *opts)
{
    *text = NULL;
    *image_path = NULL;
    if (m->evt_overlay_on != 0) {
        EnterCriticalSection(&_cs);
        // garbage collection
        lua_gc(L, _config->_lua_gc_opt, 0);
        lua_pushvalue(m->L, m->evt_overlay_on);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        if (lua_pcall(L, 1, 3, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR in overlay_on: %s\n", GetCurrentThreadId(), err);
            lua_pop(L, 1);
        }
        else {
            // check return values
            if (lua_isstring(L, -3)) {
                const char *s = luaL_checkstring(L, -3);
                _overlay_utf8_text[0] = '\0';
                strncat(_overlay_utf8_text, s, sizeof(_overlay_utf8_text)-1);
                *text = _overlay_utf8_text;
                // strip any trailing whitespace
                char *res = *text;
                if (s[0] != '\0') {
                    char *p = res + strlen(res) - 1;
                    while ((p >= res) && ((p[0] == '\r') || (p[0] == '\n') || (p[0] == ' '))) {
                        p[0] = '\0';
                        p--;
                    }
                }
            }
            if (lua_isstring(L, -2)) {
                const char *s = luaL_checkstring(L, -2);
                _overlay_utf8_image_path[0] = '\0';
                strncat(_overlay_utf8_image_path, s, sizeof(_overlay_utf8_image_path)-1);
                *image_path = _overlay_utf8_image_path;
            }
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "image_width");
                if (lua_isnumber(L, -1)) {
                    double value = lua_tonumber(L, -1);
                    opts->image_width = value; // width in pixels
                    if (value < 1.0f) {
                        // treat as screen-width percentage
                        opts->image_width = DX11.Width * value;
                    }
                }
                lua_pop(L, 1);
                lua_getfield(L, -1, "image_height");
                if (lua_isnumber(L, -1)) {
                    double value = lua_tonumber(L, -1);
                    opts->image_height = value; // height in pixels
                    if (value < 1.0f) {
                        // treat as screen-height percentage
                        opts->image_height = DX11.Height * value;
                    }
                }
                lua_pop(L, 1);
                lua_getfield(L, -1, "image_aspect_ratio");
                if (lua_isnumber(L, -1)) {
                    double value = lua_tonumber(L, -1);
                    opts->image_aspect_ratio = value;
                    opts->has_image_ar = true;
                }
                lua_pop(L, 1);
                lua_getfield(L, -1, "image_hmargin");
                if (lua_isnumber(L, -1)) {
                    opts->image_hmargin = lua_tointeger(L, -1); // hmargin in pixels
                    opts->has_image_hmargin = true;
                }
                lua_pop(L, 1);
                lua_getfield(L, -1, "image_vmargin");
                if (lua_isnumber(L, -1)) {
                    opts->image_vmargin = lua_tointeger(L, -1); // vmargin in pixels
                    opts->has_image_vmargin = true;
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 3);
        }
        LeaveCriticalSection(&_cs);
    }
}

void module_key_down(module_t *m, int vkey)
{
    if (m->evt_key_down != 0) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(m->L, m->evt_key_down);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        lua_pushinteger(L, vkey); // ctx
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
            lua_pop(L, 1);
        }
        LeaveCriticalSection(&_cs);
    }
}

void module_gamepad_input(module_t *m, struct di_change_t *changes, size_t len)
{
    if (m->evt_gamepad_input != 0) {
        EnterCriticalSection(&_cs);
        lua_pushvalue(m->L, m->evt_gamepad_input);
        lua_xmove(m->L, L, 1);
        // push params
        lua_pushvalue(L, 1); // ctx
        lua_newtable(L); // table of changes
        for (int i=0; i<len; i++) {
            lua_pushinteger(L, changes[i].dwType);
            lua_pushinteger(L, changes[i].state);
            lua_settable(L, -3);
        }
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            const char *err = luaL_checkstring(L, -1);
            logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
            lua_pop(L, 1);
        }
        LeaveCriticalSection(&_cs);
    }
    else if (m->evt_key_down != 0) {
        // check global gamepad input mapping
        for (int i=0; i<len; i++) {
            BYTE vkey;
            bool mapped = _gamepad_config->lookup(changes[i].dwType, changes[i].state, &vkey);
            if (!mapped) {
                continue;
            }
            DBG(256) logu_("for input event (0x%x,%d) found mapped vkey: 0x%x\n", changes[i].dwType, changes[i].state, vkey);
            EnterCriticalSection(&_cs);
            lua_pushvalue(m->L, m->evt_key_down);
            lua_xmove(m->L, L, 1);
            // push params
            lua_pushvalue(L, 1); // ctx
            lua_pushinteger(L, vkey); // ctx
            if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                const char *err = luaL_checkstring(L, -1);
                logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
                lua_pop(L, 1);
            }
            LeaveCriticalSection(&_cs);
        }
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

void module_read(module_t *m, const char *file_name, void *data, LONGLONG len, FILE_LOAD_INFO *fli)
{
    char *res(NULL);
    EnterCriticalSection(&_cs);
    lua_pushvalue(m->L, m->evt_lcpk_read);
    lua_xmove(m->L, L, 1);
    // push params
    lua_pushvalue(L, 1); // ctx
    lua_pushstring(L, file_name);
    lua_pushlightuserdata(L, data);
    lua_pushinteger(L, len);
    if (fli) {
        lua_pushinteger(L, fli->total_bytes_to_read);
        lua_pushinteger(L, fli->bytes_read_so_far);
    }
    if (lua_pcall(L, 6, 0, 0) != LUA_OK) {
        const char *err = luaL_checkstring(L, -1);
        logu_("[%d] lua ERROR: %s\n", GetCurrentThreadId(), err);
        lua_pop(L, 1);
    }
    LeaveCriticalSection(&_cs);
}

void module_make_key(module_t *m, const char *file_name, char *key, size_t key_maxsize)
{
    key[0] = '\0';
    size_t maxlen = key_maxsize-1;
    if (m->evt_lcpk_make_key != 0) {
        lock_t lock(&_cs);
        // garbage collection
        lua_gc(L, _config->_lua_gc_opt, 0);
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
        lock_t lock(&_cs);
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

    wstring *fn(NULL);
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

void prep_stuff()
{
    LoadLibrary(L"d3d_compiler_46");
	hr = FW1CreateFactory(FW1_VERSION, &g_pFW1Factory);
    if (FAILED(hr)) {
        logu_("FW1CreateFactory failed with: %p\n", hr);
        return;
    }
	hr = g_pFW1Factory->CreateFontWrapper(DX11.Device, L"Arial", &g_pFontWrapper);
    if (FAILED(hr)) {
        logu_("CreateFontWrapper failed with: %p\n", hr);
        return;
    }

    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
    ID3D10Blob* pBlobVS = NULL;
    ID3D10Blob* pBlobError = NULL;
    hr = D3DCompile(g_strVS, lstrlenA(g_strVS) + 1, "VS", NULL, NULL, "VS",
        "vs_4_0", dwShaderFlags, 0, &pBlobVS, &pBlobError);
    if (FAILED(hr))
    {
        if (pBlobError != NULL)
        {
            logu_((char*)pBlobError->GetBufferPointer());
            pBlobError->Release();
        }
        logu_("D3DCompile failed\n");
        return;
    }
    hr = DX11.Device->CreateVertexShader(pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(),
        NULL, &g_pVertexShader);

    ID3D10Blob* pBlobTexVS = NULL;
    ID3D10Blob* pBlobTexError = NULL;
    hr = D3DCompile(g_strTexVS, lstrlenA(g_strTexVS) + 1, "VStex", NULL, NULL, "VStex",
        "vs_4_0", dwShaderFlags, 0, &pBlobTexVS, &pBlobTexError);
    if (FAILED(hr))
    {
        if (pBlobError != NULL)
        {
            logu_((char*)pBlobError->GetBufferPointer());
            pBlobTexError->Release();
        }
        logu_("D3DCompile failed\n");
        return;
    }
    hr = DX11.Device->CreateVertexShader(pBlobTexVS->GetBufferPointer(), pBlobTexVS->GetBufferSize(),
        NULL, &g_pTexVertexShader);

/*
#include "vshader.h"
    logu_("creating vertex shader from array of %d bytes\n", sizeof(g_siderVS));
    hr = DX11.Device->CreateVertexShader(g_siderVS, sizeof(g_siderVS), NULL, &g_pVertexShader);
    if (FAILED(hr)) {
        logu_("DX11.Device->CreateVertexShader failed\n");
        return;
    }
*/

    // Compile and create the pixel shader
    ID3D10Blob* pBlobPS = NULL;
    char pixel_shader[512];
    memset(pixel_shader, 0, sizeof(pixel_shader));
    float r = float(_config->_overlay_background_color & 0x00ff)/255.0;
    float g = float((_config->_overlay_background_color & 0x00ff00) >> 8)/255.0;
    float b = float((_config->_overlay_background_color & 0x00ff0000) >> 16)/255.0;
    float a = float((_config->_overlay_background_color & 0x00ff000000) >> 24)/255.0;
    sprintf(pixel_shader, g_strPS, r, g, b, a);
    hr = D3DCompile(pixel_shader, lstrlenA(pixel_shader) + 1, "PS", NULL, NULL, "PS",
        "ps_4_0", dwShaderFlags, 0, &pBlobPS, &pBlobError);
    if (FAILED(hr))
    {
        if (pBlobError != NULL)
        {
            logu_((char*)pBlobError->GetBufferPointer());
            pBlobError->Release();
        }
        logu_("D3DCompile failed\n");
        return;
    }
    hr = DX11.Device->CreatePixelShader(pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(),
        NULL, &g_pPixelShader);
    if (FAILED(hr)) {
        logu_("DX11.Device->CreatePixelShader failed\n");
        return;
    }
    pBlobPS->Release();

    // Compile and create another pixel shader
    pBlobPS = NULL;
    memset(pixel_shader, 0, sizeof(pixel_shader));
    sprintf(pixel_shader, g_strTexPS, _config->_overlay_image_alpha_max);
    hr = D3DCompile(pixel_shader, strlen(pixel_shader) + 1, "PStex", NULL, NULL, "PStex",
        "ps_4_0", dwShaderFlags, 0, &pBlobPS, &pBlobError);
    if (FAILED(hr))
    {
        if (pBlobError != NULL)
        {
            logu_((char*)pBlobError->GetBufferPointer());
            pBlobError->Release();
        }
        logu_("D3DCompile failed\n");
        return;
    }
    hr = DX11.Device->CreatePixelShader(pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(),
        NULL, &g_pTexPixelShader);
    if (FAILED(hr)) {
        logu_("DX11.Device->CreatePixelShader failed\n");
        return;
    }
    pBlobPS->Release();

/*
#include "pshader.h"
    logu_("creating pixel shader from array of %d bytes\n", sizeof(g_siderPS));
    hr = DX11.Device->CreatePixelShader(g_siderPS, sizeof(g_siderPS), NULL, &g_pPixelShader);
    if (FAILED(hr)) {
        logu_("DX11.Device->CreatePixelShader failed\n");
        return;
    }
*/

    // Create the input layout
    D3D11_INPUT_ELEMENT_DESC elements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = _countof(elements);
    hr = DX11.Device->CreateInputLayout(elements, numElements, pBlobVS->GetBufferPointer(),
        pBlobVS->GetBufferSize(), &g_pInputLayout);
    if (FAILED(hr)) {
        logu_("DX11.Device->CreateInputLayout failed\n");
        return;
    }

    // Create the input layout for texture
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, sizeof(float)*4, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    numElements = _countof(layout);
    hr = DX11.Device->CreateInputLayout(layout, numElements, pBlobTexVS->GetBufferPointer(),
        pBlobTexVS->GetBufferSize(), &g_pTexInputLayout);
    if (FAILED(hr)) {
        logu_("DX11.Device->CreateInputLayout failed\n");
        return;
    }

    pBlobVS->Release();
    pBlobTexVS->Release();

    // Create the state objects
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = DX11.Device->CreateSamplerState( &sampDesc, &g_pSamplerLinear );
    if (FAILED(hr)) {
        logu_("DX11.Device->CreateSamplerState failed\n");
        return;
    }

    D3D11_BLEND_DESC BlendState;
    ZeroMemory(&BlendState, sizeof(D3D11_BLEND_DESC));

    BlendState.RenderTarget[0].BlendEnable = TRUE;
    BlendState.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendState.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendState.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendState.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendState.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BlendState.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendState.RenderTarget[0].RenderTargetWriteMask = 0x0f;
    DX11.Device->CreateBlendState(&BlendState, &g_pBlendState);

    // default to automatic font size
    _font_size = DX11.Height/40.0;
    if (_config->_overlay_font_size > 0) {
        _font_size = (float)_config->_overlay_font_size;
    }

    logu_("prep done successfully!\n");
}

int prep_ui(float font_size, float right_margin)
{
    swprintf(_overlay_text, L"%s | %s | %s", _overlay_header.c_str(), (*_curr_overlay_m)->filename->c_str(), _current_overlay_text);
    UINT flags = 0; //FW1_RESTORESTATE;
    //if (_config->_overlay_location == 1) {
    //    flags |= FW1_BOTTOM;
    //}

    FW1_RECTF rectIn;
    rectIn.Left = 5.0f;
    rectIn.Top = 0.0f;
    rectIn.Right = DX11.Width - right_margin;
    rectIn.Bottom = DX11.Height;
	FW1_RECTF rect = g_pFontWrapper->MeasureString(_overlay_text, _config->_overlay_font.c_str(), font_size, &rectIn, flags);
    //logu_("rect: %0.2f,%0.2f,%0.2f,%0.2f\n", rect.Left,rect.Top,rect.Right,rect.Bottom);
    float height = rect.Bottom;
    if (height < 0) { height = DX11.Height + height; }
    float rel_height = (height + rect.Top) / DX11.Height + 0.005;

    if (_overlay_image.have && _overlay_image.height > 0) {
        float rel_image_height = ((float)_overlay_image.height + _overlay_image.vmargin*2) / DX11.Height;
        rel_height = max(rel_height, rel_image_height);
    }
    //logu_("rel_height: %0.2f\n", rel_height);
    int pixel_height = rel_height * DX11.Height;

    // overlay
    {
        D3D11_BUFFER_DESC bd;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(g_vertices);
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = 0;
        bd.MiscFlags = 0;
        bd.StructureByteStride = 0;
        D3D11_SUBRESOURCE_DATA initData;
        initData.pSysMem = g_vertices;
        hr = DX11.Device->CreateBuffer(&bd, &initData, &g_pVertexBuffer);
        if (FAILED(hr)) {
            logu_("DX11.Device->CreateBuffer failed (for overlay)\n");
            return 0;
        }
    }

    // image
    if (_overlay_image.have) {
        D3D11_BUFFER_DESC bd;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(g_texVertices);
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = 0;
        bd.MiscFlags = 0;
        bd.StructureByteStride = 0;
        D3D11_SUBRESOURCE_DATA initData;
        initData.pSysMem = g_texVertices;
        hr = DX11.Device->CreateBuffer(&bd, &initData, &g_pTexVertexBuffer);
        if (FAILED(hr)) {
            logu_("DX11.Device->CreateBuffer failed (for image)\n");
            return 0;
        }
    }

    return pixel_height;
}

void draw_text(float font_size, float right_margin)
{
    UINT flags = FW1_RESTORESTATE;
    //FLOAT y = DX11.Height*0.0f;
    if (_config->_overlay_location == 1) {
        //flags |= FW1_BOTTOM;
        //y = DX11.Height*1.0f;
    }

    FW1_RECTF rectIn;
    rectIn.Left = 5.0f;
    rectIn.Top = 0.0f;
    rectIn.Right = DX11.Width - right_margin;
    rectIn.Bottom = DX11.Height;

	g_pFontWrapper->DrawString(
		DX11.Context,
        _overlay_text,
        _config->_overlay_font.c_str(),
		font_size,// Font size
		//DX11.Width*0.01f,// X position
		//y,// Y position
        &rectIn,
        _config->_overlay_text_color, //0xd080ff80 - Text color, 0xAaBbGgRr
        NULL, NULL,
		flags //0// Flags (for example FW1_RESTORESTATE to keep context states unchanged)
	);
	//pFontWrapper->Release();
	//pFW1Factory->Release();
}

void draw_ui(float top, float bottom, float right_margin)
{
    // Create the render target view
    ID3D11Texture2D* pRenderTargetTexture;
    hr = DX11.SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pRenderTargetTexture);
    if (FAILED(hr)) {
        logu_("DX11.SwapChain->GetBuffer failed\n");
        return;
    }

    hr = DX11.Device->CreateRenderTargetView(pRenderTargetTexture, NULL, &g_pRenderTargetView);
    if (FAILED(hr)) {
        logu_("DX11.Device->CreateRenderTargetView failed\n");
        pRenderTargetTexture->Release();
        return;
    }
    pRenderTargetTexture->Release();

    RECT rc;
    GetClientRect(DX11.Window, &rc);

    // draw overlay background
    {
        DX11.Context->IASetInputLayout(g_pInputLayout);

        UINT stride = sizeof(SimpleVertex);
        UINT offset = 0;
        DX11.Context->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
        DX11.Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        DX11.Context->VSSetShader(g_pVertexShader, NULL, 0);
        DX11.Context->PSSetShader(g_pPixelShader, NULL, 0);

        D3D11_VIEWPORT vp;
        vp.Width = (FLOAT)(rc.right - rc.left);
        vp.Height = (FLOAT)(bottom - top);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = top;

        DX11.Context->RSSetViewports(1, &vp);
        DX11.Context->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);
        DX11.Context->OMSetBlendState(g_pBlendState, NULL, 0xffffffff);
        DX11.Context->Draw(6, 0); //6 vertices start at 0
    }

    // draw texture
    if (_overlay_image.have) {
        DX11.Context->IASetInputLayout(g_pTexInputLayout);

        UINT stride = sizeof(TexturedVertex);
        UINT offset = 0;
        DX11.Context->IASetVertexBuffers(0, 1, &g_pTexVertexBuffer, &stride, &offset);
        DX11.Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        DX11.Context->VSSetShader(g_pTexVertexShader, NULL, 0);
        DX11.Context->PSSetShader(g_pTexPixelShader, NULL, 0);
        DX11.Context->PSSetShaderResources( 0, 1, &g_textureView );
        DX11.Context->PSSetSamplers( 0, 1, &g_pSamplerLinear );

        D3D11_VIEWPORT vp;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = rc.right - rc.left - _overlay_image.width - _overlay_image.hmargin;
        vp.TopLeftY = top + _overlay_image.vmargin;
        vp.Width = _overlay_image.width;
        vp.Height = _overlay_image.height;
        DX11.Context->RSSetViewports(1, &vp);
        DX11.Context->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);

        //float bf [4] = {1.0f, 1.0f, 1.0f, 1.0f};
        DX11.Context->OMSetBlendState(g_pBlendState, NULL, 0xffffffff);
        DX11.Context->Draw(6, 0); //6 vertices start at 0
    }

    // text
    {
        D3D11_VIEWPORT vp;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = top;
        vp.Width = rc.right - rc.left;
        vp.Height = bottom - top;
        DX11.Context->RSSetViewports(1, &vp);
        draw_text(_font_size, right_margin);
    }

    //cleanup
    g_pRenderTargetView->Release();
}

void sider_switch_overlay_to_next_module()
{
    if (_curr_overlay_m != _modules.end()) {
        // next module
        _curr_overlay_m++;
        for (; _curr_overlay_m != _modules.end(); _curr_overlay_m++) {
            module_t *m = *_curr_overlay_m;
            if (m->evt_overlay_on) {
                log_(L"now active module on overlay: %s\n", m->filename->c_str());
                break;
            }
        }
        if (_curr_overlay_m == _modules.end()) {
            // start from beginning again
            _curr_overlay_m = _modules.begin();
            for (; _curr_overlay_m != _modules.end(); _curr_overlay_m++) {
                module_t *m = *_curr_overlay_m;
                if (m->evt_overlay_on) {
                    log_(L"now active module on overlay: %s\n", m->filename->c_str());
                    break;
                }
            }
        }
        _overlay_image.to_clear = true;
    }
}

void clear_overlay_texture() {
    SAFE_RELEASE(g_texture);
    SAFE_RELEASE(g_textureView);
    if (_overlay_image.filepath) { free(_overlay_image.filepath); }
    memset(&_overlay_image, 0, sizeof(overlay_image_t));
}

int get_stick_state(int state) {
    float val = (state - 32767)/32767.0f;
    if (val < -0.85) { return -1; }
    else if (val > 0.85) { return 1; }
    return 0;
}

DWORD direct_input_poll(void *param) {
    HRESULT hr;
    while (_controller_poll) {
        hr = g_IDirectInputDevice8->Acquire();
        if (SUCCEEDED(hr)) {
            memcpy(_prev_controller_buttons, _controller_buttons, sizeof(_controller_buttons));
            hr = g_IDirectInputDevice8->GetDeviceState(sizeof(_controller_buttons), _controller_buttons);
            if (SUCCEEDED(hr)) {
                // log changes
                DBG(64) {
                    if (memcmp(_prev_controller_buttons, _controller_buttons, sizeof(_controller_buttons))!=0) {
                        logu_("was: ");
                        list<DIDEVICEOBJECTINSTANCE>::iterator it;
                        for (it = _di_objects.begin(); it != _di_objects.end(); it++) {
                            if (it->dwType & DIDFT_AXIS) {
                                log_(L"|%s (0x%x): %d\n", it->tszName, it->dwType, *(DWORD*)(_prev_controller_buttons + it->dwOfs));
                            }
                            else if (it->dwType & DIDFT_POV) {
                                log_(L"|%s (0x%x): %d\n", it->tszName, it->dwType, *(DWORD*)(_prev_controller_buttons + it->dwOfs));
                            }
                            else {
                                log_(L"|%s (0x%x): %d\n", it->tszName, it->dwType, _prev_controller_buttons[it->dwOfs]);
                            }
                        }
                        logu_("\n");
                        logu_("now: ");
                        for (it = _di_objects.begin(); it != _di_objects.end(); it++) {
                            if (it->dwType & DIDFT_AXIS) {
                                log_(L"|%s (0x%x): %d\n", it->tszName, it->dwType, *(DWORD*)(_controller_buttons + it->dwOfs));
                            }
                            else if (it->dwType & DIDFT_POV) {
                                log_(L"|%s (0x%x): %d\n", it->tszName, it->dwType, *(DWORD*)(_controller_buttons + it->dwOfs));
                            }
                            else {
                                log_(L"|%s (0x%x): %d\n", it->tszName, it->dwType, _controller_buttons[it->dwOfs]);
                            }
                        }
                        logu_("\n");
                    }
                }

                // test for events
                bool handled(false);

                // overlay toggle
                BYTE was_b1 = *(BYTE*)(_prev_controller_buttons + _di_overlay_toggle1.dwOfs);
                BYTE was_b2 = *(BYTE*)(_prev_controller_buttons + _di_overlay_toggle2.dwOfs);
                BYTE b1 = *(BYTE*)(_controller_buttons + _di_overlay_toggle1.dwOfs);
                BYTE b2 = *(BYTE*)(_controller_buttons + _di_overlay_toggle2.dwOfs);
                if (b1==128 && b2==128 && (was_b1!=b1 || was_b2!=b2)) {
                    _overlay_on = !_overlay_on;
                    set_controller_poll_delay();
                    handled = true;
                    DBG(64) logu_("overlay: %s\n", (_overlay_on)?"ON":"OFF");
                }
                else {
                    // module switch
                    BYTE was_b = *(BYTE*)(_prev_controller_buttons + _di_module_switch.dwOfs);
                    BYTE b = *(BYTE*)(_controller_buttons + _di_module_switch.dwOfs);
                    if (b==128 && was_b!=b) {
                        if (_overlay_on) {
                            sider_switch_overlay_to_next_module();
                            handled = true;
                        }
                    }
                }

                // check states
                if (_overlay_on && !handled && _curr_overlay_m != _modules.end()) {
                    _di_changes_len = 0;
                    list<DIDEVICEOBJECTINSTANCE>::iterator it;
                    for (it = _di_objects.begin(); it != _di_objects.end(); it++) {
                        if (it->dwType & DIDFT_AXIS) {
                            // sticks/d-pad
                            int was = get_stick_state(*(int*)(_prev_controller_buttons + it->dwOfs));
                            int now = get_stick_state(*(int*)(_controller_buttons + it->dwOfs));
                            if (was != now) {
                                _di_changes[_di_changes_len].state = now;
                                _di_changes[_di_changes_len].dwType = it->dwType;
                                _di_changes_len++;
                            }
                        }
                        else if (it->dwType & DIDFT_POV) {
                            // rotational stick
                            int was = *(int*)(_prev_controller_buttons + it->dwOfs);
                            int now = *(int*)(_controller_buttons + it->dwOfs);
                            if (was != now) {  // degree-like: 0/4500/9000/13500/27000/31500
                                int state = (now == -1) ? now : now / 100;
                                _di_changes[_di_changes_len].state = state;
                                _di_changes[_di_changes_len].dwType = it->dwType;
                                _di_changes_len++;
                            }
                        }
                        else {
                            // buttons
                            BYTE was = _prev_controller_buttons[it->dwOfs];
                            BYTE now = _controller_buttons[it->dwOfs];
                            if (was != now) { // down/up: 128/0
                                int state = now / 128;
                                _di_changes[_di_changes_len].state = state;
                                _di_changes[_di_changes_len].dwType = it->dwType;
                                _di_changes_len++;
                            }
                        }
                    }

                    if (_di_changes_len>0) {
                        DBG(256) {
                            logu_("number of input changes: %d\n", _di_changes_len);
                            for (int i=0; i<_di_changes_len; i++) {
                                logu_("change: dwType=0x%x, state=%d\n", _di_changes[i].dwType, _di_changes[i].state);
                            }
                        }
                        // lua callback: generate input-change event
                        module_t *m = *_curr_overlay_m;
                        module_gamepad_input(m, _di_changes, _di_changes_len);
                    }
                }
            }
            else {
                if (hr == DIERR_INVALIDPARAM) {
                    logu_("failed to get device state: DIERR_INVALIDPARAM\n");
                }
                else if (hr == DIERR_NOTACQUIRED) {
                    logu_("failed to get device state: DIERR_NOTACQUIRED\n");
                }
                else {
                    logu_("failed to get device state: %x\n", hr);
                }
            }
            g_IDirectInputDevice8->Unacquire();
        }

        Sleep(_controller_poll_delay);
    }
    logu_("Done polling DirectInput device\n");
    return 0;
}

HRESULT sider_Present(IDXGISwapChain *swapChain, UINT SyncInterval, UINT Flags)
{
    //logu_("Present called for swapChain: %p\n", swapChain);

    if (_config->_overlay_enabled) {
        if (kb_handle == NULL) {
            kb_handle = SetWindowsHookEx(WH_KEYBOARD, sider_keyboard_proc, myHDLL, GetCurrentThreadId());
        }
    }

    if (_reload_modified) {
        clear_overlay_texture();
        lua_reload_modified_modules();
        _reload_modified = false;
    }

    // process priority
    if (!_priority_set) {
        _priority_set = true;
        if (_config->_priority_class) {
            if (SetPriorityClass(GetCurrentProcess(), _config->_priority_class)) {
                logu_("SetPriorityClass successful for priority: 0x%x\n", _config->_priority_class);
            }
            else {
                logu_("SetPriorityClass failed for priority: 0x%x\n", _config->_priority_class);
            }
        }
    }

    if (_has_controller) {
        if (!_controller_prepped) {
            HRESULT hr;
            if (FAILED(g_IDirectInputDevice8->SetCooperativeLevel(DX11.Window, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE))) {
                logu_("failed to set cooperative level\n");
            }
            hr = g_IDirectInputDevice8->SetDataFormat(&_data_format);
            if (FAILED(hr)) {
                if (hr == DIERR_INVALIDPARAM) {
                    logu_("failed to set data format: DIERR_INVALIDPARAM\n");
                }
                else {
                    logu_("failed to set data format: %x\n");
                }
            }
            _controller_prepped = true;

            // launch
            _controller_poll = true;
            DWORD thread_id;
            _controller_poll_handle = CreateThread(NULL, 0, direct_input_poll, NULL, 0, &thread_id);
            SetThreadPriority(_controller_poll_handle, THREAD_PRIORITY_LOWEST);
            logu_("created controller poll thread: 0x%x\n", thread_id);
        }
    }

    if (_overlay_on) {
        // ask currently active module for text
        char *text = NULL;
        if (_config->_lua_enabled) {
            // lua callbacks
            if (_curr_overlay_m != _modules.end()) {
                char *image_path = NULL;
                layout_t opts;
                memset(&opts, 0, sizeof(layout_t));
                int image_width = 0;
                int image_hmargin;
                int image_vmargin;
                module_t *m = *_curr_overlay_m;
                module_overlay_on(m, &text, &image_path, &opts);
                if (text) {
                    wchar_t *ws = Utf8::utf8ToUnicode((BYTE*)text);
                    wcscpy(_current_overlay_text, ws);
                    Utf8::free(ws);
                }
                else {
                    // empty
                    _current_overlay_text[0] = L'\0';
                }

                if (_overlay_image.to_clear) {
                    clear_overlay_texture();
                }

                if (image_path != NULL) {
                    if (!_overlay_image.filepath || strcmp(image_path, _overlay_image.filepath)!=0) {
                        // load image into texture
                        SAFE_RELEASE(g_texture);
                        SAFE_RELEASE(g_textureView);
                        _overlay_image.filepath = strdup(image_path);

                        HRESULT hr;
                        wchar_t *ws = Utf8::utf8ToUnicode((BYTE*)image_path);
                        if (memcmp(".dds", image_path+strlen(image_path)-4, 4)==0) {
                            hr = DirectX::CreateDDSTextureFromFile(DX11.Device, ws, &g_texture, &g_textureView);
                        }
                        else {
                            // try other supported formats
                            hr = DirectX::CreateWICTextureFromFile(DX11.Device, ws, &g_texture, &g_textureView);
                        }
                        Utf8::free(ws);
                        if (SUCCEEDED(hr)) {
                            DBG(128) logu_("Loaded 2D texture: {%s}\n", _overlay_image.filepath);
                            _overlay_image.have = true;

                            D3D11_RESOURCE_DIMENSION resType = D3D11_RESOURCE_DIMENSION_UNKNOWN;
                            g_texture->GetType( &resType );

                            switch( resType )
                            {
                            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                                {
                                    ID3D11Texture2D* tex = (ID3D11Texture2D*)g_texture;
                                    D3D11_TEXTURE2D_DESC desc;
                                    tex->GetDesc(&desc);

                                    // This is a 2D texture. Check values of desc here
                                    DBG(128) logu_("texture Width: %d\n", desc.Width);
                                    DBG(128) logu_("texture Height: %d\n", desc.Height);
                                    DBG(128) logu_("texture MipLevels: %d\n", desc.MipLevels);
                                    DBG(128) logu_("texture ArraySize: %d\n", desc.ArraySize);
                                    DBG(128) logu_("texture Format: %d\n", desc.Format);
                                    _overlay_image.source_width = desc.Width;
                                    _overlay_image.source_height = desc.Height;

                                    // calculate dimensions based on two of:
                                    // opts.image_width, opts.image_height, opts.image_aspect_ratio
                                    // if not enough info: use default width of 0.1*screen-width and source image aspect ratio
                                    if (opts.image_width > 0) {
                                        _overlay_image.width = opts.image_width;
                                        if (opts.image_height > 0) {
                                            _overlay_image.height = opts.image_height;
                                        }
                                        else if (opts.has_image_ar) {
                                            _overlay_image.height = _overlay_image.width / opts.image_aspect_ratio;
                                        }
                                        else {
                                            _overlay_image.height = _overlay_image.width * ((double)desc.Height / desc.Width);
                                        }
                                    }
                                    else {
                                        // width is not specified
                                        if (opts.image_height > 0) {
                                            _overlay_image.height = opts.image_height;
                                            if (opts.has_image_ar) {
                                                _overlay_image.width = _overlay_image.height * opts.image_aspect_ratio;
                                            }
                                            else {
                                                _overlay_image.width = _overlay_image.height * ((double)desc.Width / desc.Height);
                                            }
                                        }
                                        else {
                                            // neither width nor height specified
                                            _overlay_image.width = min(desc.Width, DX11.Width * 0.1);
                                            if (opts.has_image_ar) {
                                                _overlay_image.height = _overlay_image.width / opts.image_aspect_ratio;
                                            }
                                            else {
                                                _overlay_image.height = _overlay_image.width * ((double)desc.Height / desc.Width);
                                            }
                                        }
                                    }

                                    _overlay_image.hmargin = (opts.has_image_hmargin) ? opts.image_hmargin : 10.0f;
                                    _overlay_image.vmargin = (opts.has_image_vmargin) ? opts.image_vmargin : 10.0f;
                                    DBG(128) logu_("on-screen pixels-width: %d\n", _overlay_image.width);
                                    DBG(128) logu_("on-screen pixels-height: %d\n", _overlay_image.height);
                                }
                                break;
                            default:
                                logu_("PROBLEM: Not a 2D texture: {%s}\n", _overlay_image.filepath);
                                if (_overlay_image.filepath) { free(_overlay_image.filepath); }
                                _overlay_image.have = false;
                            }
                        }
                        else {
                            logu_("PROBLEM: Cannot load texture from: {%s}\n", _overlay_image.filepath);
                            _overlay_image.have = false;
                        }
                    }
                }
                else {
                    // image_path is NULL, so clear the image
                    clear_overlay_texture();
                }

                float right_margin = (_overlay_image.have) ? _overlay_image.width + _overlay_image.hmargin*2 : 5.0f;

                // render overlay
                DX11.Device->GetImmediateContext(&DX11.Context);
                int pixel_height = prep_ui(_font_size, right_margin);
                if (pixel_height > 0) {
                    float top = (_config->_overlay_location == 0) ? 0 : DX11.Height - pixel_height;
                    draw_ui(top, top + pixel_height, right_margin);
                }
                SAFE_RELEASE(g_pVertexBuffer);
                SAFE_RELEASE(g_pTexVertexBuffer);
                DX11.Context->Release();
            }
        }
    }

    HRESULT hr = _org_Present(swapChain, SyncInterval, Flags);
    return hr;
}

HRESULT sider_CreateSwapChain(IDXGIFactory1 *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
    HRESULT hr = _org_CreateSwapChain(pFactory, pDevice, pDesc, ppSwapChain);
    logu_("hr=0x%x, IDXGISwapChain: %p\n", hr, *ppSwapChain);
    _swap_chain = *ppSwapChain;

    _device = (ID3D11Device*)pDevice;
    logu_("==> device: %p\n", _device);
    if (SUCCEEDED(_swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&_device))) {
        logu_("==> device: %p\n", _device);
    }

    _device_context = NULL;
    _device->GetImmediateContext(&_device_context);
    logu_("==> device context: %p\n", _device_context);
    logu_("==> swap chain: %p\n", _swap_chain);
    if (_device_context) {
        _device_context->Release();
    }

    DX11.Device = _device;
    DX11.SwapChain = _swap_chain;
    DX11.Device->GetImmediateContext(&DX11.Context);

    DXGI_SWAP_CHAIN_DESC desc;
    if (SUCCEEDED(DX11.SwapChain->GetDesc(&desc))) {
        DX11.Window = desc.OutputWindow;
        DX11.Width = desc.BufferDesc.Width;
        DX11.Height = desc.BufferDesc.Height;
        logu_("==> window handle: %p\n", DX11.Window);
    }

    prep_stuff();

    // check if we need to hook Present method
    IDXGISwapChain *sc = (IDXGISwapChain*)(*ppSwapChain);
    BYTE** vtbl = *(BYTE***)sc;
    PFN_IDXGISwapChain_Present present = (PFN_IDXGISwapChain_Present)vtbl[8];
    DBG(64) logu_("current Present = %p\n", present);
    if ((BYTE*)present == (BYTE*)sider_Present) {
        DBG(64) logu_("Present already hooked.\n");
    }
    else {
        logu_("Hooking Present\n");
        _org_Present = present;
        logu_("_org_Present = %p\n", _org_Present);

        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(vtbl+8, 8, newProtection, &protection)) {
            vtbl[8] = (BYTE*)sider_Present;

            present = (PFN_IDXGISwapChain_Present)vtbl[8];
            logu_("now Present = %p\n", present);
        }
        else {
            logu_("ERROR: VirtualProtect failed for: %p\n", vtbl+8);
        }
    }

    return hr;
}

HRESULT sider_CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
    HRESULT hr = _org_CreateDXGIFactory1(riid, ppFactory);
    DBG(64) logu_("hr=0x%x, IDXGIFactory1: %p\n", hr, *ppFactory);

    // check if we need to hook SwapChain method
    IDXGIFactory1 *f = (IDXGIFactory1*)(*ppFactory);
    BYTE** vtbl = *(BYTE***)f;
    PFN_IDXGIFactory1_CreateSwapChain sc = (PFN_IDXGIFactory1_CreateSwapChain)vtbl[10];
    DBG(64) logu_("current CreateSwapChain = %p\n", sc);
    if ((BYTE*)sc == (BYTE*)sider_CreateSwapChain) {
        DBG(64) logu_("CreateSwapChain already hooked.\n");
    }
    else {
        logu_("Hooking CreateSwapChain\n");
        _org_CreateSwapChain = sc;
        logu_("_org_CreateSwapChain = %p\n", _org_CreateSwapChain);

        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(vtbl+10, 8, newProtection, &protection)) {
            vtbl[10] = (BYTE*)sider_CreateSwapChain;

            sc = (PFN_IDXGIFactory1_CreateSwapChain)vtbl[10];
            logu_("now CreateSwapChain = %p\n", sc);
        }
        else {
            logu_("ERROR: VirtualProtect failed for: %p\n", vtbl+10);
        }
    }
    return hr;
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
    FILE_LOAD_INFO *fli;

    //log_(L"rs (R12) = %p\n", rs);
    if (rs) {
        if (_config->_lua_enabled && _rewrite_count > 0) do_rewrite(rs->filename);
        DBG(1) logu_("read_file:: rs->filesize: %llx, rs->offset: %llx, rs->filename: %s\n",
            rs->filesize, rs->offset.full, rs->filename);

        BYTE* p = (BYTE*)rs;
        fli = *((FILE_LOAD_INFO **)(p - 0x18));

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

    if (rs) {
        LONGLONG num_bytes_read = *lpNumberOfBytesRead;

        // livecpk_read
        if (_config->_lua_enabled) {
            list<module_t*>::iterator i;
            for (i = _modules.begin(); i != _modules.end(); i++) {
                module_t *m = *i;
                if (m->evt_lcpk_read != 0) {
                    module_read(m, rs->filename, lpBuffer, num_bytes_read, fli);
                }
            }
        }
    }

    return result;
}

void sider_mem_copy(BYTE *dst, LONGLONG dst_len, BYTE *src, LONGLONG src_len, struct READ_STRUCT *rs)
{
    HANDLE handle = INVALID_HANDLE_VALUE;
    wstring *filename = NULL;

    // do the original copy operation
    memcpy_s(dst, dst_len, src, src_len);

    LONGLONG dst_len_used = dst_len;

    if (rs) {
        if (_config->_lua_enabled && _rewrite_count > 0) do_rewrite(rs->filename);
        DBG(1) logu_("mem_copy:: rs->filesize: %llx, rs->offset: %llx, rs->filename: %s\n",
            rs->filesize, rs->offset.full, rs->filename);

        BYTE* p = (BYTE*)rs;
        FILE_LOAD_INFO *fli = *((FILE_LOAD_INFO **)(p - 0x18));

        wstring *fn(NULL);
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

                dst_len_used = numberOfBytesRead;
            }
        }

        // livecpk_read
        if (_config->_lua_enabled) {
            list<module_t*>::iterator i;
            for (i = _modules.begin(); i != _modules.end(); i++) {
                module_t *m = *i;
                if (m->evt_lcpk_read != 0) {
                    module_read(m, rs->filename, dst, dst_len_used, fli);
                }
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

    wstring *fn(NULL);
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
    if (!dest || !team_id_encoded) {
        // safety check
        return;
    }

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
            _stadium_choice_count = 0;
        }
        else {
            _tournament_id = mi->tournament_id_encoded;
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
    bool ok = mi && (mi->db0x03 >= 0 && mi->db0x03 <=6 ) && (mi->db0x17 == 0x17 || mi->db0x17 == 0x12);
    if (!ok) {
        // safety check
        DBG(16) logu_("%02x %02x\n", mi->db0x03, mi->db0x17);
        return;
    }
    DBG(16) logu_("%02x %02x (ok)\n", mi->db0x03, mi->db0x17);

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
        //set_context_field_nil("stadium_choice");
        //_had_stadium_choice = false;

        for (i = _modules.begin(); i != _modules.end(); i++) {
            module_t *m = *i;
            module_after_set_conditions(m);
        }
    }

    logu_("settings now:: stadium=%d, timeofday=%d, weather=%d, season=%d\n",
        dest_ss->stadium, dest_ss->timeofday, dest_ss->weather, dest_ss->season);
}

WORD sider_trophy_check(WORD trophy_id)
{
    WORD tid = trophy_id;
    DBG(16) logu_("trophy check:: trophy-id: 0x%0x\n", tid);
    if (_config->_lua_enabled) {
        // lua callbacks
        list<module_t*>::iterator i;
        for (i = _modules.begin(); i != _modules.end(); i++) {
            module_t *m = *i;
            WORD new_tid = tid;
            WORD new_tournament_id = _tournament_id;
            if (module_trophy_rewrite(m, _tournament_id, &new_tournament_id)) {
                EnterCriticalSection(&_tcs);
                trophy_map_t::iterator it = _trophy_map->find(new_tournament_id);
                if (it != _trophy_map->end()) {
                    new_tid = it->second;
                    DBG(16) logu_("trophy check:: rewrite trophy-id: 0x%x --> 0x%x\n", tid, new_tid);
                    LeaveCriticalSection(&_tcs);
                    return new_tid;
                }
                LeaveCriticalSection(&_tcs);
            }
        }
    }
    return tid;
}

void sider_context_reset()
{
    clear_context_fields(_context_fields, _context_fields_count);
    _tournament_id = 0xffff;
    _stadium_choice_count = 0;
    logu_("context reset\n");
}

void sider_free_select(BYTE *controller_restriction)
{
    *controller_restriction = 0;
}

void sider_trophy_table(TROPHY_TABLE_ENTRY *tt)
{
    //logu_("trophy table addr: %p\n", tt);
    //logu_("tid: %d (0x%x) --> 0x%x\n", tt->tournament_id, tt->trophy_id);
    EnterCriticalSection(&_tcs);
    for (int i=0; i<TT_LEN; i++) {
        (*_trophy_map)[tt->tournament_id] = tt->trophy_id;
        tt++;
    }
    LeaveCriticalSection(&_tcs);
}

char* sider_ball_name(char *ball_name)
{
    if (_config->_lua_enabled) {
        // lua callbacks
        list<module_t*>::iterator i;
        for (i = _modules.begin(); i != _modules.end(); i++) {
            module_t *m = *i;
            char *new_ball_name = module_ball_name(m, ball_name);
            if (new_ball_name) {
                return new_ball_name;
            }
        }
    }
    return ball_name;
}

char* sider_stadium_name(STAD_INFO_STRUCT *stad_info)
{
    if (_config->_lua_enabled) {
        // lua callbacks
        list<module_t*>::iterator i;
        for (i = _modules.begin(); i != _modules.end(); i++) {
            module_t *m = *i;
            char *new_stadium_name = module_stadium_name(m, stad_info->name, stad_info->id);
            if (new_stadium_name) {
                return new_stadium_name;
            }
        }
    }
    return stad_info->name;
}

STAD_INFO_STRUCT* sider_def_stadium_name(DWORD stadium_id)
{
    memset(&_stadium_info, 0, sizeof(STAD_INFO_STRUCT));
    _stadium_info.id = stadium_id;
    strcpy((char*)&_stadium_info.name, "Unknown stadium");
    return &_stadium_info;
}

void sider_set_stadium_choice(MATCH_INFO_STRUCT *mi, BYTE stadium_choice)
{
    _stadium_choice_count++;
    mi->stadium_choice = stadium_choice;
    if (_stadium_choice_count % 2 == 1) {
        if (_config->_lua_enabled) {
            // lua callbacks
            list<module_t*>::iterator i;
            for (i = _modules.begin(); i != _modules.end(); i++) {
                module_t *m = *i;
                BYTE new_stadium_choice;
                if (module_set_stadium_choice(m, stadium_choice, &new_stadium_choice)) {
                    mi->stadium_choice = new_stadium_choice;
                    break;
                }
            }
        }
        set_context_field_int("stadium_choice", mi->stadium_choice);
        logu_("set_stadium_choice: %d\n", mi->stadium_choice);
    }
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
            VirtualProtect(bptr, 8, protection, &newProtection);
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
    if (VirtualProtect(loc, 12 + nops, newProtection, &protection)) {
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
    if (VirtualProtect(loc, 12 + nops, newProtection, &protection)) {
        memcpy(loc, "\x48\xb9", 2);
        memcpy(loc+2, &p, sizeof(BYTE*));  // mov rcx,<target_addr>
        memcpy(loc+10, "\xff\xd1", 2);      // call rcx
        if (nops) {
            memset(loc+12, '\x90', nops);  // nop ;one of more nops for padding
        }
        log_(L"hook_call_rcx: hooked at %p (target: %p)\n", loc, p);
    }
}

void hook_call_rdx(BYTE *loc, BYTE *p, size_t nops) {
    if (!loc) {
        return;
    }
    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect(loc, 12 + nops, newProtection, &protection)) {
        memcpy(loc, "\x48\xba", 2);
        memcpy(loc+2, &p, sizeof(BYTE*));  // mov rcx,<target_addr>
        memcpy(loc+10, "\xff\xd2", 2);      // call rcx
        if (nops) {
            memset(loc+12, '\x90', nops);  // nop ;one of more nops for padding
        }
        log_(L"hook_call_rdx: hooked at %p (target: %p)\n", loc, p);
    }
}

void hook_call_with_tail(BYTE *loc, BYTE *p, BYTE *tail, size_t tail_size) {
    if (!loc) {
        return;
    }
    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect(loc, 12 + tail_size, newProtection, &protection)) {
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
    if (VirtualProtect(loc, head_size + 12 + tail_size, newProtection, &protection)) {
        memcpy(loc, head, head_size);   // head code
        memcpy(loc+head_size, "\x48\xb8", 2);
        memcpy(loc+head_size+2, &p, sizeof(BYTE*));  // mov rax,<target_addr>
        memcpy(loc+head_size+10, "\xff\xd0", 2);     // call rax
        memcpy(loc+head_size+12, tail, tail_size);   // tail code
        log_(L"hook_call_with_head_and_tail: hooked at %p (target: %p)\n", loc, p);
    }
}

void hook_call_rdx_with_head_and_tail(BYTE *loc, BYTE *p, BYTE *head, size_t head_size, BYTE *tail, size_t tail_size) {
    if (!loc) {
        return;
    }
    DWORD protection = 0 ;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect(loc, head_size + 12 + tail_size, newProtection, &protection)) {
        memcpy(loc, head, head_size);   // head code
        memcpy(loc+head_size, "\x48\xba", 2);
        memcpy(loc+head_size+2, &p, sizeof(BYTE*));  // mov rdx,<target_addr>
        memcpy(loc+head_size+10, "\xff\xd2", 2);     // call rdx
        memcpy(loc+head_size+12, tail, tail_size);   // tail code
        log_(L"hook_call_rdx_with_head_and_tail: hooked at %p (target: %p)\n", loc, p);
    }
}

void hook_call_rdx_with_head_and_tail_and_moved_call(BYTE *loc, BYTE *p, BYTE *head, size_t head_size, BYTE *tail, size_t tail_size, BYTE *moved_call_old, BYTE* moved_call_new) {
    if (!loc) {
        return;
    }
    DWORD protection = 0 ;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect(loc, head_size + 12 + tail_size, newProtection, &protection)) {
        int old_call_offs = *(int*)(moved_call_old + 1);
        memcpy(loc, head, head_size);   // head code
        memcpy(loc+head_size, "\x48\xba", 2);
        memcpy(loc+head_size+2, &p, sizeof(BYTE*));  // mov rdx,<target_addr>
        memcpy(loc+head_size+10, "\xff\xd2", 2);     // call rdx
        memcpy(loc+head_size+12, tail, tail_size);   // tail code

        int new_call_offs = old_call_offs - (int)(moved_call_new - moved_call_old);
        *(int*)(moved_call_new + 1) = new_call_offs;

        log_(L"hook_call_rdx_with_head_and_tail_and_moved_call: hooked at %p (target: %p)\n", loc, p);
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
    else if (strcmp(event_key, "livecpk_read")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_lcpk_read = lua_gettop(_curr_m->L);
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
    else if (strcmp(event_key, "set_stadium_choice")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_set_stadium_choice = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
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
    */
    else if (strcmp(event_key, "get_ball_name")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_get_ball_name = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "overlay_on")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_overlay_on = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "key_down")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_key_down = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "gamepad_input")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_gamepad_input = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    else if (strcmp(event_key, "get_stadium_name")==0) {
        lua_pushvalue(L, -1);
        lua_xmove(L, _curr_m->L, 1);
        _curr_m->evt_get_stadium_name = lua_gettop(_curr_m->L);
        logu_("Registered for \"%s\" event\n", event_key);
    }
    /*
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
        "collectgarbage",
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

    // z lib
    init_z_lib(L);
    lua_setfield(L, -2, "zlib");

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

            FILETIME last_mod_time;
            memset(&last_mod_time, 0, sizeof(FILETIME));
            GetFileTime(handle, NULL, NULL, &last_mod_time);

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
            m->filename = new wstring(it->c_str());
            m->last_modified = last_mod_time;
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
                m->stack_position = lua_gettop(L);
                logu_("OK: Lua module initialized: %s (stack position: %d)\n", mfile.c_str(), m->stack_position);
                //logu_("gettop: %d\n", lua_gettop(L));

                // add to list of loaded modules
                _modules.push_back(m);
            }
        }
        _curr_overlay_m = _modules.end();
        list<module_t*>::iterator j;
        for (j = _modules.begin(); j != _modules.end(); j++) {
            module_t *m = *j;
            if (m->evt_overlay_on) {
                _curr_overlay_m = j;
                break;
            }
        }
        log_(L"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
        log_(L"Lua module system initialized.\n");
        log_(L"Active modules: %d\n", _modules.size());
        log_(L"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    }
}

void lua_reload_modified_modules()
{
    lock_t lock(&_cs);
    list<module_t*>::iterator j;
    int count = 0;
    log_(L"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    log_(L"Reloading modified modules ...\n");

    for (j = _modules.begin(); j != _modules.end(); j++) {
        module_t *m = *j;

        wstring script_file(sider_dir);
        script_file += L"modules\\";
        script_file += m->filename->c_str();

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
            log_(L"Reloading skipped because: PROBLEM: Unable to open file: %s\n",
                script_file.c_str());
            continue;
        }

        FILETIME last_mod_time;
        memset(&last_mod_time, 0, sizeof(FILETIME));
        GetFileTime(handle, NULL, NULL, &last_mod_time);

        uint64_t *a = (uint64_t*)&last_mod_time;
        uint64_t *b = (uint64_t*)&(m->last_modified);

        if (!(*a > *b)) {
            // not modified since last load
            CloseHandle(handle);
            continue;
        }

        log_(L"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
        log_(L"Modified module: %s ...\n", m->filename->c_str());

        size = GetFileSize(handle, NULL);
        BYTE *buf = new BYTE[size+1];
        memset(buf, 0, size+1);
        DWORD bytesRead = 0;
        if (!ReadFile(handle, buf, size, &bytesRead, NULL)) {
            log_(L"PROBLEM: ReadFile error for lua module: %s\n",
                m->filename->c_str());
            CloseHandle(handle);
            continue;
        }
        CloseHandle(handle);
        // script is now in memory

        char *mfilename = (char*)Utf8::unicodeToUtf8(m->filename->c_str());
        string mfile(mfilename);
        Utf8::free(mfilename);
        int r = luaL_loadbuffer(L, (const char*)buf, size, mfile.c_str());
        delete buf;
        if (r != 0) {
            const char *err = lua_tostring(L, -1);
            logu_("Lua module reloading PROBLEM: %s.\n", err);
            logu_("WARNING: We are keeping the old version in memory\n");
            lua_pop(L, 1);
            continue;
        }

        // set environment
        push_env_table(L, m->filename->c_str());
        lua_setfenv(L, -2);

        // run the module
        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            logu_("Lua module initializing problem: %s. "
                  "Reloading cancelled\n", err);
            lua_pop(L, 1);
            continue;
        }

        // check that module chunk is correctly constructed:
        // it must return a table
        if (!lua_istable(L, -1)) {
            logu_("PROBLEM: Lua module (%s) must return a table. "
                  "Reloading cancelled\n", mfile.c_str());
            lua_pop(L, 1);
            continue;
        }

        // now we have module table on the stack
        // run its "init" method, with a context object
        lua_getfield(L, -1, "init");
        if (!lua_isfunction(L, -1)) {
            logu_("PROBLEM: Lua module (%s) does not "
                  "have \"init\" function. Reloading cancelled.\n",
                  mfile.c_str());
            lua_pop(L, 1);
            continue;
        }

        module_t *newm = new module_t();
        memset(newm, 0, sizeof(module_t));
        newm->filename = new wstring(m->filename->c_str());
        newm->last_modified = last_mod_time;
        newm->cache = new lookup_cache_t();
        newm->L = luaL_newstate();
        _curr_m = newm;

        lua_pushvalue(L, 1); // ctx
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            logu_("PROBLEM: Lua module (%s) \"init\" function "
                  "returned an error: %s\n", mfile.c_str(), err);
            logu_("Reloading cancelled\n");
            lua_pop(L, 1);
            // pop the module table too, since we are not using it
            lua_pop(L, 1);

            // clean up
            lua_close(newm->L);
            delete newm->cache;
            delete newm;
        }
        else {
            newm->stack_position = m->stack_position;
            log_(L"RELOAD OK: Lua module initialized: %s (stack position: %d)\n", newm->filename->c_str(), newm->stack_position);
            memcpy(m, newm, sizeof(module_t));  // new version takes over
            lua_replace(L, newm->stack_position); // move to original module position on the stack
            count++;

            //logu_("gettop: %d\n", lua_gettop(L));

            // cleanup old state
            // todo: figure out a safe way

            // check if need to advance _curr_overlay_m iterator
            if (_curr_overlay_m != _modules.end() && m == *_curr_overlay_m) {
                if (!m->evt_overlay_on) {
                    // this module no longer supports evt_overlay_on
                    // need to switch to another
                    bool switched(false);
                    list<module_t*>::iterator k = _curr_overlay_m;
                    k++;
                    for (; k != _modules.end(); k++) {
                        module_t *newm = *k;
                        if (newm->evt_overlay_on) {
                            _curr_overlay_m = k;
                            switched = true;
                            break;
                        }
                    }
                    // go again from the start, if not switched yet
                    if (!switched) {
                        list<module_t*>::iterator k;
                        for (k = _modules.begin(); k != _modules.end(); k++) {
                            module_t *newm = *k;
                            if (newm->evt_overlay_on) {
                                _curr_overlay_m = k;
                                break;
                            }
                        }
                    }
                }
            }
            // reload finished
        }
    }
    log_(L"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    log_(L"Reloaded modules: %d\n", count);
    log_(L"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
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

    InitializeCriticalSection(&_tcs);
    _trophy_map = new trophy_map_t();

    log_(L"debug = %d\n", _config->_debug);
    //if (_config->_game_speed) {
    //    log_(L"game.speed = %0.3f\n", *(_config->_game_speed));
    //}
    log_(L"game.priority.class = 0x%x\n", _config->_priority_class);
    log_(L"livecpk.enabled = %d\n", _config->_livecpk_enabled);
    log_(L"lookup-cache.enabled = %d\n", _config->_lookup_cache_enabled);
    log_(L"lua.enabled = %d\n", _config->_lua_enabled);
    log_(L"lua.gc.opt = %s\n", (_config->_lua_gc_opt == LUA_GCSTEP)? L"step" : L"collect");
    log_(L"luajit.ext.enabled = %d\n", _config->_luajit_extensions_enabled);
    //log_(L"address-cache.enabled = %d\n", (int)(!_config->_ac_off));
    log_(L"key-cache.ttl-sec = %d\n", _config->_key_cache_ttl_sec);
    log_(L"rewrite-cache.ttl-sec = %d\n", _config->_rewrite_cache_ttl_sec);
    log_(L"start.minimized = %d\n", _config->_start_minimized);
    log_(L"free.side.select = %d\n", _config->_free_side_select);
    log_(L"overlay.enabled = %d\n", _config->_overlay_enabled);
    log_(L"overlay.on-from-start = %d\n", _config->_overlay_on_from_start);
    log_(L"overlay.gamepad.enabled = %d\n", _config->_overlay_controlled_by_gamepad);
    log_(L"overlay.font = %s\n", _config->_overlay_font.c_str());
    log_(L"overlay.text-color = 0x%08x\n", _config->_overlay_text_color);
    log_(L"overlay.background-color = 0x%08x\n", _config->_overlay_background_color);
    log_(L"overlay.image-alpha-max = %0.3f\n", _config->_overlay_image_alpha_max);
    log_(L"overlay.location = %d\n", _config->_overlay_location);
    log_(L"overlay.font-size = %d\n", _config->_overlay_font_size);
    log_(L"overlay.vkey.toggle = 0x%02x\n", _config->_overlay_vkey_toggle);
    log_(L"overlay.vkey.next-module = 0x%02x\n", _config->_overlay_vkey_next_module);
    log_(L"overlay.gamepad.poll-interval-msec = %d\n", _config->_overlay_gamepad_poll_interval_msec);
    log_(L"gamepad.poll-interval-msec = %d\n", _config->_gamepad_poll_interval_msec);
    log_(L"vkey.reload-1 = 0x%02x\n", _config->_vkey_reload_1);
    log_(L"vkey.reload-2 = 0x%02x\n", _config->_vkey_reload_2);
    log_(L"close.on.exit = %d\n", _config->_close_sider_on_exit);
    log_(L"match.minutes = %d\n", _config->_num_minutes);

    log_(L"--------------------------\n");
    log_(L"Global input mapping: %d items\n", _gamepad_config->_map.size());
    for (unordered_map<DWORD,gamepad_input_mapping_t>::iterator it = _gamepad_config->_map.begin();
            it != _gamepad_config->_map.end();
            it++) {
        log_(L"gamepad.input.mapping: 0x%08x --> (0x%x,%d,0x%x)\n", it->first,
            it->second.dwType, it->second.value, it->second.vkey);
    }

    log_(L"--------------------------\n");
    log_(L"hook.set-team-id = %d\n", _config->_hook_set_team_id);
    log_(L"hook.set-settings = %d\n", _config->_hook_set_settings);
    log_(L"hook.context-reset = %d\n", _config->_hook_context_reset);
    log_(L"hook.trophy-table = %d\n", _config->_hook_trophy_table);
    log_(L"hook.trophy-check = %d\n", _config->_hook_trophy_check);
    log_(L"--------------------------\n");

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
            init_direct_input();
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
            cfg->_hp_at_trophy_table > 0 &&
            cfg->_hp_at_ball_name > 0 &&
            cfg->_hp_at_stadium_name > 0 &&
            cfg->_hp_at_def_stadium_name > 0 &&
            cfg->_hp_at_context_reset > 0 &&
            cfg->_hp_at_set_stadium_choice > 0
        );
    }
    if (cfg->_num_minutes > 0) {
        all = all && (
            //cfg->_hp_at_set_min_time > 0 &&
            cfg->_hp_at_set_max_time > 0 &&
            cfg->_hp_at_set_minutes > 0
        );
    }
    if (cfg->_overlay_enabled) {
        all = all && (
            cfg->_hp_at_dxgi > 0
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

#define NUM_PATTERNS 20
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
    frag[13] = pattern_trophy_table;
    frag[14] = pattern_ball_name;
    frag[15] = pattern_dxgi;
    frag[16] = pattern_set_stadium_choice;
    frag[17] = pattern_stadium_name;
    frag[18] = pattern_def_stadium_name;
    frag[19] = pattern2_set_settings;
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
    frag_len[9] = 0; //sizeof(pattern_set_min_time)-1;
    frag_len[10] = sizeof(pattern_set_max_time)-1;
    frag_len[11] = (_config->_num_minutes > 0) ? sizeof(pattern_set_minutes)-1 : 0;
    frag_len[12] = _config->_free_side_select ? sizeof(pattern_sider)-1 : 0;
    frag_len[13] = _config->_lua_enabled ? sizeof(pattern_trophy_table)-1 : 0;
    frag_len[14] = _config->_lua_enabled ? sizeof(pattern_ball_name)-1 : 0;
    frag_len[15] = _config->_overlay_enabled ? sizeof(pattern_dxgi)-1 : 0;
    frag_len[16] = _config->_lua_enabled ? sizeof(pattern_set_stadium_choice)-1 : 0;
    frag_len[17] = _config->_lua_enabled ? sizeof(pattern_stadium_name)-1 : 0;
    frag_len[18] = _config->_lua_enabled ? sizeof(pattern_def_stadium_name)-1 : 0;
    frag_len[19] = _config->_lua_enabled ? sizeof(pattern2_set_settings)-1 : 0;
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
    offs[13] = offs_trophy_table;
    offs[14] = offs_ball_name;
    offs[15] = offs_dxgi;
    offs[16] = offs_set_stadium_choice;
    offs[17] = offs_stadium_name;
    offs[18] = offs_def_stadium_name;
    offs[19] = offs_set_settings;
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
    addrs[13] = &_config->_hp_at_trophy_table;
    addrs[14] = &_config->_hp_at_ball_name;
    addrs[15] = &_config->_hp_at_dxgi;
    addrs[16] = &_config->_hp_at_set_stadium_choice;
    addrs[17] = &_config->_hp_at_stadium_name;
    addrs[18] = &_config->_hp_at_def_stadium_name;
    addrs[19] = &_config->_hp_at_set_settings;

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

        if (_config->_overlay_enabled) {
            if (_config->_hp_at_dxgi) {
                BYTE *addr = get_target_addr(_config->_hp_at_dxgi);
                BYTE *loc = get_target_location(addr);
                _org_CreateDXGIFactory1 = *(PFN_CreateDXGIFactory1*)loc;
                logu_("_org_CreateDXGIFactory1: %p\n", _org_CreateDXGIFactory1);
                hook_indirect_call(addr, (BYTE*)sider_CreateDXGIFactory1);
            }
        }

        if (_config->_lua_enabled) {
            log_(L"-------------------------------\n");
            log_(L"sider_set_team_id: %p\n", sider_set_team_id_hk);
            log_(L"sider_set_settings: %p\n", sider_set_settings_hk);
            log_(L"sider_trophy_check: %p\n", sider_trophy_check_hk);
            log_(L"sider_trophy_table: %p\n", sider_trophy_table_hk);
            log_(L"sider_context_reset: %p\n", sider_context_reset_hk);
            log_(L"sider_ball_name: %p\n", sider_ball_name_hk);
            log_(L"sider_stadium_name: %p\n", sider_stadium_name_hk);
            log_(L"sider_def_stadium_name: %p\n", sider_def_stadium_name_hk);
            log_(L"sider_set_stadium_choice: %p\n", sider_set_stadium_choice_hk);

            if (_config->_hook_set_team_id) {
                BYTE *check_addr = _config->_hp_at_set_team_id - offs_set_team_id + offs_check_set_team_id;
                logu_("_hp_at_set_team_id: %p\n", _config->_hp_at_set_team_id);
                logu_("check_addr: %p\n", check_addr);
                logu_("instruction at check_addr: %02x %02x\n", check_addr[0], check_addr[1]);
                if (memcmp(check_addr, check_set_team_id_1, sizeof(check_set_team_id_1)-1)==0) {
                    logu_("Using 1st variation of set_team_id hook\n");
                    hook_call_rdx_with_head_and_tail(_config->_hp_at_set_team_id, (BYTE*)sider_set_team_id_hk,
                        (BYTE*)pattern_set_team_id_head, sizeof(pattern_set_team_id_head)-1,
                        (BYTE*)pattern_set_team_id_tail_1, sizeof(pattern_set_team_id_tail_1)-1);
                }
                else if (memcmp(check_addr, check_set_team_id_2, sizeof(check_set_team_id_2)-1)==0) {
                    logu_("Using 2nd variation of set_team_id hook\n");
                    hook_call_rdx_with_head_and_tail(_config->_hp_at_set_team_id, (BYTE*)sider_set_team_id_hk,
                        (BYTE*)pattern_set_team_id_head, sizeof(pattern_set_team_id_head)-1,
                        (BYTE*)pattern_set_team_id_tail_2, sizeof(pattern_set_team_id_tail_2)-1);
                }
            }
            if (_config->_hook_set_settings)
                hook_call_with_head_and_tail(_config->_hp_at_set_settings, (BYTE*)sider_set_settings_hk,
                    (BYTE*)pattern_set_settings_head, sizeof(pattern_set_settings_head)-1,
                    (BYTE*)pattern_set_settings_tail, sizeof(pattern_set_settings_tail)-1);
            if (_config->_hook_trophy_check)
                hook_call_rcx(_config->_hp_at_trophy_check, (BYTE*)sider_trophy_check_hk, 0);
            if (_config->_hook_trophy_table)
                hook_call_rcx(_config->_hp_at_trophy_table, (BYTE*)sider_trophy_table_hk, 0);
            if (_config->_hook_context_reset)
                hook_call(_config->_hp_at_context_reset, (BYTE*)sider_context_reset_hk, 6);
            hook_call_with_head_and_tail(_config->_hp_at_ball_name, (BYTE*)sider_ball_name_hk,
                (BYTE*)pattern_ball_name_head, sizeof(pattern_ball_name_head)-1,
                (BYTE*)pattern_ball_name_tail, sizeof(pattern_ball_name_tail)-1);
            hook_call_with_head_and_tail(_config->_hp_at_stadium_name, (BYTE*)sider_stadium_name_hk,
                (BYTE*)pattern_stadium_name_head, sizeof(pattern_stadium_name_head)-1,
                (BYTE*)pattern_stadium_name_tail, sizeof(pattern_stadium_name_tail)-1);
            hook_call_with_head_and_tail(_config->_hp_at_set_stadium_choice, (BYTE*)sider_set_stadium_choice_hk,
                (BYTE*)pattern_set_stadium_choice_head, sizeof(pattern_set_stadium_choice_head)-1,
                (BYTE*)pattern_set_stadium_choice_tail, sizeof(pattern_set_stadium_choice_tail)-1);

            BYTE *old_moved_call = _config->_hp_at_def_stadium_name + def_stadium_name_moved_call_offs_old;
            BYTE *new_moved_call = _config->_hp_at_def_stadium_name + def_stadium_name_moved_call_offs_new;
            hook_call_rdx_with_head_and_tail_and_moved_call(
                _config->_hp_at_def_stadium_name, (BYTE*)sider_def_stadium_name_hk,
                (BYTE*)pattern_def_stadium_name_head, sizeof(pattern_def_stadium_name_head)-1,
                (BYTE*)pattern_def_stadium_name_tail, sizeof(pattern_def_stadium_name_tail)-1,
                old_moved_call, new_moved_call);
            log_(L"-------------------------------\n");
        }

        if (_config->_free_side_select) {
            log_(L"sider_free_select: %p\n", sider_free_select_hk);
            hook_call_rcx(_config->_hp_at_sider, (BYTE*)sider_free_select_hk, 0);
        }

        //patch_at_location(_config->_hp_at_set_min_time, "\x90\x90\x90\x90\x90\x90\x90", 7);
        patch_at_location(_config->_hp_at_set_max_time, "\x90\x90\x90\x90\x90\x90\x90", 7);

        if (_config->_num_minutes != 0) {

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

void init_direct_input()
{
    // initialize DirectInput
    g_IDirectInput8 = NULL;
    if (!_config->_overlay_controlled_by_gamepad) {
        return;
    }
    if (SUCCEEDED(DirectInput8Create(
        myHDLL, DIRECTINPUT_VERSION, IID_IDirectInput8,
        (void**)&g_IDirectInput8, NULL))) {
        logu_("g_IDirectInput8 = %p\n", g_IDirectInput8);

        // enumerate devices
        _has_controller = false;
        logu_("Enumerating game controllers\n");
        if (SUCCEEDED(g_IDirectInput8->EnumDevices(
            DI8DEVCLASS_GAMECTRL, sider_device_enum_callback, NULL, DIEDFL_ALLDEVICES))) {
            logu_("Done enumerating game controllers\n");

            if (_has_controller) {
                g_IDirectInputDevice8 = NULL;
                if (SUCCEEDED(g_IDirectInput8->CreateDevice(
                    g_controller_guid_instance, &g_IDirectInputDevice8, NULL))) {
                    logu_("DirectInputDevice created: %p\n", g_IDirectInputDevice8);

                    // enumerate buttons and prepare data format
                    if (SUCCEEDED(g_IDirectInputDevice8->EnumObjects(
                        sider_object_enum_callback, NULL, DIDFT_PSHBUTTON | DIDFT_AXIS | DIDFT_POV))) {
                        logu_("number of inputs: %d\n", _di_objects.size());

                        memset(&_data_format, 0, sizeof(DIDATAFORMAT));
                        _data_format.dwSize = sizeof(DIDATAFORMAT);
                        _data_format.dwObjSize = sizeof(DIOBJECTDATAFORMAT);
                        _data_format.dwFlags = DIDF_ABSAXIS;
                        _data_format.dwDataSize = sizeof(_controller_buttons);
                        _data_format.dwNumObjs = _di_objects.size();
                        size_t rgodf_size = sizeof(DIOBJECTDATAFORMAT) * _di_objects.size();
                        _data_format.rgodf = (LPDIOBJECTDATAFORMAT)malloc(rgodf_size);
                        int i = 0;
                        list<DIDEVICEOBJECTINSTANCE>::iterator it;
                        for (it = _di_objects.begin(); it != _di_objects.end(); it++, i++) {
                            _data_format.rgodf[i].pguid = &it->guidType;
                            _data_format.rgodf[i].dwOfs = it->dwOfs;
                            _data_format.rgodf[i].dwType = it->dwType;
                            _data_format.rgodf[i].dwFlags = 0;
                        }

                        _di_changes = new di_change_t[_di_objects.size()];

                        _controller_prepped = false;
                        memset(_controller_buttons, 0, sizeof(_controller_buttons));
                        memset(_prev_controller_buttons, 0, sizeof(_prev_controller_buttons));
                    }
                }
            }
        }
        else {
            logu_("PROBLEM enumerating game controllers\n");
        }
    }
    else {
        logu_("PROBLEM creating DirectInput interface\n");
    }
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
                read_gamepad_global_mapping(_gamepad_config);

                wstring version;
                get_module_version(hDLL, version);
                open_log_(L"============================\n");
                log_(L"Sider DLL: version %s\n", version.c_str());
                log_(L"Filename match: %s\n", match->c_str());

                _overlay_on = _config->_overlay_on_from_start;
                set_controller_poll_delay();
                _overlay_header = L"sider ";
                _overlay_header += version;
                memset(_overlay_text, 0, sizeof(_overlay_text));
                memset(_current_overlay_text, 0, sizeof(_current_overlay_text));
                memset(_overlay_utf8_text, 0, sizeof(_overlay_utf8_text));
                memset(_overlay_utf8_image_path, 0, sizeof(_overlay_utf8_image_path));
                memset(&_overlay_image, 0, sizeof(overlay_image_t));

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

                if (_controller_poll_handle != INVALID_HANDLE_VALUE) {
                    log_(L"Waiting for controller poll thread to finish ...\n");
                    _controller_poll = false;
                    DWORD res = WaitForSingleObject(_controller_poll_handle, 1000);
                    log_(L"Wait is over: %08x\n", res);
                }

                if (g_IDirectInputDevice8) {
                    g_IDirectInputDevice8->Unacquire();
                    log_(L"Releasing DirectInputDevice interface (%p)\n", g_IDirectInputDevice8);
                    g_IDirectInputDevice8->Release();
                    g_IDirectInputDevice8 = NULL;
                }
                if (g_IDirectInput8) {
                    log_(L"Releasing DirectInput interface (%p)\n", g_IDirectInput8);
                    g_IDirectInput8->Release();
                    g_IDirectInput8 = NULL;
                }
                if (_di_changes) {
                    delete [] _di_changes;
                    _di_changes = NULL;
                }

                log_(L"DLL detaching from (%s).\n", module_filename);
                log_(L"Unmapping from PES.\n");

                if (L) { lua_close(L); }

                // tell sider.exe to close
                if (_config->_close_sider_on_exit) {
                    main_hwnd = FindWindow(SIDERCLS, NULL);
                    if (main_hwnd) {
                        PostMessage(main_hwnd, SIDER_MSG_EXIT, 0, 0);
                        log_(L"Posted message for sider.exe to quit\n");
                    }
                }
                close_log_();
                DeleteCriticalSection(&_cs);
                DeleteCriticalSection(&_tcs);
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

LRESULT CALLBACK sider_keyboard_proc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code < 0) {
        return CallNextHookEx(kb_handle, code, wParam, lParam);
    }

    if (code == HC_ACTION) {
        if (wParam == _config->_overlay_vkey_toggle && ((lParam & 0x80000000) != 0)) {
            _overlay_on = !_overlay_on;
            set_controller_poll_delay();
            DBG(64) logu_("overlay: %s\n", (_overlay_on)?"ON":"OFF");
            if (_overlay_on) {
                _overlay_image.to_clear = true;
            }
        }
        else if (wParam == _config->_vkey_reload_2 && ((lParam & 0x80000000) != 0)) {
            if (_reload_1_down) {
                _reload_modified = true;
            }
        }
        else if (wParam == _config->_vkey_reload_1 && ((lParam & 0x80000000) == 0)) {
            _reload_1_down = ((lParam & 0x80000000) == 0);
        }

        if (_overlay_on) {
            //logu_("sider_keyboard_proc: wParam=%p, lParam=%p\n", wParam, lParam);
            // deliver keyboard event to module
            if (_config->_lua_enabled && ((lParam & 0x80000000) == 0)) {
                if (_curr_overlay_m != _modules.end()) {
                    // module switching keys
                    // "[" - 0xdb, "]" - 0xdd, "~" - 0xc0, "1" - 0x31
                    if (wParam == _config->_overlay_vkey_next_module) {
                        sider_switch_overlay_to_next_module();
                    }
                    else {
                        // lua callback
                        module_t *m = *_curr_overlay_m;
                        module_key_down(m, (int)wParam);
                    }
                }
            }
        }
    }
    return CallNextHookEx(kb_handle, code, wParam, lParam);
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
    if (kb_handle) {
        UnhookWindowsHookEx(kb_handle);
    }
}
