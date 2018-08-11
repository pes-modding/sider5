#ifndef _SIDER_PATTERNS_H
#define _SIDER_PATTERNS_H

// code patterns to search

static BYTE lcpk_pattern_at_read_file[23] =
    "\x48\x8b\x0b"
    "\x48\x83\x64\x24\x20\x00"
    "\x4c\x8d\x4c\x24\x60"
    "\x44\x8b\xc7"
    "\x48\x8b\xd6"
    "\xff\x15";
static int lcpk_offs_at_read_file = 20;

static BYTE lcpk_pattern_at_get_size[25] =
    "\xeb\x05"
    "\xe8\x7b\xf3\xff\xff"
    "\x85\xc0"
    "\x74\x24"
    "\x8b\x44\x24\x34"
    "\x89\x43\x04"
    "\x8b\x44\x24\x30"
    "\x89\x03";
static int lcpk_offs_at_get_size = 24;

static BYTE lcpk_pattern_at_write_cpk_filesize[16] =
    "\x48\x8b\x44\x24\x48"
    "\x48\x89\x47\x08"
    "\x49\x89\x7d\x00"
    "\x33\xc0";
static int lcpk_offs_at_write_cpk_filesize = 0;

static BYTE lcpk_pattern_at_mem_copy[13] =
    "\x4c\x8b\x01"
    "\x4c\x8b\xcb"
    "\x49\x8b\xcb"
    "\x4d\x03\xc2";
static int lcpk_offs_at_mem_copy = 9;

static BYTE lcpk_pattern_at_lookup_file[16] =
    "\x48\x8d\x8f\x10\x01\x00\x00"
    "\x4c\x8b\xc6"
    "\x48\x8d\x54\x24\x20";
static int lcpk_offs_at_lookup_file = 0;

/*
000000014126DF00 | 49 63 00                           | movsxd rax,dword ptr ds:[r8]            | prep to write team info
000000014126DF03 | 83 F8 02                           | cmp eax,2                               |
000000014126DF06 | 7D 16                              | jge pes2018.14126DF1E                   |
000000014126DF08 | 4C 69 C0 20 05 00 00               | imul r8,rax,520                         |
000000014126DF0F | 48 81 C1 04 01 00 00               | add rcx,104                             |
000000014126DF16 | 49 03 C8                           | add rcx,r8                              |
*/
static BYTE pattern_set_team_id[26] =
    "\x49\x63\x00"
    "\x83\xf8\x02"
    "\x7d\x16"
    "\x4c\x69\xc0\x20\x05\x00\x00"
    "\x48\x81\xc1\x04\x01\x00\x00"
    "\x49\x03\xc8";
static int offs_set_team_id = 0;

/*
000000014126DF0C | 83 F8 02                           | cmp eax,2                               |
000000014126DF0F | 7D 0D                              | jge pes2018.14126DF1E                   |
000000014126DF11 | 90                                 | nop                                     |
000000014126DF12 | 90                                 | nop                                     |
000000014126DF13 | 90                                 | nop                                     |
000000014126DF14 | 90                                 | nop                                     |
000000014126DF15 | 90                                 | nop                                     |
000000014126DF16 | 90                                 | nop                                     |
000000014126DF17 | 90                                 | nop                                     |
000000014126DF18 | 90                                 | nop                                     |
*/
static BYTE pattern_set_team_id_tail[14] =
    "\x83\xf8\x02"
    "\x7d\x0d"
    "\x90\x90\x90\x90\x90\x90\x90\x90";

/*
00000001412A4FD5 | 0F B6 82 8B 00 00 00               | movzx eax,byte ptr ds:[rdx+8B]          |
00000001412A4FDC | 88 81 8B 00 00 00                  | mov byte ptr ds:[rcx+8B],al             |
00000001412A4FE2 | 48 8B C1                           | mov rax,rcx                             |
00000001412A4FE5 | C3                                 | ret                                     |
*/
static BYTE pattern_set_settings[18] =
    "\x0f\xb6\x82\x8b\x00\x00\x00"
    "\x88\x81\x8b\x00\x00\x00"
    "\x48\x8b\xc1"
    "\xc3";
static int offs_set_settings = 0;

/*
0000000141C5A870 | 0F B7 D0                           | movzx edx,ax                            | check tournament_id for trophy
0000000141C5A873 | 66 89 44 24 50                     | mov word ptr ss:[rsp+50],ax             |
0000000141C5A878 | 48 8B CD                           | mov rcx,rbp                             |
*/
static BYTE pattern_trophy_check[12] =
    "\x0f\xb7\xd0"
    "\x66\x89\x44\x24\x50"
    "\x48\x8b\xcd";
static int offs_trophy_check = -12;

/*
0000000140A0DF3C | 48 89 8B 84 00 00 00                 | mov qword ptr ds:[rbx+84],rcx           |
0000000140A0DF43 | 48 C7 83 AC 59 01 00 FF FF FF FF     | mov qword ptr ds:[rbx+159AC],FFFFFFFFFF |
*/
static BYTE pattern_context_reset[19] =
    "\x48\x89\x8b\x84\x00\x00\x00"
    "\x48\xc7\x83\xac\x59\x01\x00\xff\xff\xff\xff";
static int offs_context_reset = 0;

////// PES 2019 demo ///////////////////

/*
00007FF71BC1F33C | C7 47 58 00 00 80 3F                 | mov dword ptr ds:[rdi+58],3F800000         | set min time (float)
00007FF71BC1F343 | F3 0F 10 47 58                       | movss xmm0,dword ptr ds:[rdi+58]           |
*/
static BYTE pattern_set_min_time[13] =
    "\xc7\x47\x58\x00\x00\x80\x3f"
    "\xf3\x0f\x10\x47\x58";
static int offs_set_min_time = 0;

/*
00007FF71BC1F351 | C7 47 58 00 00 F0 41                 | mov dword ptr ds:[rdi+58],41F00000         | set max time (float)
00007FF71BC1F358 | 48 8B CE                             | mov rcx,rsi                                |
*/
static BYTE pattern_set_max_time[11] =
    "\xc7\x47\x58\x00\x00\xf0\x41"
    "\x48\x8b\xce";
static int offs_set_max_time = 0;

/*
00007FF71B036790 | 88 51 1C                             | mov byte ptr ds:[rcx+1C],dl                | set minutes
00007FF71B036793 | C3                                   | ret                                        |
00007FF71B036794 | CC                                   | int3                                       |
00007FF71B036795 | CC                                   | int3                                       |
00007FF71B036796 | CC                                   | int3                                       |
00007FF71B036797 | CC                                   | int3                                       |
00007FF71B036798 | CC                                   | int3                                       |
00007FF71B036799 | CC                                   | int3                                       |
*/
static BYTE pattern_set_minutes[11] =
    "\x88\x51\x1c"
    "\xc3"
    "\xcc\xcc\xcc\xcc\xcc\xcc";
static int offs_set_minutes = 0;
static BYTE patch_set_minutes[6] =
    "\xc6\x41\x1c\x0a"
    "\xc3";

#endif
