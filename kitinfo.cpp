#include "kitinfo.h"
#include "sider.h"

void str_to_rgb(BYTE *dst, char *src) {
    BYTE r1=0,r2=0;
    BYTE g1=0,g2=0;
    BYTE b1=0,b2=0;
    BYTE *bytes[6];
    bytes[0]=&r1; bytes[1]=&r2;
    bytes[2]=&g1; bytes[3]=&g2;
    bytes[4]=&b1; bytes[5]=&b2;
    for (int i=0; i<6; i++) {
        int j = i+1;
        BYTE *b = bytes[i];
        if (src[j]>='A' && src[j]<='F') { *b = (src[j]-'A')+10; }
        else if (src[j]>='a' && src[j]<='f') { *b = (src[j]-'a')+10; }
        else if (src[j]>='0' && src[j]<='9') { *b = src[j]-'0'; }
    }
    dst[0] = r1*16+r2;
    dst[1] = g1*16+g2;
    dst[2] = b1*16+b2;
}

void set_word_bits(void *vp, int value, int bit_from, int bit_to) {
    WORD *p = (WORD*)vp;
    int num_bits = bit_to - bit_from;
    WORD clear_mask = 0xffff;
    for (int i=bit_from; i<bit_to; i++) {
        clear_mask -= (1 << i);
    }
    //logu_("value = %d\n", value);
    //logu_("clear_mask = 0x%04x\n", clear_mask);
    // clear target bits
    //logu_("word was: 0x%04x\n", *p);
    *p &= clear_mask;
    //logu_("after clear mask: 0x%04x\n", *p);
    // clamp the value so that we don't overwrite
    // bits that are not ours
    WORD value_clamp_mask = 0x0000;
    for (int i=0; i<bit_to-bit_from; i++) {
        value_clamp_mask += (1 << i);
    }
    WORD w = value;
    w &= value_clamp_mask;
    // set clamped value
    *p |= w << bit_from;
    //logu_("word now: 0x%04x\n", *p);
}

int get_word_bits(void *vp, int bit_from, int bit_to) {
    WORD *p = (WORD*)vp;
    int num_bits = bit_to - bit_from;
    WORD clear_mask = 0x0000;
    for (int i=bit_from; i<bit_to; i++) {
        clear_mask += (1 << i);
    }
    //logu_("clear_mask = 0x%04x\n", clear_mask);
    // clear non-target bits
    WORD w = *p;
    //logu_("word was: 0x%04x\n", w);
    w &= clear_mask;
    //logu_("after clear mask: 0x%04x\n", w);
    // get desired bits
    int value = (int)(w >> bit_from);
    //logu_("value: %d\n", value);
    return value;
}

void set_kit_info_from_lua_table(lua_State *L, int index, BYTE *dst) {
    if (!dst) {
        // nothing to do
        return;
    }

    // shirt parameters
    /**
    ShortSleevesModel=1       ; 1=Normal
    ShirtModel=144       ; 144, 151=Semi-long short sleeves, 160, 176=Legacy
    Collar=123       ; 1 to 126
    TightKit=0       ; 0=Off, 1=On
    ShirtPattern=3       ; 0 to 6
    WinterCollar=123       ; 1 to 126
    LongSleevesType=62       ; 62=Normal & Undershirt, 187=Only Undershirt
    **/
    lua_getfield(L, index, "ShortSleevesModel");
    if (lua_isnumber(L, -1)) {
        dst[0] = luaL_checkinteger(L, -1);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "ShirtModel");
    if (lua_isnumber(L, -1)) {
        dst[1] = luaL_checkinteger(L, -1);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "LongSleevesType");
    if (lua_isnumber(L, -1)) {
        dst[2] = luaL_checkinteger(L, -1);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "Collar");
    if (lua_isnumber(L, -1)) {
        dst[0x14] = luaL_checkinteger(L, -1);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "TightKit");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x1a, luaL_checkinteger(L, -1), 15, 16);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "ShirtPattern");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x24, luaL_checkinteger(L, -1), 5, 8);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "WinterCollar");
    if (lua_isnumber(L, -1)) {
        dst[0x15] = luaL_checkinteger(L, -1);
    }
    lua_pop(L, 1);

    /**
    ; shirt - back side
    Name=0       ; 0=On, 1=Off
    NameShape=0       ; 0=Straight, 1=Light curve, 2=Medium curve, 3=Extreme curve
    NameY=15       ; 0 to 17
    NameSize=15       ; 0 to 31
    BackNumberY=21       ; 0 to 29
    BackNumberSize=13       ; 0 to 14
    BackNumberSpacing=1       ; 0 to 3
    **/
    lua_getfield(L, index, "Name");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x1e, luaL_checkinteger(L, -1), 0, 1);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "NameShape");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x1c, luaL_checkinteger(L, -1), 14, 16);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "NameY");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x1c, luaL_checkinteger(L, -1), 4, 9);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "NameSize");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x1c, luaL_checkinteger(L, -1), 9, 14);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "BackNumberY");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x18, luaL_checkinteger(L, -1), 0, 5);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "BackNumberSize");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x18, luaL_checkinteger(L, -1), 6, 10);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "BackNumberSpacing");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x18, luaL_checkinteger(L, -1), 12, 14);
    }
    lua_pop(L, 1);

    /**
    ; shirt - front side
    ChestNumberX=5       ; 0 to 31
    ChestNumberY=5       ; 0 to 7
    ChestNumberSize=14       ; 0 to 31
    **/
    lua_getfield(L, index, "ChestNumberX");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x1a, luaL_checkinteger(L, -1), 4, 8);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "ChestNumberY");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x1a, luaL_checkinteger(L, -1), 0, 3);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "ChestNumberSize");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x1a, luaL_checkinteger(L, -1), 8, 13);
    }
    lua_pop(L, 1);

    /**
    ; shorts parameters
    ShortsModel=2       ; 0 to 17
    ShortsNumberSide=0       ; 0=Left, 1=Right
    ShortsNumberX=9       ; 0 to 14
    ShortsNumberY=10       ; 0 to 15
    ShortsNumberSize=9       ; 0 to 31
    **/
    lua_getfield(L, index, "ShortsModel");
    if (lua_isnumber(L, -1)) {
        dst[3] = luaL_checkinteger(L, -1);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "ShortsNumberSide");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x16, luaL_checkinteger(L, -1), 15, 16);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "ShortsNumberX");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x16, luaL_checkinteger(L, -1), 6, 10);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "ShortsNumberY");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x16, luaL_checkinteger(L, -1), 0, 4);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "ShortsNumberSize");
    if (lua_isnumber(L, -1)) {
        set_word_bits(dst+0x16, luaL_checkinteger(L, -1), 11, 15);
    }
    lua_pop(L, 1);

    /**
    ; texture files
    KitFile=u0008p1
    BackNumbersFile=u0008p1_back
    ChestNumbersFile=u0008p1_chest
    LegNumbersFile=u0008p1_leg
    NameFontFile=u0008p1_name
    **/
    char *p = (char*)dst;
    lua_getfield(L, index, "KitFile");
    if (lua_isstring(L, -1)) {
        const char *s = luaL_checkstring(L, -1);
        memset(p+0x28, 0, 16);
        strncat(p+0x28, s, 15);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "BackNumbersFile");
    if (lua_isstring(L, -1)) {
        const char *s = luaL_checkstring(L, -1);
        memset(p+0x38, 0, 16);
        strncat(p+0x38, s, 15);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "ChestNumbersFile");
    if (lua_isstring(L, -1)) {
        const char *s = luaL_checkstring(L, -1);
        memset(p+0x48, 0, 16);
        strncat(p+0x48, s, 15);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "LegNumbersFile");
    if (lua_isstring(L, -1)) {
        const char *s = luaL_checkstring(L, -1);
        memset(p+0x58, 0, 16);
        strncat(p+0x58, s, 15);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "NameFontFile");
    if (lua_isstring(L, -1)) {
        const char *s = luaL_checkstring(L, -1);
        memset(p+0x68, 0, 16);
        strncat(p+0x68, s, 15);
    }
    lua_pop(L, 1);

    /**
    ; kit colors
    ShirtColor1=#141E32
    ShirtColor2=#141E32
    UndershirtColor=#2874B7
    ShortsColor=#D7D7D7
    SocksColor=#BD2835
    **/
    lua_getfield(L, index, "ShirtColor1");
    if (lua_isstring(L, -1)) {
        char s[8];
        memset(s,0,8);
        strncat(s, luaL_checkstring(L, -1), 7);
        str_to_rgb(dst+4, s);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "ShirtColor2");
    if (lua_isstring(L, -1)) {
        char s[8];
        memset(s,0,8);
        strncat(s, luaL_checkstring(L, -1), 7);
        str_to_rgb(dst+7, s);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "ShortsColor");
    if (lua_isstring(L, -1)) {
        char s[8];
        memset(s,0,8);
        strncat(s, luaL_checkstring(L, -1), 7);
        str_to_rgb(dst+0x0a, s);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "SocksColor");
    if (lua_isstring(L, -1)) {
        char s[8];
        memset(s,0,8);
        strncat(s, luaL_checkstring(L, -1), 7);
        str_to_rgb(dst+0x0d, s);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "UndershirtColor");
    if (lua_isstring(L, -1)) {
        char s[8];
        memset(s,0,8);
        strncat(s, luaL_checkstring(L, -1), 7);
        str_to_rgb(dst+0x10, s);
    }
    lua_pop(L, 1);

    /**
    ; sleeve badge positions
    RightShortX=14       ; 0 to 31
    RightShortY=16       ; 0 to 31
    RightLongX=14       ; 0 to 31
    RightLongY=16       ; 0 to 31
    LeftShortX=14       ; 0 to 31
    LeftShortY=16       ; 0 to 31
    LeftLongX=14       ; 0 to 31
    LeftLongY=16       ; 0 to 31
    **/
    lua_getfield(L, index, "RightShortX");
    if (lua_isstring(L, -1)) {
        set_word_bits(dst+0x1e, luaL_checkinteger(L, -1), 12, 15);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "RightShortY");
    if (lua_isstring(L, -1)) {
        set_word_bits(dst+0x20, luaL_checkinteger(L, -1), 0, 5);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "RightLongX");
    if (lua_isstring(L, -1)) {
        set_word_bits(dst+0x22, luaL_checkinteger(L, -1), 0, 4);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "RightLongY");
    if (lua_isstring(L, -1)) {
        set_word_bits(dst+0x22, luaL_checkinteger(L, -1), 4, 9);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "LeftShortX");
    if (lua_isstring(L, -1)) {
        set_word_bits(dst+0x1e, luaL_checkinteger(L, -1), 2, 6);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "LeftShortY");
    if (lua_isstring(L, -1)) {
        set_word_bits(dst+0x1e, luaL_checkinteger(L, -1), 6, 11);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "LeftLongX");
    if (lua_isstring(L, -1)) {
        set_word_bits(dst+0x20, luaL_checkinteger(L, -1), 6, 10);
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "LeftLongY");
    if (lua_isstring(L, -1)) {
        set_word_bits(dst+0x20, luaL_checkinteger(L, -1), 10, 15);
    }
    lua_pop(L, 1);
}

void get_kit_info_to_lua_table(lua_State *L, int index, BYTE *src) {
    if (!src) {
        // nothing to do
        return;
    }

    index--; // adjust index, because we will be pushing values

    // shirt parameters
    /**
    ShortSleevesModel=1       ; 1=Normal
    ShirtModel=144       ; 144, 151=Semi-long short sleeves, 160, 176=Legacy
    Collar=123       ; 1 to 126
    TightKit=0       ; 0=Off, 1=On
    ShirtPattern=3       ; 0 to 6
    WinterCollar=123       ; 1 to 126
    LongSleevesType=62       ; 62=Normal & Undershirt, 187=Only Undershirt
    **/
    lua_pushinteger(L, src[0]);
    lua_setfield(L, index, "ShortSleevesModel");
    lua_pushinteger(L, src[1]);
    lua_setfield(L, index, "ShirtModel");
    lua_pushinteger(L, src[2]);
    lua_setfield(L, index, "LongSleevesType");
    lua_pushinteger(L, src[0x14]);
    lua_setfield(L, index, "Collar");
    lua_pushinteger(L, get_word_bits(src+0x1a, 15, 16));
    lua_setfield(L, index, "TightKit");
    lua_pushinteger(L, get_word_bits(src+0x24, 5, 8));
    lua_setfield(L, index, "ShirtPattern");
    lua_pushinteger(L, src[0x15]);
    lua_setfield(L, index, "WinterCollar");

    /**
    ; shirt - back side
    Name=0       ; 0=On, 1=Off
    NameShape=0       ; 0=Straight, 1=Light curve, 2=Medium curve, 3=Extreme curve
    NameY=15       ; 0 to 17
    NameSize=15       ; 0 to 31
    BackNumberY=21       ; 0 to 29
    BackNumberSize=13       ; 0 to 14
    BackNumberSpacing=1       ; 0 to 3
    **/
    lua_pushinteger(L, get_word_bits(src+0x1e, 0, 1));
    lua_setfield(L, index, "Name");
    lua_pushinteger(L, get_word_bits(src+0x1c, 14, 16));
    lua_setfield(L, index, "NameShape");
    lua_pushinteger(L, get_word_bits(src+0x1c, 4, 9));
    lua_setfield(L, index, "NameY");
    lua_pushinteger(L, get_word_bits(src+0x1c, 9, 14));
    lua_setfield(L, index, "NameSize");
    lua_pushinteger(L, get_word_bits(src+0x18, 0, 5));
    lua_setfield(L, index, "BackNumberY");
    lua_pushinteger(L, get_word_bits(src+0x18, 6, 10));
    lua_setfield(L, index, "BackNumberSize");
    lua_pushinteger(L, get_word_bits(src+0x18, 12, 14));
    lua_setfield(L, index, "BackNumberSpacing");

    /**
    ; shirt - front side
    ChestNumberX=5       ; 0 to 31
    ChestNumberY=5       ; 0 to 7
    ChestNumberSize=14       ; 0 to 31
    **/
    lua_pushinteger(L, get_word_bits(src+0x1a, 4, 8));
    lua_setfield(L, index, "ChestNumberX");
    lua_pushinteger(L, get_word_bits(src+0x1a, 0, 3));
    lua_setfield(L, index, "ChestNumberY");
    lua_pushinteger(L, get_word_bits(src+0x1a, 8, 13));
    lua_setfield(L, index, "ChestNumberSize");

    /**
    ; shorts parameters
    ShortsModel=2       ; 0 to 17
    ShortsNumberSide=0       ; 0=Left, 1=Right
    ShortsNumberX=9       ; 0 to 14
    ShortsNumberY=10       ; 0 to 15
    ShortsNumberSize=9       ; 0 to 31
    **/
    lua_pushinteger(L, src[3]);
    lua_setfield(L, index, "ShortsModel");
    lua_pushinteger(L, get_word_bits(src+0x16, 15, 16));
    lua_setfield(L, index, "ShortsNumberSide");
    lua_pushinteger(L, get_word_bits(src+0x16, 6, 10));
    lua_setfield(L, index, "ShortsNumberX");
    lua_pushinteger(L, get_word_bits(src+0x16, 0, 4));
    lua_setfield(L, index, "ShortsNumberY");
    lua_pushinteger(L, get_word_bits(src+0x16, 11, 15));
    lua_setfield(L, index, "ShortsNumberSize");

    /**
    ; texture files
    KitFile=u0008p1
    BackNumbersFile=u0008p1_back
    ChestNumbersFile=u0008p1_chest
    LegNumbersFile=u0008p1_leg
    NameFontFile=u0008p1_name
    **/
    char *p = (char*)src;
    lua_pushstring(L, p+0x28);
    lua_setfield(L, index, "KitFile");
    lua_pushstring(L, p+0x38);
    lua_setfield(L, index, "BackNumbersFile");
    lua_pushstring(L, p+0x48);
    lua_setfield(L, index, "ChestNumbersFile");
    lua_pushstring(L, p+0x58);
    lua_setfield(L, index, "LegNumbersFile");
    lua_pushstring(L, p+0x68);
    lua_setfield(L, index, "NameFontFile");

    /**
    ; kit colors
    ShirtColor1=#141E32
    ShirtColor2=#141E32
    UndershirtColor=#2874B7
    ShortsColor=#D7D7D7
    SocksColor=#BD2835
    **/
    lua_pushfstring(L, "#%02x%02x%02x", src[4], src[5], src[6]);
    lua_setfield(L, index, "ShirtColor1");
    lua_pushfstring(L, "#%02x%02x%02x", src[7], src[8], src[9]);
    lua_setfield(L, index, "ShirtColor2");
    lua_pushfstring(L, "#%02x%02x%02x", src[0x0a], src[0x0b], src[0x0c]);
    lua_setfield(L, index, "ShortsColor");
    lua_pushfstring(L, "#%02x%02x%02x", src[0x0d], src[0x0e], src[0x0f]);
    lua_setfield(L, index, "SocksColor");
    lua_pushfstring(L, "#%02x%02x%02x", src[0x10], src[0x11], src[0x12]);
    lua_setfield(L, index, "UndershirtColor");

    /**
    ; sleeve badge positions
    RightShortX=14       ; 0 to 31
    RightShortY=16       ; 0 to 31
    RightLongX=14       ; 0 to 31
    RightLongY=16       ; 0 to 31
    LeftShortX=14       ; 0 to 31
    LeftShortY=16       ; 0 to 31
    LeftLongX=14       ; 0 to 31
    LeftLongY=16       ; 0 to 31
    **/
    lua_pushinteger(L, get_word_bits(src+0x1e, 12, 15));
    lua_setfield(L, index, "RightShortX");
    lua_pushinteger(L, get_word_bits(src+0x20, 0, 5));
    lua_setfield(L, index, "RightShortY");
    lua_pushinteger(L, get_word_bits(src+0x22, 0, 4));
    lua_setfield(L, index, "RightLongX");
    lua_pushinteger(L, get_word_bits(src+0x22, 4, 9));
    lua_setfield(L, index, "RightLongY");
    lua_pushinteger(L, get_word_bits(src+0x1e, 2, 6));
    lua_setfield(L, index, "LeftShortX");
    lua_pushinteger(L, get_word_bits(src+0x1e, 6, 11));
    lua_setfield(L, index, "LeftShortY");
    lua_pushinteger(L, get_word_bits(src+0x20, 6, 10));
    lua_setfield(L, index, "LeftLongX");
    lua_pushinteger(L, get_word_bits(src+0x20, 10, 15));
    lua_setfield(L, index, "LeftLongY");
}
