
#include "patch.h"
#include "game.h"
#include "move.h"
#include "tech.h"
#include "wtp.h"

const char* ac_alpha = "ac_mod\\alphax";
const char* ac_help = "ac_mod\\helpx";
const char* ac_tutor = "ac_mod\\tutor";
const char* ac_concepts = "ac_mod\\conceptsx";
const char* ac_opening = "opening";
const char* ac_movlist = "movlist";
const char* ac_movlist_txt = "movlist.txt";
const char* ac_movlistx_txt = "movlistx.txt";

const char* lm_params[] = {
    "crater", "volcano", "jungle", "uranium",
    "sargasso", "ruins", "dunes", "fresh",
    "mesa", "canyon", "geothermal", "ridge",
    "borehole", "nexus", "unity", "fossil"
};

const byte asm_find_start[] = {
    0x8D,0x45,0xF8,0x50,0x8D,0x45,0xFC,0x50,0x8B,0x45,0x08,0x50,
    0xE8,0x00,0x00,0x00,0x00,0x83,0xC4,0x0C
};

int prev_rnd = -1;
Points spawns;
Points natives;
Points goodtiles;

bool FileExists(const char* path) {
    return GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES;
}

HOOK_API int crop_yield(int fac, int base, int x, int y, int tf) {
    int value = tx_crop_yield(fac, base, x, y, tf);
    MAP* sq = mapsq(x, y);
    if (sq && sq->items & TERRA_THERMAL_BORE) {
        value++;
    }
    return value;
}

void process_map(int k) {
    TileSearch ts;
    Points visited;
    Points current;
    natives.clear();
    goodtiles.clear();
    /* Map area square root values: Standard = 56, Huge = 90 */
    int limit = *tx_map_area_sq_root * (*tx_map_area_sq_root < 70 ? 1 : 2);

    for (int y = 0; y < *tx_map_axis_y; y++) {
        for (int x = y&1; x < *tx_map_axis_x; x+=2) {
            MAP* sq = mapsq(x, y);
            if (unit_in_tile(sq) == 0)
                natives.insert({x, y});
            if (y < 4 || y >= *tx_map_axis_x - 4)
                continue;
            if (sq && !is_ocean(sq) && !visited.count({x, y})) {
                int n = 0;
                ts.init(x, y, TRIAD_LAND, k);
                while ((sq = ts.get_next()) != NULL) {
                    auto p = mp(ts.rx, ts.ry);
                    visited.insert(p);
                    if (~sq->items & TERRA_FUNGUS && !(sq->landmarks & ~LM_FRESH)) {
                        current.insert(p);
                    }
                    n++;
                }
                if (n >= limit) {
                    for (auto& p : current) {
                        goodtiles.insert(p);
                    }
                }
                current.clear();
            }
        }
    }
    if ((int)goodtiles.size() < *tx_map_area_sq_root * 8) {
        goodtiles.clear();
    }
    debug("process_map %d %d %d %d %d\n", *tx_map_axis_x, *tx_map_axis_y,
        *tx_map_area_sq_root, visited.size(), goodtiles.size());
}

bool valid_start (int fac, int iter, int x, int y, bool aquatic) {
    Points pods;
    MAP* sq = mapsq(x, y);
    if (!sq || sq->items & BASE_DISALLOWED || (sq->rocks & TILE_ROCKY && !is_ocean(sq)))
        return false;
    if (sq->landmarks & ~LM_FRESH)
        return false;
    if (aquatic != is_ocean(sq) || tx_bonus_at(x, y) != RES_NONE)
        return false;
    if (point_range(natives, x, y) < 4)
        return false;
    int sc = 0;
    int nut = 0;
    for (int i=0; i<44; i++) {
        int x2 = wrap(x + offset_tbl[i][0]);
        int y2 = y + offset_tbl[i][1];
        sq = mapsq(x2, y2);
        if (sq) {
            int bonus = tx_bonus_at(x2, y2);
            if (is_ocean(sq)) {
                if (!is_ocean_shelf(sq))
                    sc--;
                else if (aquatic)
                    sc += 4;
            } else {
                sc += (sq->level & (TILE_RAINY | TILE_MOIST) ? 3 : 1);
                if (sq->items & TERRA_RIVER)
                    sc += (i < 20 ? 4 : 2);
                if (sq->rocks & TILE_ROLLING)
                    sc += 1;
            }
            if (bonus > 0) {
                if (i < 20 && bonus == RES_NUTRIENT && !is_ocean(sq))
                    nut++;
                sc += (i < 20 ? 10 : 5);
            }
            if (tx_goody_at(x2, y2) > 0) {
                sc += (i < 20 ? 25 : 8);
                if (!is_ocean(sq) && (i < 20 || pods.size() < 2))
                    pods.insert({x2, y2});
            }
            if (sq->items & TERRA_FUNGUS) {
                sc -= (i < 20 ? 4 : 1);
            }
        }
    }
    int min_sc = 160 - iter/2;
    bool need_bonus = (!aquatic && conf.nutrient_bonus > is_human(fac));
    debug("find_score %2d | %3d %3d | %d %d %d %d\n", iter, x, y, pods.size(), nut, min_sc, sc);
    if (sc >= min_sc && need_bonus && (int)pods.size() > 1 - iter/50) {
        int n = 0;
        while (!aquatic && pods.size() > 0 && nut + n < 2) {
            Points::const_iterator it(pods.begin());
            std::advance(it, random(pods.size()));
            sq = mapsq(it->first, it->second);
            pods.erase(it);
            sq->items &= ~(TERRA_FUNGUS | TERRA_MINERAL_RES | TERRA_ENERGY_RES);
            sq->items |= (TERRA_SUPPLY_REMOVE | TERRA_BONUS_RES | TERRA_NUTRIENT_RES);
            n++;
        }
        return true;
    }
    return sc >= min_sc && (!need_bonus || nut > 1);
}

HOOK_API void find_start(int fac, int* tx, int* ty) {
    bool aquatic = tx_factions_meta[fac].rule_flags & FACT_AQUATIC;
    int k = (*tx_map_axis_y < 80 ? 8 : 16);
    if (*tx_random_seed != prev_rnd) {
        process_map(k/2);
        prev_rnd = *tx_random_seed;
    }
    spawns.clear();
    for (int i=0; i<*tx_total_num_vehicles; i++) {
        VEH* v = &tx_vehicles[i];
        if (v->faction_id != 0 && v->faction_id != fac) {
            spawns.insert({v->x, v->y});
        }
    }
    int z = 0;
    int x = 0;
    int y = 0;
    int i = 0;
    while (++i < 120) {
        if (!aquatic && goodtiles.size() > 0) {
            Points::const_iterator it(goodtiles.begin());
            std::advance(it, random(goodtiles.size()));
            x = it->first;
            y = it->second;
        } else {
            y = (random(*tx_map_axis_y - k) + k/2);
            x = (random(*tx_map_axis_x) &~1) + (y&1);
        }
        int min_range = max(8, *tx_map_area_sq_root/3 - i/5);
        z = point_range(spawns, x, y);
        debug("find_iter %d %d | %3d %3d | %2d %2d\n", fac, i, x, y, min_range, z);
        if (z >= min_range && valid_start(fac, i, x, y, aquatic)) {
            *tx = x;
            *ty = y;
            break;
        }
    }
    tx_site_set(*tx, *ty, tx_world_site(*tx, *ty, 0));
    debug("find_start %d %d %d %d %d\n", fac, i, *tx, *ty, point_range(spawns, *tx, *ty));
    fflush(debug_log);
}

void write_call_ptr(int addr, int func) {
    if (*(byte*)addr != 0xE8 || abs(*(int*)(addr+1)) > (int)AC_IMAGE_LEN) {
        throw std::exception();
    }
    *(int*)(addr+1) = func - addr - 5;
}

void write_offset(int addr, const void* ofs) {
    if (*(byte*)addr != 0x68 || *(int*)(addr+1) < (int)AC_IMAGE_BASE) {
        throw std::exception();
    }
    *(int*)(addr+1) = (int)ofs;
}

void remove_call(int addr) {
    if (*(byte*)addr != 0xE8) {
        throw std::exception();
    }
    memset((void*)addr, 0x90, 5);
}

/**
Replaces old byte sequence to new byte sequence at address.
Verifies that byte contains old values before replacing.
*/
void write_bytes(int address, int length, byte old_bytes[], byte new_bytes[]) {
    // check address is not out of range
    if ((byte*)address < (byte*)AC_IMAGE_BASE || (byte*)address + length > (byte*)AC_IMAGE_BASE + AC_IMAGE_LEN) {
        throw std::runtime_error("write_bytes: address is out of code range.");
    }

    // write new bytes at address
    for (int i = 0; i < length; i++)
    {
        // verify old byte match value at address
        if (*((byte*)address + i) != old_bytes[i]) {
            debug("write_bytes: old byte doesn't match at address: %p, byte at address: %x, byte expected: %x.", (byte*)address + i, *((byte*)address + i), old_bytes[i])
            throw std::runtime_error("write_bytes: old byte doesn't match.");
        }

        // write new byte
        *((byte*)address + i) = new_bytes[i];

    }

}

// ========================================
// The Will to Power patch procedures
// ========================================

/**
Patch battle compute wrapper.
*/
void patch_battle_compute()
{
    // wrap battle computation into custom function

    write_call_ptr(0x0050474C, (int)battle_compute);
    write_call_ptr(0x00506EA6, (int)battle_compute);
    write_call_ptr(0x005085E0, (int)battle_compute);

    // wrap string concatenation into custom function for battle computation output only

    write_call_ptr(0x00422915, (int)battle_compute_compose_value_percentage);

}

/**
Ignores reactor power multiplier in combat processing.
*/
void patch_ignore_reactor_power_in_combat_processing()
{
    // Ignore reactor power in combat processing
    // set up both units firepower as if it were psi combat

    // ignore defender reactor power

    int ignore_defender_reactor_power_combat_bytes_length = 1;

    /* jl */
    byte old_ignore_defender_reactor_power_combat_bytes[] = { 0x7C };

    /* jmp */
    byte new_ignore_defender_reactor_power_combat_bytes[] = { 0xEB };

    write_bytes(0x00506F90, ignore_defender_reactor_power_combat_bytes_length, old_ignore_defender_reactor_power_combat_bytes, new_ignore_defender_reactor_power_combat_bytes);

    // ignore attacker reactor power

    int ignore_attacker_reactor_power_combat_bytes_length = 1;

    /* jl */
    byte old_ignore_attacker_reactor_power_combat_bytes[] = { 0x7C };

    /* jmp */
    byte new_ignore_attacker_reactor_power_combat_bytes[] = { 0xEB };

    write_bytes(0x00506FF6, ignore_attacker_reactor_power_combat_bytes_length, old_ignore_attacker_reactor_power_combat_bytes, new_ignore_attacker_reactor_power_combat_bytes);

}

/**
Fixes combat roll formula to match odds.
*/
void patch_combat_roll()
{
    // replace code

    int combat_roll_bytes_length = 0x30;

    /*
    0:  8b 75 ec                mov    esi,DWORD PTR [ebp-0x14]
    3:  8d 46 ff                lea    eax,[esi-0x1]
    6:  85 c0                   test   eax,eax
    8:  7f 04                   jg     0xe
    a:  33 ff                   xor    edi,edi
    c:  eb 0a                   jmp    0x18
    e:  E8 65 CE 13 00          call   <randomizer>
    13: 99                      cdq
    14: f7 fe                   idiv   esi
    16: 8b fa                   mov    edi,edx
    18: 8b 75 e4                mov    esi,DWORD PTR [ebp-0x1c]
    1b: 8d 4e ff                lea    ecx,[esi-0x1]
    1e: 85 c9                   test   ecx,ecx
    20: 7f 04                   jg     0x26
    22: 33 d2                   xor    edx,edx
    24: eb 08                   jmp    0x2e
    26: E8 4D CE 13 00          call   <randomizer>
    2b: 99                      cdq
    2c: f7 fe                   idiv   esi
    2e: 3b fa                   cmp    edi,edx
    */
    byte combat_roll_old_bytes[] =
        { 0x8B, 0x75, 0xEC, 0x8D, 0x46, 0xFF, 0x85, 0xC0, 0x7F, 0x04, 0x33, 0xFF, 0xEB, 0x0A, 0xE8, 0x65, 0xCE, 0x13, 0x00, 0x99, 0xF7, 0xFE, 0x8B, 0xFA, 0x8B, 0x75, 0xE4, 0x8D, 0x4E, 0xFF, 0x85, 0xC9, 0x7F, 0x04, 0x33, 0xD2, 0xEB, 0x08, 0xE8, 0x4D, 0xCE, 0x13, 0x00, 0x99, 0xF7, 0xFE, 0x3B, 0xFA, 0x0F, 0x8E, 0x4E, 0x04, 0x00, 0x00 }
    ;

    /*
    0:  8d bd 10 ff ff ff       lea    edi,[ebp-0xf0]
    6:  57                      push   edi
    7:  8b 7d a4                mov    edi,DWORD PTR [ebp-0x5c]
    a:  57                      push   edi
    b:  8b 7d a0                mov    edi,DWORD PTR [ebp-0x60]
    e:  57                      push   edi
    f:  8b 7d f8                mov    edi,DWORD PTR [ebp-0x8]
    12: 57                      push   edi
    13: 8b 7d fc                mov    edi,DWORD PTR [ebp-0x4]
    16: 57                      push   edi
    17: 8b 7d e4                mov    edi,DWORD PTR [ebp-0x1c]
    1a: 57                      push   edi
    1b: 8b 7d ec                mov    edi,DWORD PTR [ebp-0x14]
    1e: 57                      push   edi
    1f: e8 fc ff ff ff          call   20 <_main+0x20>
    24: 83 c4 1c                add    esp,0x1c
    27: 85 c0                   test   eax,eax
    ...
    */
    byte combat_roll_new_bytes[] =
        { 0x8D, 0xBD, 0x10, 0xFF, 0xFF, 0xFF, 0x57, 0x8B, 0x7D, 0xA4, 0x57, 0x8B, 0x7D, 0xA0, 0x57, 0x8B, 0x7D, 0xF8, 0x57, 0x8B, 0x7D, 0xFC, 0x57, 0x8B, 0x7D, 0xE4, 0x57, 0x8B, 0x7D, 0xEC, 0x57, 0xE8, 0xFC, 0xFF, 0xFF, 0xFF, 0x83, 0xC4, 0x1C, 0x85, 0xC0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x005091A5,
        combat_roll_bytes_length,
        combat_roll_old_bytes,
        combat_roll_new_bytes
    )
    ;

    // set call pointer to custom combat_roll function

    write_call_ptr(0x005091A5 + 0x1f, (int)combat_roll);

}

/**
Calculates odds.
*/
void patch_calculate_odds()
{
    // replace calculate odds code

    int calculate_odds_bytes_length = 0xC3;

    /*
    0:  8b 5d fc                mov    ebx,DWORD PTR [ebp-0x4]
    3:  83 c4 18                add    esp,0x18
    6:  0f bf 83 32 28 95 00    movsx  eax,WORD PTR [ebx+0x952832]
    d:  8d 0c 40                lea    ecx,[eax+eax*2]
    10: 8d 04 88                lea    eax,[eax+ecx*4]
    13: c1 e0 02                shl    eax,0x2
    16: 80 b8 92 b8 9a 00 0c    cmp    BYTE PTR [eax+0x9ab892],0xc
    1d: 75 07                   jne    0x26
    1f: b8 01 00 00 00          mov    eax,0x1
    24: eb 1c                   jmp    0x42
    26: 33 d2                   xor    edx,edx
    28: 6a 64                   push   0x64
    2a: 8a 90 8f b8 9a 00       mov    dl,BYTE PTR [eax+0x9ab88f]
    30: 6a 01                   push   0x1
    32: 8b c2                   mov    eax,edx
    34: 50                      push   eax
    35: e8 92 ae f1 ff          call   0xfff1aecc
    3a: 8d 04 80                lea    eax,[eax+eax*4]
    3d: 83 c4 0c                add    esp,0xc
    40: d1 e0                   shl    eax,1
    42: 33 c9                   xor    ecx,ecx
    44: 8a 8b 38 28 95 00       mov    cl,BYTE PTR [ebx+0x952838]
    4a: 2b c1                   sub    eax,ecx
    4c: 78 0e                   js     0x5c
    4e: 3d 0f 27 00 00          cmp    eax,0x270f
    53: be 0f 27 00 00          mov    esi,0x270f
    58: 7f 02                   jg     0x5c
    5a: 8b f0                   mov    esi,eax
    5c: 8b 7d f8                mov    edi,DWORD PTR [ebp-0x8]
    5f: 0f af 75 ec             imul   esi,DWORD PTR [ebp-0x14]
    63: 0f bf 87 32 28 95 00    movsx  eax,WORD PTR [edi+0x952832]
    6a: 8d 14 40                lea    edx,[eax+eax*2]
    6d: 8d 04 90                lea    eax,[eax+edx*4]
    70: c1 e0 02                shl    eax,0x2
    73: 80 b8 92 b8 9a 00 0c    cmp    BYTE PTR [eax+0x9ab892],0xc
    7a: 75 07                   jne    0x83
    7c: b8 01 00 00 00          mov    eax,0x1
    81: eb 1c                   jmp    0x9f
    83: 33 c9                   xor    ecx,ecx
    85: 6a 64                   push   0x64
    87: 8a 88 8f b8 9a 00       mov    cl,BYTE PTR [eax+0x9ab88f]
    8d: 6a 01                   push   0x1
    8f: 8b c1                   mov    eax,ecx
    91: 50                      push   eax
    92: e8 35 ae f1 ff          call   0xfff1aecc
    97: 8d 04 80                lea    eax,[eax+eax*4]
    9a: 83 c4 0c                add    esp,0xc
    9d: d1 e0                   shl    eax,1
    9f: 33 d2                   xor    edx,edx
    a1: 8a 97 38 28 95 00       mov    dl,BYTE PTR [edi+0x952838]
    a7: 2b c2                   sub    eax,edx
    a9: 78 12                   js     0xbd
    ab: 3d 0f 27 00 00          cmp    eax,0x270f
    b0: 7e 07                   jle    0xb9
    b2: bf 0f 27 00 00          mov    edi,0x270f
    b7: eb 06                   jmp    0xbf
    b9: 8b f8                   mov    edi,eax
    bb: eb 02                   jmp    0xbf
    bd: 33 ff                   xor    edi,edi
    bf: 0f af 7d e4             imul   edi,DWORD PTR [ebp-0x1c]
    */
    byte calculate_odds_old_bytes[] =
        { 0x8B, 0x5D, 0xFC, 0x83, 0xC4, 0x18, 0x0F, 0xBF, 0x83, 0x32, 0x28, 0x95, 0x00, 0x8D, 0x0C, 0x40, 0x8D, 0x04, 0x88, 0xC1, 0xE0, 0x02, 0x80, 0xB8, 0x92, 0xB8, 0x9A, 0x00, 0x0C, 0x75, 0x07, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xEB, 0x1C, 0x33, 0xD2, 0x6A, 0x64, 0x8A, 0x90, 0x8F, 0xB8, 0x9A, 0x00, 0x6A, 0x01, 0x8B, 0xC2, 0x50, 0xE8, 0x92, 0xAE, 0xF1, 0xFF, 0x8D, 0x04, 0x80, 0x83, 0xC4, 0x0C, 0xD1, 0xE0, 0x33, 0xC9, 0x8A, 0x8B, 0x38, 0x28, 0x95, 0x00, 0x2B, 0xC1, 0x78, 0x0E, 0x3D, 0x0F, 0x27, 0x00, 0x00, 0xBE, 0x0F, 0x27, 0x00, 0x00, 0x7F, 0x02, 0x8B, 0xF0, 0x8B, 0x7D, 0xF8, 0x0F, 0xAF, 0x75, 0xEC, 0x0F, 0xBF, 0x87, 0x32, 0x28, 0x95, 0x00, 0x8D, 0x14, 0x40, 0x8D, 0x04, 0x90, 0xC1, 0xE0, 0x02, 0x80, 0xB8, 0x92, 0xB8, 0x9A, 0x00, 0x0C, 0x75, 0x07, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xEB, 0x1C, 0x33, 0xC9, 0x6A, 0x64, 0x8A, 0x88, 0x8F, 0xB8, 0x9A, 0x00, 0x6A, 0x01, 0x8B, 0xC1, 0x50, 0xE8, 0x35, 0xAE, 0xF1, 0xFF, 0x8D, 0x04, 0x80, 0x83, 0xC4, 0x0C, 0xD1, 0xE0, 0x33, 0xD2, 0x8A, 0x97, 0x38, 0x28, 0x95, 0x00, 0x2B, 0xC2, 0x78, 0x12, 0x3D, 0x0F, 0x27, 0x00, 0x00, 0x7E, 0x07, 0xBF, 0x0F, 0x27, 0x00, 0x00, 0xEB, 0x06, 0x8B, 0xF8, 0xEB, 0x02, 0x33, 0xFF, 0x0F, 0xAF, 0x7D, 0xE4 }
    ;

    /*
    0:  83 c4 18                add    esp,0x18
    3:  89 e0                   mov    eax,esp
    5:  83 e8 04                sub    eax,0x4
    8:  50                      push   eax
    9:  89 e0                   mov    eax,esp
    b:  83 e8 04                sub    eax,0x4
    e:  50                      push   eax
    f:  8b 7d e4                mov    edi,DWORD PTR [ebp-0x1c]
    12: 57                      push   edi
    13: 8b 75 ec                mov    esi,DWORD PTR [ebp-0x14]
    16: 56                      push   esi
    17: 8b 7d f8                mov    edi,DWORD PTR [ebp-0x8]
    1a: 57                      push   edi
    1b: 8b 5d fc                mov    ebx,DWORD PTR [ebp-0x4]
    1e: 53                      push   ebx
    1f: e8 fc ff ff ff          call   20 <_main+0x20>
    24: 83 c4 10                add    esp,0x10
    27: 5e                      pop    esi
    28: 5f                      pop    edi
    ...
    */
    byte calculate_odds_new_bytes[] =
        { 0x83, 0xC4, 0x18, 0x89, 0xE0, 0x83, 0xE8, 0x04, 0x50, 0x89, 0xE0, 0x83, 0xE8, 0x04, 0x50, 0x8B, 0x7D, 0xE4, 0x57, 0x8B, 0x75, 0xEC, 0x56, 0x8B, 0x7D, 0xF8, 0x57, 0x8B, 0x5D, 0xFC, 0x53, 0xE8, 0xFC, 0xFF, 0xFF, 0xFF, 0x83, 0xC4, 0x10, 0x5E, 0x5F, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00508034,
        calculate_odds_bytes_length,
        calculate_odds_old_bytes,
        calculate_odds_new_bytes
    )
    ;

    // set call pointer to custom calculate_odds function

    write_call_ptr(0x00508034 + 0x1f, (int)calculate_odds);

}

/**
Disables alien guaranteed technologies assignment.
*/
void patch_alien_guaranteed_technologies()
{
    // replace jz by !ALIEN condition with jmp unconditional

    int bytes_length = 1;

    /* jz */
    byte old_bytes[] = { 0x74 };

    /* jmp */
    byte new_bytes[] = { 0xEB };

    write_bytes(0x005B29F8, bytes_length, old_bytes, new_bytes);

}

/**
Assigns alternative base values for weapon icon selection algorithm.
*/
void patch_weapon_icon_selection_algorithm()
{
    // load weapon ID instead of weapon value for icon selection

    int load_weapon_value_bytes_length = 0xC;

    /*
    shl    eax,0x4
    cmp    esi,ebx
    movsx  ecx,BYTE PTR [eax+0x94ae68]
    */
    byte old_load_weapon_value_bytes[] =
        { 0xC1, 0xE0, 0x04, 0x3B, 0xF3, 0x0F, 0xBE, 0x88, 0x68, 0xAE, 0x94, 0x00 }
    ;

    /*
    mov    ecx,eax
    cmp    esi,ebx
    shl    eax,0x4
    nop
    nop
    nop
    nop
    nop
    */
    byte new_load_weapon_value_bytes[] =
        { 0x89, 0xC1, 0x39, 0xDE, 0xC1, 0xE0, 0x04, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x004C2E15,
        load_weapon_value_bytes_length,
        old_load_weapon_value_bytes,
        new_load_weapon_value_bytes
    )
    ;

    // rearrange switch table to match weapon ID instead of weapon value

    int weapon_icon_selection_table_bytes_length = 0x8D;

    /*
    or      eax, 0xFFFFFFFF
    push    ebx
    mov     DWORD PTR [ebp-0x68], eax
    mov     DWORD PTR [ebp-0x64], eax
    mov     eax, 2
    push    esi
    mov     DWORD PTR [ebp-0x50], eax
    mov     DWORD PTR [ebp-0x4C], eax
    mov     eax, 4
    push    edi
    mov     DWORD PTR [ebp-0x48], eax
    mov     DWORD PTR [ebp-0x44], eax
    mov     eax, 9
    mov     ecx, 8
    mov     DWORD PTR [ebp-0x18], eax
    mov     DWORD PTR [ebp-0x14], eax
    mov     DWORD PTR [ebp-0x10], eax
    mov     DWORD PTR [ebp-0xC], eax
    mov     eax, [ebp+0xC]
    xor     ebx, ebx
    mov     edx, 5
    mov     edi, 7
    cmp     eax, ecx
    mov     DWORD PTR [ebp-0x60], ebx
    mov     DWORD PTR [ebp-0x5C], ebx
    mov     DWORD PTR [ebp-0x58], 1
    mov     DWORD PTR [ebp-0x54], 3
    mov     DWORD PTR [ebp-0x40], edx
    mov     DWORD PTR [ebp-0x3C], edx
    mov     DWORD PTR [ebp-0x38], 6
    mov     DWORD PTR [ebp-0x34], edi
    mov     DWORD PTR [ebp-0x30], edi
    mov     DWORD PTR [ebp-0x2C], edi
    mov     DWORD PTR [ebp-0x28], ecx
    mov     DWORD PTR [ebp-0x24], ecx
    mov     DWORD PTR [ebp-0x20], ecx
    mov     DWORD PTR [ebp-0x1C], ecx
    mov     DWORD PTR [ebp-0x8], 0xA
    mov     DWORD PTR [ebp-0x4], 0xD
    */
    byte old_weapon_icon_selection_table_bytes[] =
        { 0x83, 0xC8, 0xFF, 0x53, 0x89, 0x45, 0x98, 0x89, 0x45, 0x9C, 0xB8, 0x02, 0x00, 0x00, 0x00, 0x56, 0x89, 0x45, 0xB0, 0x89, 0x45, 0xB4, 0xB8, 0x04, 0x00, 0x00, 0x00, 0x57, 0x89, 0x45, 0xB8, 0x89, 0x45, 0xBC, 0xB8, 0x09, 0x00, 0x00, 0x00, 0xB9, 0x08, 0x00, 0x00, 0x00, 0x89, 0x45, 0xE8, 0x89, 0x45, 0xEC, 0x89, 0x45, 0xF0, 0x89, 0x45, 0xF4, 0x8B, 0x45, 0x0C, 0x33, 0xDB, 0xBA, 0x05, 0x00, 0x00, 0x00, 0xBF, 0x07, 0x00, 0x00, 0x00, 0x3B, 0xC1, 0x89, 0x5D, 0xA0, 0x89, 0x5D, 0xA4, 0xC7, 0x45, 0xA8, 0x01, 0x00, 0x00, 0x00, 0xC7, 0x45, 0xAC, 0x03, 0x00, 0x00, 0x00, 0x89, 0x55, 0xC0, 0x89, 0x55, 0xC4, 0xC7, 0x45, 0xC8, 0x06, 0x00, 0x00, 0x00, 0x89, 0x7D, 0xCC, 0x89, 0x7D, 0xD0, 0x89, 0x7D, 0xD4, 0x89, 0x4D, 0xD8, 0x89, 0x4D, 0xDC, 0x89, 0x4D, 0xE0, 0x89, 0x4D, 0xE4, 0xC7, 0x45, 0xF8, 0x0A, 0x00, 0x00, 0x00, 0xC7, 0x45, 0xFC, 0x0D, 0x00, 0x00, 0x00 }
    ;

    /*
    or     eax,0xffffffff
    mov    DWORD PTR [ebp-0x38],eax
    mov    DWORD PTR [ebp-0x34],eax
    mov    DWORD PTR [ebp-0x2c],eax
    mov    DWORD PTR [ebp-0x28],eax
    mov    DWORD PTR [ebp-0x24],eax
    mov    DWORD PTR [ebp-0x20],eax
    mov    DWORD PTR [ebp-0x1c],eax
    mov    DWORD PTR [ebp-0x18],eax
    mov    DWORD PTR [ebp-0x14],eax
    mov    DWORD PTR [ebp-0x10],eax
    mov    DWORD PTR [ebp-0xc],eax
    mov    DWORD PTR [ebp-0x8],eax
    mov    DWORD PTR [ebp-0x4],eax
    mov    DWORD PTR [ebp-0x68],eax
    inc    eax
    mov    DWORD PTR [ebp-0x64],eax
    inc    eax
    mov    DWORD PTR [ebp-0x60],eax
    inc    eax
    mov    DWORD PTR [ebp-0x58],eax
    inc    eax
    mov    DWORD PTR [ebp-0x5c],eax
    inc    eax
    mov    DWORD PTR [ebp-0x54],eax
    inc    eax
    mov    DWORD PTR [ebp-0x50],eax
    inc    eax
    mov    DWORD PTR [ebp-0x4c],eax
    inc    eax
    mov    DWORD PTR [ebp-0x48],eax
    inc    eax
    mov    DWORD PTR [ebp-0x44],eax
    inc    eax
    mov    DWORD PTR [ebp-0x40],eax
    inc    eax
    mov    DWORD PTR [ebp-0x3c],eax
    mov    DWORD PTR [ebp-0x30],0xd
    push   ebx
    push   esi
    push   edi
    xor    ebx,ebx
    mov    edx,0x5
    mov    edi,0x7
    mov    eax,DWORD PTR [ebp+0xc]
    mov    ecx,0x8
    cmp    eax,ecx
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    */
    byte new_weapon_icon_selection_table_bytes[] =
        { 0x83, 0xC8, 0xFF, 0x89, 0x45, 0xC8, 0x89, 0x45, 0xCC, 0x89, 0x45, 0xD4, 0x89, 0x45, 0xD8, 0x89, 0x45, 0xDC, 0x89, 0x45, 0xE0, 0x89, 0x45, 0xE4, 0x89, 0x45, 0xE8, 0x89, 0x45, 0xEC, 0x89, 0x45, 0xF0, 0x89, 0x45, 0xF4, 0x89, 0x45, 0xF8, 0x89, 0x45, 0xFC, 0x89, 0x45, 0x98, 0x40, 0x89, 0x45, 0x9C, 0x40, 0x89, 0x45, 0xA0, 0x40, 0x89, 0x45, 0xA8, 0x40, 0x89, 0x45, 0xA4, 0x40, 0x89, 0x45, 0xAC, 0x40, 0x89, 0x45, 0xB0, 0x40, 0x89, 0x45, 0xB4, 0x40, 0x89, 0x45, 0xB8, 0x40, 0x89, 0x45, 0xBC, 0x40, 0x89, 0x45, 0xC0, 0x40, 0x89, 0x45, 0xC4, 0xC7, 0x45, 0xD0, 0x0D, 0x00, 0x00, 0x00, 0x53, 0x56, 0x57, 0x31, 0xDB, 0xBA, 0x05, 0x00, 0x00, 0x00, 0xBF, 0x07, 0x00, 0x00, 0x00, 0x8B, 0x45, 0x0C, 0xB9, 0x08, 0x00, 0x00, 0x00, 0x39, 0xC8, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x004C2CF6,
        weapon_icon_selection_table_bytes_length,
        old_weapon_icon_selection_table_bytes,
        new_weapon_icon_selection_table_bytes
    )
    ;

}

/**
Enables alternative prototype cost formula.
*/
void patch_alternative_prototype_cost_formula()
{
    write_call_ptr(0x00436ADD, (int)proto_cost);
    write_call_ptr(0x0043704C, (int)proto_cost);
    write_call_ptr(0x005817C9, (int)proto_cost);
    write_call_ptr(0x00581833, (int)proto_cost);
    write_call_ptr(0x00581BB3, (int)proto_cost);
    write_call_ptr(0x00581BCB, (int)proto_cost);
    write_call_ptr(0x00582339, (int)proto_cost);
    write_call_ptr(0x00582359, (int)proto_cost);
    write_call_ptr(0x00582378, (int)proto_cost);
    write_call_ptr(0x00582398, (int)proto_cost);
    write_call_ptr(0x005823B0, (int)proto_cost);
    write_call_ptr(0x00582482, (int)proto_cost);
    write_call_ptr(0x0058249A, (int)proto_cost);
    write_call_ptr(0x0058254A, (int)proto_cost);
    write_call_ptr(0x005827E4, (int)proto_cost);
    write_call_ptr(0x00582EC5, (int)proto_cost);
    write_call_ptr(0x00582FEC, (int)proto_cost);
    write_call_ptr(0x005A5D35, (int)proto_cost);
    write_call_ptr(0x005A5F15, (int)proto_cost);

}

/**
Disables hurry penalty threshold.
*/
void patch_disable_hurry_penalty_threshold()
{
    int disable_hurry_penalty_threshold_bytes_length = 0x6;

    /*
    cmp     esi, dword_949830
    */
    byte old_disable_hurry_penalty_threshold_bytes[] =
        { 0x3B, 0x35, 0x30, 0x98, 0x94, 0x00 }
    ;

    /*
    cmp     esi, 0x0
    nop
    nop
    nop
    */
    byte new_disable_hurry_penalty_threshold_bytes[] =
        { 0x83, 0xFE, 0x00, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00419007,
        disable_hurry_penalty_threshold_bytes_length,
        old_disable_hurry_penalty_threshold_bytes,
        new_disable_hurry_penalty_threshold_bytes
    )
    ;

    int disable_sp_hurry_penalty_threshold_bytes_length = 0x3;

    /*
    shl     eax, 2
    */
    byte old_disable_sp_hurry_penalty_threshold_bytes[] =
        { 0xC1, 0xE0, 0x02 }
    ;

    /*
    xor     eax, eax
    nop
    */
    byte new_disable_sp_hurry_penalty_threshold_bytes[] =
        { 0x31, 0xC0, 0x90 }
    ;

    write_bytes
    (
        0x00418FFB,
        disable_sp_hurry_penalty_threshold_bytes_length,
        old_disable_sp_hurry_penalty_threshold_bytes,
        new_disable_sp_hurry_penalty_threshold_bytes
    )
    ;

}

/**
Enables alternative unit hurry cost formula.
*/
void patch_alternative_unit_hurry_formula()
{
    int alternative_unit_hurry_formula_bytes_length = 0x3;

    /*
    lea    edi,[edx+ecx*2]
    */
    byte old_alternative_unit_hurry_formula_bytes[] =
        { 0x8D, 0x3C, 0x4A }
    ;

    /*
    lea    edi,[edi+ecx*4]
    */
    byte new_alternative_unit_hurry_formula_bytes[] =
        { 0x8D, 0x3C, 0x8F }
    ;

    write_bytes
    (
        0x00418FD4,
        alternative_unit_hurry_formula_bytes_length,
        old_alternative_unit_hurry_formula_bytes,
        new_alternative_unit_hurry_formula_bytes
    )
    ;

}

/**
Enables alternative upgrade cost formula.
*/
void patch_alternative_upgrade_cost_formula()
{
    write_call_ptr(0x004D0ECF, (int)upgrade_cost);
    write_call_ptr(0x004D16D9, (int)upgrade_cost);
    write_call_ptr(0x004EFB76, (int)upgrade_cost);
    write_call_ptr(0x004EFEB9, (int)upgrade_cost);

}

/**
Sets Perimeter Defense and Tachyon Field defense bonus.
*/
void patch_defensive_structures_bonus(int perimeter_defense_multiplier, int tachyon_field_bonus)
{
    int perimeter_defense_multiplier_bytes_length = 0x5;

    /*
    mov     esi, 4
    */
    byte old_perimeter_defense_multiplier_bytes[] =
        { 0xBE, 0x04, 0x00, 0x00, 0x00 }
    ;

    /*
    mov     esi, ?
    */
    byte new_perimeter_defense_multiplier_bytes[] =
        { 0xBE, (byte)perimeter_defense_multiplier, 0x00, 0x00, 0x00 }
    ;

    write_bytes
    (
        0x005034B9,
        perimeter_defense_multiplier_bytes_length,
        old_perimeter_defense_multiplier_bytes,
        new_perimeter_defense_multiplier_bytes
    )
    ;

    int tachyon_field_bonus_bytes_length = 0x3;

    /*
    add     esi, 2
    */
    byte old_tachyon_field_bonus_bytes[] =
        { 0x83, 0xC6, 0x02 }
    ;

    /*
    add     esi, ?
    */
    byte new_tachyon_field_bonus_bytes[] =
        { 0x83, 0xC6, (byte)tachyon_field_bonus }
    ;

    write_bytes
    (
        0x00503506,
        tachyon_field_bonus_bytes_length,
        old_tachyon_field_bonus_bytes,
        new_tachyon_field_bonus_bytes
    )
    ;

}

/**
Uses defender reactor power for collateral damage computation.
*/
void patch_collateral_damage_defender_reactor()
{
    int collateral_damage_defender_reactor_bytes_length = 0x2;

    /*
    jge     short loc_50AA9F
    */
    byte old_collateral_damage_defender_reactor_bytes[] =
        { 0x7D, 0x16 }
    ;

    /*
    nop
    nop
    */
    byte new_collateral_damage_defender_reactor_bytes[] =
        { 0x90, 0x90 }
    ;

    write_bytes
    (
        0x0050AA87,
        collateral_damage_defender_reactor_bytes_length,
        old_collateral_damage_defender_reactor_bytes,
        new_collateral_damage_defender_reactor_bytes
    )
    ;

}

/**
Sets collateral damage value.
*/
void patch_collateral_damage_value(int collateral_damage_value)
{
    int collateral_damage_value_bytes_length = 0x2;

    /*
    mov     dl, 3
    */
    byte old_collateral_damage_value_bytes[] =
        { 0xB2, 0x03 }
    ;

    /*
    mov     dl, ?
    */
    byte new_collateral_damage_value_bytes[] =
        { 0xB2, (byte)collateral_damage_value }
    ;

    write_bytes
    (
        0x0050AAA5,
        collateral_damage_value_bytes_length,
        old_collateral_damage_value_bytes,
        new_collateral_damage_value_bytes
    )
    ;

}

/**
Disables AQUATIC bonus minerals.
*/
void patch_disable_aquatic_bonus_minerals()
{
    int disable_aquatic_bonus_minerals_bytes_length = 0x1;

    /*
    inc     esi
    */
    byte old_disable_aquatic_bonus_minerals_bytes[] =
        { 0x46 }
    ;

    /*
    nop
    */
    byte new_disable_aquatic_bonus_minerals_bytes[] =
        { 0x90 }
    ;

    write_bytes
    (
        0x004E7604,
        disable_aquatic_bonus_minerals_bytes_length,
        old_disable_aquatic_bonus_minerals_bytes,
        new_disable_aquatic_bonus_minerals_bytes
    )
    ;

}

/**
Following methods modify repair rates.
*/
void patch_repair_minimal(int repair_minimal)
{
    int repair_minimal_bytes_length = 0x7;

    /*
    0:  c7 45 fc 01 00 00 00    mov    DWORD PTR [ebp-0x4],0x1
    */
    byte repair_minimal_bytes_old[] =
        { 0xC7, 0x45, 0xFC, 0x01, 0x00, 0x00, 0x00 }
    ;

    /*
    0:  c7 45 fc 01 00 00 00    mov    DWORD PTR [ebp-0x4],<repair_minimal>
    */
    byte repair_minimal_bytes_new[] =
        { 0xC7, 0x45, 0xFC, (byte)repair_minimal, 0x00, 0x00, 0x00 }
    ;

    write_bytes
    (
        0x00526200,
        repair_minimal_bytes_length,
        repair_minimal_bytes_old,
        repair_minimal_bytes_new
    )
    ;

}
void patch_repair_fungus(int repair_fungus)
{
    int repair_fungus_bytes_length = 0x7;

    /*
    0:  c7 45 fc 02 00 00 00    mov    DWORD PTR [ebp-0x4],0x2
    */
    byte repair_fungus_bytes_old[] =
        { 0xC7, 0x45, 0xFC, 0x02, 0x00, 0x00, 0x00 }
    ;

    /*
    0:  c7 45 fc 02 00 00 00    mov    DWORD PTR [ebp-0x4],<repair_fungus>
    */
    byte repair_fungus_bytes_new[] =
        { 0xC7, 0x45, 0xFC, (byte)repair_fungus, 0x00, 0x00, 0x00 }
    ;

    write_bytes
    (
        0x00526261,
        repair_fungus_bytes_length,
        repair_fungus_bytes_old,
        repair_fungus_bytes_new
    )
    ;

}
void patch_repair_friendly()
{
    int repair_friendly_bytes_length = 0x3;

    /*
    0:  ff 45 fc                inc    DWORD PTR [ebp-0x4]
    */
    byte repair_friendly_bytes_old[] =
        { 0xFF, 0x45, 0xFC }
    ;

    /*
    ...
    */
    byte repair_friendly_bytes_new[] =
        { 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00526284,
        repair_friendly_bytes_length,
        repair_friendly_bytes_old,
        repair_friendly_bytes_new
    )
    ;

}
void patch_repair_airbase()
{
    int repair_airbase_bytes_length = 0x3;

    /*
    0:  ff 45 fc                inc    DWORD PTR [ebp-0x4]
    */
    byte repair_airbase_bytes_old[] =
        { 0xFF, 0x45, 0xFC }
    ;

    /*
    ...
    */
    byte repair_airbase_bytes_new[] =
        { 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x005262D4,
        repair_airbase_bytes_length,
        repair_airbase_bytes_old,
        repair_airbase_bytes_new
    )
    ;

}
void patch_repair_bunker()
{
    int repair_bunker_bytes_length = 0x3;

    /*
    0:  ff 45 fc                inc    DWORD PTR [ebp-0x4]
    */
    byte repair_bunker_bytes_old[] =
        { 0xFF, 0x45, 0xFC }
    ;

    /*
    ...
    */
    byte repair_bunker_bytes_new[] =
        { 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x005262F5,
        repair_bunker_bytes_length,
        repair_bunker_bytes_old,
        repair_bunker_bytes_new
    )
    ;

}
void patch_repair_base(int repair_base)
{
    int repair_base_bytes_length = 0x10;

    /*
    0:  8b 45 fc                mov    eax,DWORD PTR [ebp-0x4]
    3:  66 8b 8e 32 28 95 00    mov    cx,WORD PTR [esi+0x952832]
    a:  40                      inc    eax
    b:  33 d2                   xor    edx,edx
    d:  89 45 fc                mov    DWORD PTR [ebp-0x4],eax
    */
    byte repair_base_bytes_old[] =
        { 0x8B, 0x45, 0xFC, 0x66, 0x8B, 0x8E, 0x32, 0x28, 0x95, 0x00, 0x40, 0x33, 0xD2, 0x89, 0x45, 0xFC }
    ;

    /*
    0:  66 8b 8e 32 28 95 00    mov    cx,WORD PTR [esi+0x952832]
    7:  31 d2                   xor    edx,edx
    9:  83 45 fc 01             add    DWORD PTR [ebp-0x4],0x1
    ...
    */
    byte repair_base_bytes_new[] =
        { 0x66, 0x8B, 0x8E, 0x32, 0x28, 0x95, 0x00, 0x31, 0xD2, 0x83, 0x45, 0xFC, (byte)repair_base, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x0052632E,
        repair_base_bytes_length,
        repair_base_bytes_old,
        repair_base_bytes_new
    )
    ;

}
void patch_repair_base_native(int repair_base_native)
{
    int repair_base_native_bytes_length = 0xB;

    /*
    0:  33 c9                   xor    ecx,ecx
    2:  8a 8e 38 28 95 00       mov    cl,BYTE PTR [esi+0x952838]
    8:  89 4d fc                mov    DWORD PTR [ebp-0x4],ecx
    */
    byte repair_base_native_bytes_old[] =
        { 0x33, 0xC9, 0x8A, 0x8E, 0x38, 0x28, 0x95, 0x00, 0x89, 0x4D, 0xFC }
    ;

    /*
    0:  83 45 fc 01             add    DWORD PTR [ebp-0x4],<repair_base_native>
    ...
    */
    byte repair_base_native_bytes_new[] =
        { 0x83, 0x45, 0xFC, (byte)repair_base_native, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00526376,
        repair_base_native_bytes_length,
        repair_base_native_bytes_old,
        repair_base_native_bytes_new
    )
    ;

}
void patch_repair_base_facility(int repair_base_facility)
{
    int repair_base_facility_bytes_length = 0xB;

    /*
    0:  33 d2                   xor    edx,edx
    2:  8a 96 38 28 95 00       mov    dl,BYTE PTR [esi+0x952838]
    8:  89 55 fc                mov    DWORD PTR [ebp-0x4],edx
    */
    byte repair_base_facility_bytes_old[] =
        { 0x33, 0xD2, 0x8A, 0x96, 0x38, 0x28, 0x95, 0x00, 0x89, 0x55, 0xFC }
    ;

    /*
    0:  83 45 fc 01             add    DWORD PTR [ebp-0x4],<repair_base_facility>
    */
    byte repair_base_facility_bytes_new[] =
        { 0x83, 0x45, 0xFC, (byte)repair_base_facility, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00526422,
        repair_base_facility_bytes_length,
        repair_base_facility_bytes_old,
        repair_base_facility_bytes_new
    )
    ;

}
void patch_repair_nano_factory(int repair_nano_factory)
{
    int repair_nano_factory_bytes_length = 0xB;

    /*
    0:  33 c9                   xor    ecx,ecx
    2:  8a 8e 38 28 95 00       mov    cl,BYTE PTR [esi+0x952838]
    8:  89 4d fc                mov    DWORD PTR [ebp-0x4],ecx
    */
    byte repair_nano_factory_bytes_old[] =
        { 0x33, 0xC9, 0x8A, 0x8E, 0x38, 0x28, 0x95, 0x00, 0x89, 0x4D, 0xFC }
    ;

    /*
    0:  83 45 fc 01             add    DWORD PTR [ebp-0x4],<repair_nano_factory>
    */
    byte repair_nano_factory_bytes_new[] =
        { 0x83, 0x45, 0xFC, (byte)repair_nano_factory, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00526540,
        repair_nano_factory_bytes_length,
        repair_nano_factory_bytes_old,
        repair_nano_factory_bytes_new
    )
    ;

}

/**
Disables planetpearls collection and corresponding message.
*/
void patch_disable_planetpearls()
{
    // do not collect planetpearls

    int disable_planetpearls_bytes_length = 0x6;

    /*
    0:  89 90 00 cc 96 00       mov    DWORD PTR [eax+0x96cc00],edx
    */
    byte disable_planetpearls_bytes_old[] =
        { 0x89, 0x90, 0x00, 0xCC, 0x96, 0x00 }
    ;

    /*
    ...
    */
    byte disable_planetpearls_bytes_new[] =
        { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x0050B22B,
        disable_planetpearls_bytes_length,
        disable_planetpearls_bytes_old,
        disable_planetpearls_bytes_new
    )
    ;

    // do not display planetpearls message

    int disable_planetpearls_message_bytes_length = 0x3;

    /*
    0:  8b 45 a8                mov    eax,DWORD PTR [ebp-0x58]
    */
    byte disable_planetpearls_message_bytes_old[] =
        { 0x8B, 0x45, 0xA8 }
    ;

    /*
    0:  31 c0                   xor    eax,eax
    ...
    */
    byte disable_planetpearls_message_bytes_new[] =
        { 0x31, 0xC0, 0x90 }
    ;

    write_bytes
    (
        0x0050B362,
        disable_planetpearls_message_bytes_length,
        disable_planetpearls_message_bytes_old,
        disable_planetpearls_message_bytes_new
    )
    ;

}

/**
Makes promotions probabilities uniform for all levels.
*/
void patch_uniform_promotions()
{
    // enable factor of 1/2 for Very Green, Green, Regular

    int halve_promotion_chances_for_all_levels_bytes_length = 0x2;

    /*
    0:  7e 17                   jle    0x19
    */
    byte halve_promotion_chances_for_all_levels_bytes_old[] =
        { 0x7E, 0x17 }
    ;

    /*
    ...
    */
    byte halve_promotion_chances_for_all_levels_bytes_new[] =
        { 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00506342,
        halve_promotion_chances_for_all_levels_bytes_length,
        halve_promotion_chances_for_all_levels_bytes_old,
        halve_promotion_chances_for_all_levels_bytes_new
    )
    ;

    // disable attacker immediate promotion for Very Green, Green

    int disable_attacker_immediate_promotion_bytes_length = 0x2;

    /*
    0:  7e 21                   jle    0x23
    */
    byte disable_attacker_immediate_promotion_bytes_old[] =
        { 0x7E, 0x21 }
    ;

    /*
    ...
    */
    byte disable_attacker_immediate_promotion_bytes_new[] =
        { 0x90, 0x90 }
    ;

    write_bytes
    (
        0x0050A2B1,
        disable_attacker_immediate_promotion_bytes_length,
        disable_attacker_immediate_promotion_bytes_old,
        disable_attacker_immediate_promotion_bytes_new
    )
    ;

    // disable defender immediate promotion for Very Green, Green

    int disable_defender_immediate_promotion_bytes_length = 0x2;

    /*
    0:  7e 3a                   jle    0x3c
    */
    byte disable_defender_immediate_promotion_bytes_old[] =
        { 0x7E, 0x3A }
    ;

    /*
    ...
    */
    byte disable_defender_immediate_promotion_bytes_new[] =
        { 0x90, 0x90 }
    ;

    write_bytes
    (
        0x0050A3ED,
        disable_defender_immediate_promotion_bytes_length,
        disable_defender_immediate_promotion_bytes_old,
        disable_defender_immediate_promotion_bytes_new
    )
    ;

    // unify attacker_1 reactor power multiplier

    int unify_attacker_1_reactor_power_multiplier_bytes_length = 0x6;

    /*
    0:  8a 81 8f b8 9a 00       mov    al,BYTE PTR [ecx+0x9ab88f]
    */
    byte unify_attacker_1_reactor_power_multiplier_bytes_old[] =
        { 0x8A, 0x81, 0x8F, 0xB8, 0x9A, 0x00 }
    ;

    /*
    0:  b0 01                   mov    al,0x1
    ...
    */
    byte unify_attacker_1_reactor_power_multiplier_bytes_new[] =
        { 0xB0, 0x01, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x0050A223,
        unify_attacker_1_reactor_power_multiplier_bytes_length,
        unify_attacker_1_reactor_power_multiplier_bytes_old,
        unify_attacker_1_reactor_power_multiplier_bytes_new
    )
    ;

    // unify attacker_2 reactor power multiplier

    int unify_attacker_2_reactor_power_multiplier_bytes_length = 0x6;

    /*
    0:  8a 90 8f b8 9a 00       mov    dl,BYTE PTR [eax+0x9ab88f]
    */
    byte unify_attacker_2_reactor_power_multiplier_bytes_old[] =
        { 0x8A, 0x90, 0x8F, 0xB8, 0x9A, 0x00 }
    ;

    /*
    0:  b2 01                   mov    dl,0x1
    ...
    */
    byte unify_attacker_2_reactor_power_multiplier_bytes_new[] =
        { 0xB2, 0x01, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x0050A283,
        unify_attacker_2_reactor_power_multiplier_bytes_length,
        unify_attacker_2_reactor_power_multiplier_bytes_old,
        unify_attacker_2_reactor_power_multiplier_bytes_new
    )
    ;

    // unify defender_1 reactor power multiplier

    int unify_defender_1_reactor_power_multiplier_bytes_length = 0x6;

    /*
    0:  8a 90 8f b8 9a 00       mov    dl,BYTE PTR [eax+0x9ab88f]
    */
    byte unify_defender_1_reactor_power_multiplier_bytes_old[] =
        { 0x8A, 0x90, 0x8F, 0xB8, 0x9A, 0x00 }
    ;

    /*
    0:  b2 01                   mov    dl,0x1
    ...
    */
    byte unify_defender_1_reactor_power_multiplier_bytes_new[] =
        { 0xB2, 0x01, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x0050A341,
        unify_defender_1_reactor_power_multiplier_bytes_length,
        unify_defender_1_reactor_power_multiplier_bytes_old,
        unify_defender_1_reactor_power_multiplier_bytes_new
    )
    ;

    // unify defender_2 reactor power multiplier

    int unify_defender_2_reactor_power_multiplier_bytes_length = 0x6;

    /*
    0:  8a 90 8f b8 9a 00       mov    dl,BYTE PTR [eax+0x9ab88f]
    */
    byte unify_defender_2_reactor_power_multiplier_bytes_old[] =
        { 0x8A, 0x90, 0x8F, 0xB8, 0x9A, 0x00 }
    ;

    /*
    0:  b2 01                   mov    dl,0x1
    ...
    */
    byte unify_defender_2_reactor_power_multiplier_bytes_new[] =
        { 0xB2, 0x01, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x0050A3BF,
        unify_defender_2_reactor_power_multiplier_bytes_length,
        unify_defender_2_reactor_power_multiplier_bytes_old,
        unify_defender_2_reactor_power_multiplier_bytes_new
    )
    ;

}

/**
Removes Very Green on defense morale bonus.
*/
void patch_very_green_no_defense_bonus()
{
    // very_green_no_defense_bonus

    int very_green_no_defense_bonus_bytes_length = 0x2;

    /*
    0:  7c 0c                   jl     0xe
    */
    byte very_green_no_defense_bonus_bytes_old[] =
        { 0x7C, 0x0C }
    ;

    /*
    ...
    */
    byte very_green_no_defense_bonus_bytes_new[] =
        { 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00501C06,
        very_green_no_defense_bonus_bytes_length,
        very_green_no_defense_bonus_bytes_old,
        very_green_no_defense_bonus_bytes_new
    )
    ;

    // very_green_no_defense_bonus_display

    int very_green_no_defense_bonus_display_bytes_length = 0x2;

    /*
    0:  75 10                   jne    0x12
    */
    byte very_green_no_defense_bonus_display_bytes_old[] =
        { 0x75, 0x10 }
    ;

    /*
    0:  75 10                   jmp    0x12
    */
    byte very_green_no_defense_bonus_display_bytes_new[] =
        { 0xEB, 0x10 }
    ;

    write_bytes
    (
        0x004B4327,
        very_green_no_defense_bonus_display_bytes_length,
        very_green_no_defense_bonus_display_bytes_old,
        very_green_no_defense_bonus_display_bytes_new
    )
    ;

}

/**
Makes sea base territory distance same as land one.
*/
void patch_sea_territory_distance_same_as_land()
{
    // sea_territory_distance_same_as_land

    int sea_territory_distance_same_as_land_bytes_length = 0x2;

    /*
    0:  d1 fa                   sar    edx,1
    */
    byte sea_territory_distance_same_as_land_bytes_old[] =
        { 0xD1, 0xFA }
    ;

    /*
    ...
    */
    byte sea_territory_distance_same_as_land_bytes_new[] =
        { 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00523F01,
        sea_territory_distance_same_as_land_bytes_length,
        sea_territory_distance_same_as_land_bytes_old,
        sea_territory_distance_same_as_land_bytes_new
    )
    ;

}

/**
Makes coastal base territory distance same as sea one.
*/
void patch_coastal_territory_distance_same_as_sea()
{
    // wrap calculate_distance_to_nearest_base

    write_call_ptr(0x00523ED7, (int)calculate_distance_to_nearest_base);

}

// ========================================
// patch setup
// ========================================

bool patch_setup(Config* cf) {
    DWORD attrs;
    int lm = ~cf->landmarks;
    if (!VirtualProtect(AC_IMAGE_BASE, AC_IMAGE_LEN, PAGE_EXECUTE_READWRITE, &attrs))
        return false;

    write_call_ptr(0x52768A, (int)turn_upkeep);
    write_call_ptr(0x52A4AD, (int)turn_upkeep);
    write_call_ptr(0x528214, (int)faction_upkeep);
    write_call_ptr(0x52918F, (int)faction_upkeep);
    write_call_ptr(0x4E61D0, (int)base_production);
    write_call_ptr(0x4E888C, (int)crop_yield);
    write_call_ptr(0x5BDC4C, (int)tech_value);
    write_call_ptr(0x579362, (int)enemy_move);
    write_call_ptr(0x4AED04, (int)social_ai);
    write_call_ptr(0x527304, (int)social_ai);
    write_call_ptr(0x5C0908, (int)log_veh_kill);

    if (FileExists(ac_movlist_txt) && !FileExists(ac_movlistx_txt)) {
        CopyFile(ac_movlist_txt, ac_movlistx_txt, TRUE);
    }
    if (cf->smac_only) {
        *(int*)0x45F97A = 0;
        *(const char**)0x691AFC = ac_alpha;
        *(const char**)0x691B00 = ac_help;
        *(const char**)0x691B14 = ac_tutor;
        write_offset(0x42B30E, ac_concepts);
        write_offset(0x42B49E, ac_concepts);
        write_offset(0x42C450, ac_concepts);
        write_offset(0x42C7C2, ac_concepts);
        write_offset(0x403BA8, ac_movlist);
        write_offset(0x4BEF8D, ac_movlist);
        write_offset(0x52AB68, ac_opening);

        /* Enable custom faction selection during the game setup. */
        memset((void*)0x58A5E1, 0x90, 6);
        memset((void*)0x58B76F, 0x90, 2);
        memset((void*)0x58B9F3, 0x90, 2);
    }
    if (cf->faction_placement) {
        remove_call(0x5B1DFF);
        remove_call(0x5B2137);
        memset((void*)0x5B220F, 0x90, 63);
        memset((void*)0x5B2257, 0x90, 11);
        memcpy((void*)0x5B220F, asm_find_start, sizeof(asm_find_start));
        write_call_ptr(0x5B221B, (int)find_start);
    }
    if (cf->revised_tech_cost) {
        write_call_ptr(0x4452D5, (int)tech_rate);
        write_call_ptr(0x498D26, (int)tech_rate);
        write_call_ptr(0x4A77DA, (int)tech_rate);
        write_call_ptr(0x521872, (int)tech_rate);
        write_call_ptr(0x5218BE, (int)tech_rate);
        write_call_ptr(0x5581E9, (int)tech_rate);
        write_call_ptr(0x5BEA4D, (int)tech_rate);
        write_call_ptr(0x5BEAC7, (int)tech_rate);

        write_call_ptr(0x52935F, (int)tech_selection);
        write_call_ptr(0x5BE5E1, (int)tech_selection);
        write_call_ptr(0x5BE690, (int)tech_selection);
        write_call_ptr(0x5BEB5D, (int)tech_selection);
    }
    if (lm & LM_JUNGLE) remove_call(0x5C88A0);
    if (lm & LM_CRATER) remove_call(0x5C88A9);
    if (lm & LM_VOLCANO) remove_call(0x5C88B5);
    if (lm & LM_MESA) remove_call(0x5C88BE);
    if (lm & LM_RIDGE) remove_call(0x5C88C7);
    if (lm & LM_URANIUM) remove_call(0x5C88D0);
    if (lm & LM_RUINS) remove_call(0x5C88D9);
    if (lm & LM_UNITY) remove_call(0x5C88EE);
    if (lm & LM_FOSSIL) remove_call(0x5C88F7);
    if (lm & LM_CANYON) remove_call(0x5C8903);
    if (lm & LM_NEXUS) remove_call(0x5C890F);
    if (lm & LM_BOREHOLE) remove_call(0x5C8918);
    if (lm & LM_SARGASSO) remove_call(0x5C8921);
    if (lm & LM_DUNES) remove_call(0x5C892A);
    if (lm & LM_FRESH) remove_call(0x5C8933);
    if (lm & LM_GEOTHERMAL) remove_call(0x5C893C);

    // ==============================
    // The Will to Power mod changes
    // ==============================

    // patch battle_compute

    patch_battle_compute();

    // patch ignore_reactor_power_in_combat_processing

    if (cf->ignore_reactor_power_in_combat)
    {
        patch_ignore_reactor_power_in_combat_processing();

    }

    // patch combat roll

    patch_combat_roll();

    // patch calculate odds

    patch_calculate_odds();

    // patch ALIEN guaranteed technologies

    if (cf->disable_alien_guaranteed_technologies)
    {
        patch_alien_guaranteed_technologies();

    }

    // patch weapon icon selection algorithm

    if (cf->alternative_weapon_icon_selection_algorithm)
    {
        patch_weapon_icon_selection_algorithm();

    }

    // patch prototype cost formula

    if (cf->alternative_prototype_cost_formula)
    {
        patch_alternative_prototype_cost_formula();

    }

    // patch facility hurry penalty threshold

    if (cf->disable_hurry_penalty_threshold)
    {
        patch_disable_hurry_penalty_threshold();

    }

    // patch unit hurry cost formula

    if (cf->alternative_unit_hurry_formula)
    {
        patch_alternative_unit_hurry_formula();

    }

    // patch upgrade cost formula

    if (cf->alternative_upgrade_cost_formula)
    {
        patch_alternative_upgrade_cost_formula();

    }

    // patch base defense structure bonuses

    if (cf->alternative_base_defensive_structure_bonuses)
    {
        patch_defensive_structures_bonus(cf->perimeter_defense_multiplier, cf->tachyon_field_bonus);

    }

    // patch collateral damage defender reactor

    if (cf->collateral_damage_defender_reactor)
    {
        patch_collateral_damage_defender_reactor();

    }

    // patch collateral damage value

    if (cf->collateral_damage_value >= 0)
    {
        patch_collateral_damage_value(cf->collateral_damage_value);

    }

    // patch disable_aquatic_bonus_minerals

    if (cf->disable_aquatic_bonus_minerals)
    {
        patch_disable_aquatic_bonus_minerals();

    }

    // patch repair rate

    if (cf->repair_minimal != 1)
    {
        patch_repair_minimal(cf->repair_minimal);

    }
    if (cf->repair_fungus != 2)
    {
        patch_repair_fungus(cf->repair_fungus);

    }
    if (!cf->repair_friendly)
    {
        patch_repair_friendly();

    }
    if (!cf->repair_airbase)
    {
        patch_repair_airbase();

    }
    if (!cf->repair_bunker)
    {
        patch_repair_bunker();

    }
    if (cf->repair_base != 1)
    {
        patch_repair_base(cf->repair_base);

    }
    if (cf->repair_base_native != 10)
    {
        patch_repair_base_native(cf->repair_base_native);

    }
    if (cf->repair_base_facility != 1)
    {
        patch_repair_base_facility(cf->repair_base_facility);

    }
    if (cf->repair_nano_factory != 1)
    {
        patch_repair_nano_factory(cf->repair_nano_factory);

    }

    // patch disable_planetpearls

    if (cf->disable_planetpearls)
    {
        patch_disable_planetpearls();

    }

    // patch uniform_promotions

    if (cf->uniform_promotions)
    {
        patch_uniform_promotions();

    }

    // patch very_green_no_defense_bonus

    if (cf->very_green_no_defense_bonus)
    {
        patch_very_green_no_defense_bonus();

    }

    // patch sea_territory_distance_same_as_land

    if (cf->sea_territory_distance_same_as_land)
    {
        patch_sea_territory_distance_same_as_land();

    }

    // patch coastal_territory_distance_same_as_sea

    if (cf->coastal_territory_distance_same_as_sea)
    {
        patch_coastal_territory_distance_same_as_sea();

    }

    // continue with original Thinker checks

    if (!VirtualProtect(AC_IMAGE_BASE, AC_IMAGE_LEN, PAGE_EXECUTE_READ, &attrs))
        return false;

    return true;
}

