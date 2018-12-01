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
000000014ACDA200 | 49 63 00                             | movsxd rax,dword ptr ds:[r8]               |
000000014ACDA203 | 83 F8 02                             | cmp eax,2                                  |
000000014ACDA206 | 7D 16                                | jge pes2019.14ACDA21E                      |
000000014ACDA208 | 4C 69 C0 EC 05 00 00                 | imul r8,rax,5EC                            |
000000014ACDA20F | 48 81 C1 18 01 00 00                 | add rcx,118                                |
000000014ACDA216 | 4C 01 C1                             | add rcx,r8                                 |
*/
static BYTE pattern_set_team_id[18] =
    //"\x49\x63\x00"
    //"\x83\xf8\x02"
    //"\x7d\x26"
    "\x4c\x69\xc0\xec\x05\x00\x00"
    "\x48\x81\xc1\x18\x01\x00\x00"
    "\x4c\x01\xc1";
static int offs_set_team_id = -8;

static BYTE pattern_set_team_id_head[2] =
    "\x52"; // push rdx

/*
000000014D9310DD | 5A                                   | pop rdx                                  |
000000014D9310DE | 83 F8 02                             | cmp eax,2                                |
000000014D9310E1 | 7D 0B                                | jge pes2019.14D9310EE                    |
000000014D9310E3 | 90                                   | nop                                      |
000000014D9310E4 | 90                                   | nop                                      |
000000014D9310E5 | 90                                   | nop                                      |
000000014D9310E6 | 90                                   | nop                                      |
000000014D9310E7 | 90                                   | nop                                      |
000000014D9310E8 | 90                                   | nop                                      |
*/
static BYTE pattern_set_team_id_tail_1[13] =
    "\x5a"
    "\x83\xf8\x02"
    "\x7d\x0b"
    "\x90\x90\x90\x90\x90\x90";
static BYTE pattern_set_team_id_tail_2[13] =
    "\x5a"
    "\x83\xf8\x02"
    "\x7d\x1b"
    "\x90\x90\x90\x90\x90\x90";

static BYTE check_set_team_id_1[3] = "\x7d\x16";
static BYTE check_set_team_id_2[3] = "\x7d\x26";
static int offs_check_set_team_id = -2;

/*
000000014ACD9D36 | 8B 82 98 00 00 00                    | mov eax,dword ptr ds:[rdx+98]        |
000000014ACD9D3C | 89 81 98 00 00 00                    | mov dword ptr ds:[rcx+98],eax        |
000000014ACD9D42 | 48 89 C8                             | mov rax,rcx                          | set_settings
000000014ACD9D45 | C3                                   | ret                                  |
*/
static BYTE pattern_set_settings[17] =
    "\x8b\x82\x98\x00\x00\x00"
    "\x89\x81\x98\x00\x00\x00"
    "\x48\x89\xc8"
    "\xc3";
static int offs_set_settings = -13;
static BYTE pattern_set_settings_head[2] =
    "\x50";  // push rax
static BYTE pattern_set_settings_tail[13] =
    "\x58"   // pop rax
    "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90";

/*
000000015110A827 | 4C 8D A5 F0 C3 00 00                  | lea r12,qword ptr ss:[rbp+C3F0] |
000000015110A82E | 8B 8D 88 04 00 00                     | mov ecx,dword ptr ss:[rbp+488]  | t-check (4)
000000015110A834 | 41 80 E6 01                           | and r14b,1                      |
000000015110A838 | D1 FE                                 | sar esi,1                       |
*/
static BYTE pattern_trophy_check[20] =
    "\x4c\x8d\xa5\xf0\xc3\x00\x00"
    "\x8b\x8d\x88\x04\x00\x00"
    "\x41\x80\xe6\x01"
    "\xd1\xfe";
static int offs_trophy_check = 7;

static BYTE pattern_trophy_check_head[5] =
    "\x48\x83\xec\x28";

static BYTE pattern_trophy_check_tail[10] =
    "\x48\x85\xd2"
    "\x0f\x84\x8d\x00\x00\x00";

/*
0000000140A0DF3C | 48 89 8B 84 00 00 00                 | mov qword ptr ds:[rbx+84],rcx           |
0000000140A0DF43 | 48 C7 83 74 1F 02 00 FF FF FF FF     | mov qword ptr ds:[rbx+21F74],FFFFFFFFFF |
*/
static BYTE pattern_context_reset[19] =
    "\x48\x89\x8b\x84\x00\x00\x00"
    "\x48\xc7\x83\x74\x1f\x02\x00\xff\xff\xff\xff";
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

// controller restrictions ("sider")

static BYTE pattern_sider[13] =
    "\xf2\x0f\x10\x00"
    "\xf2\x0f\x11\x83\xa4\x00\x00\x00";
static int offs_sider = 0;

// tournament_id --> trophy_id table
/*
00000001509325A1 | 4C 8D 9C 24 70 0A 00 00        | lea r11,qword ptr ss:[rsp+A70]       |
00000001509325A9 | 49 8B 5B 30                    | mov rbx,qword ptr ds:[r11+30]        |
00000001509325AD | 49 8B 73 38                    | mov rsi,qword ptr ds:[r11+38]        |
00000001509325B1 | 49 8B 7B 40                    | mov rdi,qword ptr ds:[r11+40]        |

000000015093258B | 48 63 C1                       | movsxd rax,ecx                       |
000000015093258E | 8B 44 C4 04                    | mov eax,dword ptr ss:[rsp+rax*8+4]   |
0000000150932592 | 48 8B 8D 60 09 00 00           | mov rcx,qword ptr ss:[rbp+960]       |
0000000150932599 | 48 31 E1                       | xor rcx,rsp                          |
*/
static BYTE pattern_trophy_table[18] =
    "\x48\x63\xc1"
    "\x8b\x44\xc4\x04"
    "\x48\x8b\x8d\x60\x09\x00\x00"
    "\x48\x31\xe1";
static int offs_trophy_table = 30;

// ball name

/*
000000014D9A0703 | 80 79 04 00                          | cmp byte ptr ds:[rcx+4],0        | before ball name copy
000000014D9A0707 | 48 8D 51 04                          | lea rdx,qword ptr ds:[rcx+4]     | rdx:"Ordem V EPL"
000000014D9A070B | 75 12                                | jne pes2019.14D9A071F            |
000000014D9A070D | 45 31 C0                             | xor r8d,r8d                      |
000000014D9A0710 | 48 89 C1                             | mov rcx,rax                      |
...
000000014D9A071F | 49 83 C8 FF                          | or r8,FFFFFFFFFFFFFFFF           |
000000014D9A0723 | 49 FF C0                             | inc r8                           |
000000014D9A0726 | 42 80 3C 02 00                       | cmp byte ptr ds:[rdx+r8],0       |
000000014D9A072B | 75 F6                                | jne pes2019.14D9A0723            |
000000014D9A072D | 48 89 C1                             | mov rcx,rax                      | rcx:dst,rdx:src,r8:len
*/
static BYTE pattern_ball_name[17] =
    "\x80\x79\x04\x00"
    "\x48\x8d\x51\x04"
    "\x75\x12"
    "\x45\x31\xc0"
    "\x48\x89\xc1";
static int offs_ball_name = 28;
static BYTE pattern_ball_name_head[3] = "\x50\x50";
static BYTE pattern_ball_name_tail[4] = "\x58\x58\x90";

/*
00000001415BD4A0 | 48 33 C4                             | xor rax,rsp                            |
00000001415BD4A3 | 48 89 84 24 E0 01 00 00              | mov qword ptr ss:[rsp+1E0],rax         |
00000001415BD4AB | 48 8B F9                             | mov rdi,rcx                            |
00000001415BD4AE | 48 8D 54 24 30                       | lea rdx,qword ptr ss:[rsp+30]          |
*/
static BYTE pattern_dxgi[20] =
    "\x48\x33\xc4"
    "\x48\x89\x84\x24\xe0\x01\x00\x00"
    "\x48\x8b\xf9"
    "\x48\x8d\x54\x24\x30";
static int offs_dxgi = 0x1a;

/*
000000014136B3F0 | 88 51 32                             | mov byte ptr ds:[rcx+32],dl            | write stadium choice
000000014136B3F3 | C3                                   | ret                                    |
000000014136B3F4 | CC                                   | int3                                   |
000000014136B3F5 | CC                                   | int3                                   |
000000014136B3F6 | CC                                   | int3                                   |
000000014136B3F7 | CC                                   | int3                                   |
000000014136B3F8 | CC                                   | int3                                   |
000000014136B3F9 | CC                                   | int3                                   |
000000014136B3FA | CC                                   | int3                                   |
000000014136B3FB | CC                                   | int3                                   |
000000014136B3FC | CC                                   | int3                                   |
000000014136B3FD | CC                                   | int3                                   |
000000014136B3FE | CC                                   | int3                                   |
000000014136B3FF | CC                                   | int3                                   |
*/
static BYTE pattern_set_stadium_choice[17] =
    "\x88\x51\x32"
    "\xc3"
    "\xcc\xcc\xcc\xcc"
    "\xcc\xcc\xcc\xcc"
    "\xcc\xcc\xcc\xcc";
static int offs_set_stadium_choice = 0;
static BYTE pattern_set_stadium_choice_head[2] =
    "\x50";
static BYTE pattern_set_stadium_choice_tail[3] =
    "\x58\xc3";

/*
000000014CFE6503 | 80 79 08 00             | cmp byte ptr ds:[rcx+8],0              | before stadium name copy
000000014CFE6507 | 48 8D 51 08             | lea rdx,qword ptr ds:[rcx+8]           | rdx:"Allianz Parque"
000000014CFE650B | 75 12                   | jne pes2019.14CFE651F                  |
000000014CFE650D | 45 31 C0                | xor r8d,r8d                            |
000000014CFE6510 | 48 89 C1                | mov rcx,rax                            |
000000014CFE6513 | E8 48 BE 4C F3          | call pes2019.1404B2360                 |
000000014CFE6518 | B0 01                   | mov al,1                               |
000000014CFE651A | 48 83 C4 28             | add rsp,28                             |
000000014CFE651E | C3                      | ret                                    |
000000014CFE651F | 49 83 C8 FF             | or r8,FFFFFFFFFFFFFFFF                 |
000000014CFE6523 | 49 FF C0                | inc r8                                 |
000000014CFE6526 | 42 80 3C 02 00          | cmp byte ptr ds:[rdx+r8],0             | rdx+r8*1:"Allianz Parque"
000000014CFE652B | 75 F6                   | jne pes2019.14CFE6523                  |
000000014CFE652D | 48 89 C1                | mov rcx,rax                            |
*/
static BYTE pattern_stadium_name[17] =
    "\x80\x79\x08\x00"
    "\x48\x8d\x51\x08"
    "\x75\x12"
    "\x45\x31\xc0"
    "\x48\x89\xc1";
static int offs_stadium_name = 28;
static BYTE pattern_stadium_name_head[3] = "\x50\x50";
static BYTE pattern_stadium_name_tail[4] = "\x58\x58\x90";

/*
000000014D33447D | 48 85 C0                             | test rax,rax                           |
000000014D334480 | 74 0D                                | je pes2019.14D33448F                   |
000000014D334482 | 48 89 F2                             | mov rdx,rsi                            |
000000014D334485 | 48 89 C1                             | mov rcx,rax                            |
000000014D334488 | E8 63 C4 C4 F4                       | call pes2019.141F808F0                 |
000000014D33448D | EB 12                                | jmp pes2019.14D3344A1                  |
000000014D33448F | 45 31 C0                             | xor r8d,r8d                            |
000000014D334492 | 48 8D 15 E9 F0 1F F5                 | lea rdx,qword ptr ds:[142533582]       |
000000014D334499 | 48 89 F1                             | mov rcx,rsi                            |
000000014D33449C | E8 6F E9 17 F3                       | call pes2019.1404B2E10                 |
*/
static BYTE pattern_def_stadium_name[19] =
    "\x48\x85\xc0"
    "\x74\x0d"
    "\x48\x89\xf2"
    "\x48\x89\xc1"
    "\xe8\x63\xc4\xc4\xf4"
    "\xeb\x12";
static int offs_def_stadium_name = 3;
static BYTE pattern_def_stadium_name_head[3] = "\x75\x0c";
static BYTE pattern_def_stadium_name_tail[15] =
    "\x48\x8b\xd6"
    "\x48\x8b\xc8"
    "\xe8\x57\xc4\xc4\xf4"
    "\xeb\x06"
    "\x90";

#endif
