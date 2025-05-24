
#include "debug.h"
#include <sys/types.h>
#include <sys/stat.h>

typedef int(__cdecl *Fexcept_handler3)(EXCEPTION_RECORD*, PVOID, CONTEXT*);
Fexcept_handler3 _except_handler3 = (Fexcept_handler3)0x646DF8;


/*
Replace the default C++ exception handler in SMACX binary with a custom version.
This allows us to get more information about crash locations.
https://en.wikipedia.org/wiki/Microsoft-specific_exception_handling_mechanisms
*/
int __cdecl mod_except_handler3(EXCEPTION_RECORD* rec, PVOID* frame, CONTEXT* ctx)
{
    if (!debug_log && !(debug_log = fopen("debug.txt", "w"))) {
        return _except_handler3(rec, frame, ctx);
    }
    int32_t bytes = 0;
    int32_t value = 0;
    int32_t config_num = sizeof(conf)/sizeof(int);
    int32_t alloc_base = 0;
    size_t binary_size = 0;
    time_t rawtime = time(&rawtime);
    struct stat filedata;
    struct tm* now = localtime(&rawtime);
    char time_now[256] = {};
    char savepath[256] = {};
    char filepath[256] = {};
    char gamepath[512] = {};
    strftime(time_now, sizeof(time_now), "%Y-%m-%d %H:%M:%S", now);
    strncpy(savepath, LastSavePath, sizeof(savepath));
    savepath[250] = '\0';

    HMODULE handle;
    NTSTATUS(WINAPI *RtlGetVersion)(LPOSVERSIONINFOEXW);
    OSVERSIONINFOEXW info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    if ((handle = GetModuleHandleA("ntdll")) != NULL
    && (*(FARPROC*)&RtlGetVersion = GetProcAddress(handle, "RtlGetVersion")) != NULL) {
        RtlGetVersion(&info);
    }

    PBYTE pb = 0;
    MEMORY_BASIC_INFORMATION mbi;
    HANDLE hProcess = GetCurrentProcess();

    while (VirtualQueryEx(hProcess, pb, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        if (mbi.BaseAddress == AC_IMAGE_BASE
        && GetModuleFileNameA((HINSTANCE)mbi.AllocationBase, gamepath, sizeof(gamepath)) > 0) {
            if(stat(gamepath, &filedata) == 0) {
                binary_size = filedata.st_size;
            }
        }
        if (mbi.AllocationBase != NULL
        && (DWORD)mbi.AllocationBase <= ctx->Eip
        && (DWORD)mbi.AllocationBase + mbi.RegionSize > ctx->Eip) {
            value = GetModuleFileNameA((HINSTANCE)mbi.AllocationBase, filepath, sizeof(filepath));
            alloc_base = (int)mbi.AllocationBase;
            break;
        }
        pb += mbi.RegionSize;
    }
    filepath[250] = '\0';

    fprintf(debug_log,
        "************************************************************\n"
        "ModVersion %s (%s)\n"
        "WinVersion %lu.%lu.%lu\n"
        "BinarySize %d\n"
        "CrashTime  %s\n"
        "ModuleName %s\n"
        "SavedGame  %s\n"
        "************************************************************\n"
        "ExceptionCode    %08x\n"
        "ExceptionFlags   %08x\n"
        "ExceptionRecord  %08x\n"
        "ExceptionAddress %08x\n"
        "ExceptionBase    %08x\n",
        MOD_VERSION,
        MOD_DATE,
        info.dwMajorVersion,
        info.dwMinorVersion,
        info.dwBuildNumber,
        binary_size,
        time_now,
        (value > 0 ? filepath : ""),
        savepath,
        (int)rec->ExceptionCode,
        (int)rec->ExceptionFlags,
        (int)rec->ExceptionRecord,
        (int)rec->ExceptionAddress,
        alloc_base);

    fprintf(debug_log,
        "CFlags %08lx\n"
        "EFlags %08lx\n"
        "EAX %08lx\n"
        "EBX %08lx\n"
        "ECX %08lx\n"
        "EDX %08lx\n"
        "ESI %08lx\n"
        "EDI %08lx\n"
        "EBP %08lx\n"
        "ESP %08lx\n"
        "EIP %08lx\n",
        ctx->ContextFlags, ctx->EFlags,
        ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx,
        ctx->Esi, ctx->Edi, ctx->Ebp, ctx->Esp, ctx->Eip);

    int32_t* p = (int32_t*)ctx->Esp;
    fprintf(debug_log, "Stack dump:\n");

    for (int i = 0; i < 24; i++) {
        if (ReadProcessMemory(hProcess, p + i, &bytes, 4, NULL)) {
            fprintf(debug_log, "%08x: %08x\n", (int32_t)(p + i), bytes);
        } else {
            fprintf(debug_log, "%08x: ********\n", (int32_t)(p + i));
        }
    }

    byte code[16] = {};
    if (ReadProcessMemory(hProcess, (LPVOID)ctx->Eip, &code, 16, NULL)) {
        fprintf(debug_log, "Code dump:\n");
        for (int i = 0; i < 16; i++) {
            fprintf(debug_log, "%02X ", code[i]);
        }
        fprintf(debug_log, "\n");
    }

    p = (int32_t*)&conf;
    for (int i = 0; i < config_num; i++) {
        if (i % 10 == 0) {
            fprintf(debug_log, "Config %d:", i);
        }
        fprintf(debug_log, " %d", p[i]);
        if (i % 10 == 9 || i == config_num-1) {
            fprintf(debug_log, "\n");
        }
    }
    fclose(debug_log);
    return _except_handler3(rec, frame, ctx);
}

/*
Original (legacy) game engine logging functions.
*/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void __cdecl log_say(const char* a1, int a2, int a3, int a4) {
    if (conf.debug_verbose) {
        debug("**** %s %d %d %d\n", a1, a2, a3, a4);
    }
}

void __cdecl log_say2(const char* a1, const char* a2, int a3, int a4, int a5) {
    if (conf.debug_verbose) {
        debug("**** %s %s %d %d %d\n", a1, a2, a3, a4, a5);
    }
}

void __cdecl log_say_hex(const char* a1, int a2, int a3, int a4) {
    if (conf.debug_verbose) {
        debug("**** %s %04x %04x %04x\n", a1, a2, a3, a4);
    }
}

void __cdecl log_say_hex2(const char* a1, const char* a2, int a3, int a4, int a5) {
    if (conf.debug_verbose) {
        debug("**** %s %s %04x %04x %04x\n", a1, a2, a3, a4, a5);
    }
}

#pragma GCC diagnostic pop
/*
Additional custom non-engine debugging functions.
*/

void print_map(int x, int y) {
    MAP* m = mapsq(x, y);
    debug("MAP %3d %3d owner: %2d bonus: %d reg: %3d cont: %3d clim: %02x val2: %02x val3: %02x "\
        "vis: %02x unk1: %02x items: %08x lm: %08x\n",
        x, y, m->owner, bonus_at(x, y), m->region, m->contour, m->climate, m->val2, m->val3,
        m->visibility, m->unk_1, m->items, m->landmarks);
}

void print_veh_stack(int x, int y) {
    if (DEBUG) {
        for (int i = 0; i < *VehCount; i++) {
            if (Vehs[i].x == x && Vehs[i].y == y) {
                print_veh(i);
            }
        }
    }
}

void print_veh(int id) {
    VEH* v = &Vehicles[id];
    debug("VEH %30s u: %3d v: %4d owner: %d base: %3d order: %2d %2d %c %3d %3d -> %3d %3d "
        "next: %4d prev: %4d moves: %2d speed: %2d damage: %2d "
        "state: %08x flags: %04x vis: %02x mor: %d turns: %d iter: %d\n",
        Units[v->unit_id].name, v->unit_id, id, v->faction_id, v->home_base_id,
        v->order, v->order_auto_type, (v->status_icon ? v->status_icon : ' '),
        v->x, v->y, v->waypoint_x[0], v->waypoint_y[0],
        v->next_veh_id_stack, v->prev_veh_id_stack, v->moves_spent, veh_speed(id, 0),
        v->damage_taken, v->state, v->flags, v->visibility, v->morale,
        v->movement_turns, v->iter_count);
}

void print_unit(int id) {
    UNIT* u = &Units[id];
    int owner = id/MaxProtoFactionNum;
    bool old = (1 << owner) & u->obsolete_factions;
    debug("UNIT %30s u: %3d owner: %d group: %2d obs: %d chs: %d rec: %d wpn: %2d arm: %2d "
        "plan: %2d cost: %2d preq: %2d flags: %4X abls: %8X\n",
        u->name, id, owner, u->group_id, old, u->chassis_id, u->reactor_id, u->weapon_id, u->armor_id,
        u->plan, u->cost, u->preq_tech, u->unit_flags, u->ability_flags);
}

void print_base(int id) {
    BASE* base = &Bases[id];
    int prod = base->item();
    debug("[ turn: %d faction: %d base: %3d x: %3d y: %3d "\
        "pop: %2d tal: %2d dro: %2d spe: %2d min: %2d acc: %2d | %08x | %3d %s | %s ]\n",
        *CurrentTurn, base->faction_id, id, base->x, base->y,
        base->pop_size, base->talent_total, base->drone_total, base->specialist_total,
        base->mineral_surplus, base->minerals_accumulated,
        base->state_flags, prod, prod_name(prod), (char*)&(base->name));
}

