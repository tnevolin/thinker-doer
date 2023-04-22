#include "patch_wtp.h"

#include "game_wtp.h"
#include "ai.h"
#include "wtp.h"
#include "aiProduction.h"
#include "aiMove.h"

void exit_fail_over()
{
	MessageBoxA(0, "Error while patching game binary over existing patch call. Game will now exit.", MOD_VERSION, MB_OK | MB_ICONSTOP);
	exit(EXIT_FAILURE);
}

/*
Patches the call that has already been patched by previous write_call.
Does not check for existing pointer to be in image length.
*/
void write_call_over(int addr, int func)
{
	if (*(byte*)addr != 0xE8)
		exit_fail_over();
	
	*(int*)(addr+1) = func - addr - 5;
	
}

/*
Patch battle compute wrapper.
*/
void patch_battle_compute()
{
    // wrap battle computation into custom function

    write_call(0x0050474C, (int)mod_battle_compute);
    write_call(0x00506EA6, (int)mod_battle_compute);
    write_call(0x005085E0, (int)mod_battle_compute);

    // wrap string concatenation into custom function for battle computation output only

    write_call(0x00422915, (int)mod_battle_compute_compose_value_percentage);

}

/*
Disables boom time delay.
*/
void patch_disable_boom_delay()
{
    int disable_boom_delay_bytes_length = 0x2;
    
    /*
	0:  3b d7                   cmp    edx,edi
	*/
    byte disable_boom_delay_bytes_old[] =
		{ 0x3B, 0xD7 }
    ;
    
    /*
	0:  39 d2                   cmp    edx,edx
	*/
    byte disable_boom_delay_bytes_new[] =
        { 0x39, 0xD2 }
    ;

    write_bytes
    (
        0x00505B1B,
        disable_boom_delay_bytes_old,
        disable_boom_delay_bytes_new,
        disable_boom_delay_bytes_length
    )
    ;
    
}

/*
Disables boom time delay.
*/
void patch_disable_battle_refresh()
{
    int disable_battle_refresh_bytes_length = 0x7;
    
    /*
	0:  c7 45 94 03 00 00 00    mov    DWORD PTR [ebp-0x6c],0x3
	*/
    byte disable_battle_refresh_bytes_old[] =
		{ 0xC7, 0x45, 0x94, 0x03, 0x00, 0x00, 0x00 }
    ;
    
    /*
	0:  c7 45 94 00 00 00 00    mov    DWORD PTR [ebp-0x6c],0x0
	*/
    byte disable_battle_refresh_bytes_new[] =
        { 0xC7, 0x45, 0x94, 0x00, 0x00, 0x00, 0x00 }
    ;

    write_bytes
    (
        0x00508FDC,
        disable_battle_refresh_bytes_old,
        disable_battle_refresh_bytes_new,
        disable_battle_refresh_bytes_length
    )
    ;
    
}

/*
Disables boom drawing.
*/
void patch_disable_boom()
{
    remove_call(0x005088D2);
    remove_call(0x00509064);
    remove_call(0x0050925D);
    remove_call(0x005096A8);
    remove_call(0x00509B24);
    remove_call(0x00509D3A);
    
}

/*
Disables tile drawing.
*/
void patch_disable_battle_calls()
{
	// tile draw
	
    remove_call(0x0050884B);
    remove_call(0x0050885A);
    remove_call(0x00508967);
    remove_call(0x00508BDB);
    remove_call(0x00508BEA);
    remove_call(0x00508C7D);
    remove_call(0x00508C8C);
    remove_call(0x005092CB);
    remove_call(0x005092D4);
    remove_call(0x00509713);
    remove_call(0x00509722);
    remove_call(0x00509877);
    remove_call(0x0050B2CD);
    remove_call(0x0050B316);
    remove_call(0x0050B347);
    
    // set_iface_mode
    
    remove_call(0x00507D4D);
    remove_call(0x00507E91);
    remove_call(0x00508867);
    remove_call(0x00508C08);
    
    // status draw
    
    remove_call(0x00507D74);
    remove_call(0x00507EB8);
    remove_call(0x0050888A);
    remove_call(0x00508958);
    remove_call(0x00508C2D);
    remove_call(0x0050924E);
    remove_call(0x00509699);
    remove_call(0x00509A75);
    
    // flush_input
    
    remove_call(0x00508899);
    remove_call(0x00508C3C);
    
    // sub_55A150
    
    remove_call(0x00508C63);
    
}

void patch_disable_focus()
{
    int disable_focus1_bytes_length = 0xd;
    
    byte disable_focus1_bytes_old[] =
		{ 0x51, 0x52, 0x50, 0xB9, 0xB0, 0x56, 0x91, 0x00, 0xE8, 0x63, 0x80, 0x00, 0x00 }
    ;
    
    byte disable_focus1_bytes_new[] =
        { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00508830,
        disable_focus1_bytes_old,
        disable_focus1_bytes_new,
        disable_focus1_bytes_length
    )
    ;
    
    int disable_focus2_bytes_length = 0xc;
    
    byte disable_focus2_bytes_old[] =
		{ 0x52, 0x50, 0xB9, 0xB0, 0x56, 0x91, 0x00, 0xE8, 0xD3, 0x7C, 0x00, 0x00 }
    ;
    
    byte disable_focus2_bytes_new[] =
        { 0x83, 0xC4, 0x04, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00508BC1,
        disable_focus2_bytes_old,
        disable_focus2_bytes_new,
        disable_focus2_bytes_length
    )
    ;
    
}

/*
Patch read basic rules.
*/
void patch_read_basic_rules()
{
    write_call(0x00587424, (int)read_basic_rules);

}

/*
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

    write_bytes(0x00506F90, old_ignore_defender_reactor_power_combat_bytes, new_ignore_defender_reactor_power_combat_bytes, ignore_defender_reactor_power_combat_bytes_length);

    // ignore attacker reactor power

    int ignore_attacker_reactor_power_combat_bytes_length = 1;

    /* jl */
    byte old_ignore_attacker_reactor_power_combat_bytes[] = { 0x7C };

    /* jmp */
    byte new_ignore_attacker_reactor_power_combat_bytes[] = { 0xEB };

    write_bytes(0x00506FF6, old_ignore_attacker_reactor_power_combat_bytes, new_ignore_attacker_reactor_power_combat_bytes, ignore_attacker_reactor_power_combat_bytes_length);

}

/*
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
        combat_roll_old_bytes,
        combat_roll_new_bytes,
        combat_roll_bytes_length
    )
    ;

    // set call pointer to custom combat_roll function

    write_call(0x005091A5 + 0x1f, (int)combat_roll);

}

/*
Display more information and more correct one in odds display dialog.
*/
void patch_display_odds()
{
	// intercept parse_num call to capture attacker odds
	
	write_call(0x005082AF, (int)captureAttackerOdds);
	
	// intercept parse_num call to capture defender odds
	
	write_call(0x005082B7, (int)captureDefenderOdds);
	
	// intercept popp call to modify the display
	
	write_call(0x005082F0, (int)modifiedDisplayOdds);

}

/*
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

    write_bytes(0x005B29F8, old_bytes, new_bytes, bytes_length);

}

/*
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
        old_load_weapon_value_bytes,
        new_load_weapon_value_bytes,
        load_weapon_value_bytes_length
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
        old_weapon_icon_selection_table_bytes,
        new_weapon_icon_selection_table_bytes,
        weapon_icon_selection_table_bytes_length
    )
    ;

}

/*
Enables alternative prototype cost formula.
*/
void patch_alternative_prototype_cost_formula()
{
    write_call(0x00436ADD, (int)proto_cost);
    // this call special purposed to calculate prototype cost, not unit cost
//	write_call(0x0043704C, (int)proto_cost);
	write_call(0x005817C9, (int)proto_cost);
    write_call(0x00581833, (int)proto_cost);
    write_call(0x00581BB3, (int)proto_cost);
    write_call(0x00581BCB, (int)proto_cost);
    write_call(0x00582339, (int)proto_cost);
    write_call(0x00582359, (int)proto_cost);
    write_call(0x00582378, (int)proto_cost);
    write_call(0x00582398, (int)proto_cost);
    write_call(0x005823B0, (int)proto_cost);
    write_call(0x00582482, (int)proto_cost);
    write_call(0x0058249A, (int)proto_cost);
    write_call(0x0058254A, (int)proto_cost);
    write_call(0x005827E4, (int)proto_cost);
    write_call(0x00582EC5, (int)proto_cost);
    write_call(0x00582FEC, (int)proto_cost);
    write_call(0x005A5D35, (int)proto_cost);
    write_call(0x005A5F15, (int)proto_cost);

}

/*
Applies flat hurry cost mechanics.
*/
void patch_flat_hurry_cost()
{
	// BaseWin__hurry

    int flat_hurry_cost_bytes_length = 0x59;

/*
0:  2b ce                   sub    ecx,esi
2:  3b d7                   cmp    edx,edi
4:  7c 1b                   jl     0x21
6:  8b d1                   mov    edx,ecx
8:  b8 67 66 66 66          mov    eax,0x66666667
d:  0f af d1                imul   edx,ecx
10: f7 ea                   imul   edx
12: c1 fa 03                sar    edx,0x3
15: 8b c2                   mov    eax,edx
17: c1 e8 1f                shr    eax,0x1f
1a: 03 d0                   add    edx,eax
1c: 8d 3c 4a                lea    edi,[edx+ecx*2]
1f: eb 03                   jmp    0x24
21: 8d 3c 09                lea    edi,[ecx+ecx*1]
24: 83 7d e4 46             cmp    DWORD PTR [ebp-0x1c],0x46
28: 7c 25                   jl     0x4f
2a: a1 30 ea 90 00          mov    eax,ds:0x90ea30
2f: 33 c9                   xor    ecx,ecx
31: 6a ff                   push   0xffffffff
33: 6a 01                   push   0x1
35: 8a 48 04                mov    cl,BYTE PTR [eax+0x4]
38: 8b 70 40                mov    esi,DWORD PTR [eax+0x40]
3b: 51                      push   ecx
3c: 03 ff                   add    edi,edi
3e: e8 35 b4 0c 00          call   0xcb478
43: c1 e0 02                shl    eax,0x2
46: 83 c4 0c                add    esp,0xc
49: 3b f0                   cmp    esi,eax
4b: 7d 02                   jge    0x4f
4d: 03 ff                   add    edi,edi
4f: 3b 35 30 98 94 00       cmp    esi,DWORD PTR ds:0x949830
55: 7d 02                   jge    0x59
57: 03 ff                   add    edi,edi
*/
    byte flat_hurry_cost_bytes_old[] =
        { 0x2B, 0xCE, 0x3B, 0xD7, 0x7C, 0x1B, 0x8B, 0xD1, 0xB8, 0x67, 0x66, 0x66, 0x66, 0x0F, 0xAF, 0xD1, 0xF7, 0xEA, 0xC1, 0xFA, 0x03, 0x8B, 0xC2, 0xC1, 0xE8, 0x1F, 0x03, 0xD0, 0x8D, 0x3C, 0x4A, 0xEB, 0x03, 0x8D, 0x3C, 0x09, 0x83, 0x7D, 0xE4, 0x46, 0x7C, 0x25, 0xA1, 0x30, 0xEA, 0x90, 0x00, 0x33, 0xC9, 0x6A, 0xFF, 0x6A, 0x01, 0x8A, 0x48, 0x04, 0x8B, 0x70, 0x40, 0x51, 0x03, 0xFF, 0xE8, 0x35, 0xB4, 0x0C, 0x00, 0xC1, 0xE0, 0x02, 0x83, 0xC4, 0x0C, 0x3B, 0xF0, 0x7D, 0x02, 0x03, 0xFF, 0x3B, 0x35, 0x30, 0x98, 0x94, 0x00, 0x7D, 0x02, 0x03, 0xFF }
    ;

/*
0:  e8 fd ff ff ff          call   2 <_main+0x2>
5:  89 c7                   mov    edi,eax
...
*/
    byte flat_hurry_cost_bytes_new[] =
        { 0xE8, 0xFD, 0xFF, 0xFF, 0xFF, 0x89, 0xC7, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00418FB8,
        flat_hurry_cost_bytes_old,
        flat_hurry_cost_bytes_new,
        flat_hurry_cost_bytes_length
    )
    ;

    write_call(0x00418FB8, (int)modifiedHurryCost);

	// display additional hurry information

	write_call(0x004192A5, (int)displayHurryCostScaledToBasicMineralCostMultiplierInformation);
	write_call(0x004192FF, (int)displayPartialHurryCostToCompleteNextTurnInformation);

}

/*
Enables alternative upgrade cost formula.
*/
void patch_alternative_upgrade_cost_formula()
{
    write_call(0x004D0ECF, (int)upgrade_cost);
    write_call(0x004D16D9, (int)upgrade_cost);
    write_call(0x004EFB76, (int)upgrade_cost);
    write_call(0x004EFEB9, (int)upgrade_cost);

}

/*
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
        old_perimeter_defense_multiplier_bytes,
        new_perimeter_defense_multiplier_bytes,
        perimeter_defense_multiplier_bytes_length
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
        old_tachyon_field_bonus_bytes,
        new_tachyon_field_bonus_bytes,
        tachyon_field_bonus_bytes_length
    )
    ;

}

/*
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
        old_collateral_damage_defender_reactor_bytes,
        new_collateral_damage_defender_reactor_bytes,
        collateral_damage_defender_reactor_bytes_length
    )
    ;

}

// integrated into Thinker
///*
//Sets collateral damage value.
//*/
//void patch_collateral_damage_value(int collateral_damage_value)
//{
//    int collateral_damage_value_bytes_length = 0x2;
//
//    /*
//    mov     dl, 3
//    */
//    byte old_collateral_damage_value_bytes[] =
//        { 0xB2, 0x03 }
//    ;
//
//    /*
//    mov     dl, ?
//    */
//    byte new_collateral_damage_value_bytes[] =
//        { 0xB2, (byte)collateral_damage_value }
//    ;
//
//    write_bytes
//    (
//        0x0050AAA5,
//        old_collateral_damage_value_bytes,
//        new_collateral_damage_value_bytes,
//        collateral_damage_value_bytes_length
//    )
//    ;
//
//}

/*
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
        old_disable_aquatic_bonus_minerals_bytes,
        new_disable_aquatic_bonus_minerals_bytes,
        disable_aquatic_bonus_minerals_bytes_length
    )
    ;

}

// integrated into Thinker
///*
//Following methods modify repair rates.
//*/
//void patch_repair_minimal(int repair_minimal)
//{
//    int repair_minimal_bytes_length = 0x7;
//
//    /*
//    0:  c7 45 fc 01 00 00 00    mov    DWORD PTR [ebp-0x4],0x1
//    */
//    byte repair_minimal_bytes_old[] =
//        { 0xC7, 0x45, 0xFC, 0x01, 0x00, 0x00, 0x00 }
//    ;
//
//    /*
//    0:  c7 45 fc 01 00 00 00    mov    DWORD PTR [ebp-0x4],<repair_minimal>
//    */
//    byte repair_minimal_bytes_new[] =
//        { 0xC7, 0x45, 0xFC, (byte)repair_minimal, 0x00, 0x00, 0x00 }
//    ;
//
//    write_bytes
//    (
//        0x00526200,
//        repair_minimal_bytes_old,
//        repair_minimal_bytes_new,
//        repair_minimal_bytes_length
//    )
//    ;
//
//}
//void patch_repair_fungus(int repair_fungus)
//{
//    int repair_fungus_bytes_length = 0x7;
//
//    /*
//    0:  c7 45 fc 02 00 00 00    mov    DWORD PTR [ebp-0x4],0x2
//    */
//    byte repair_fungus_bytes_old[] =
//        { 0xC7, 0x45, 0xFC, 0x02, 0x00, 0x00, 0x00 }
//    ;
//
//    /*
//    0:  c7 45 fc 02 00 00 00    mov    DWORD PTR [ebp-0x4],<repair_fungus>
//    */
//    byte repair_fungus_bytes_new[] =
//        { 0xC7, 0x45, 0xFC, (byte)repair_fungus, 0x00, 0x00, 0x00 }
//    ;
//
//    write_bytes
//    (
//        0x00526261,
//        repair_fungus_bytes_old,
//        repair_fungus_bytes_new,
//        repair_fungus_bytes_length
//    )
//    ;
//
//}
//void patch_repair_friendly()
//{
//    int repair_friendly_bytes_length = 0x3;
//
//    /*
//    0:  ff 45 fc                inc    DWORD PTR [ebp-0x4]
//    */
//    byte repair_friendly_bytes_old[] =
//        { 0xFF, 0x45, 0xFC }
//    ;
//
//    /*
//    ...
//    */
//    byte repair_friendly_bytes_new[] =
//        { 0x90, 0x90, 0x90 }
//    ;
//
//    write_bytes
//    (
//        0x00526284,
//        repair_friendly_bytes_old,
//        repair_friendly_bytes_new,
//        repair_friendly_bytes_length
//    )
//    ;
//
//}
//void patch_repair_airbase()
//{
//    int repair_airbase_bytes_length = 0x3;
//
//    /*
//    0:  ff 45 fc                inc    DWORD PTR [ebp-0x4]
//    */
//    byte repair_airbase_bytes_old[] =
//        { 0xFF, 0x45, 0xFC }
//    ;
//
//    /*
//    ...
//    */
//    byte repair_airbase_bytes_new[] =
//        { 0x90, 0x90, 0x90 }
//    ;
//
//    write_bytes
//    (
//        0x005262D4,
//        repair_airbase_bytes_old,
//        repair_airbase_bytes_new,
//        repair_airbase_bytes_length
//    )
//    ;
//
//}
//void patch_repair_bunker()
//{
//    int repair_bunker_bytes_length = 0x3;
//
//    /*
//    0:  ff 45 fc                inc    DWORD PTR [ebp-0x4]
//    */
//    byte repair_bunker_bytes_old[] =
//        { 0xFF, 0x45, 0xFC }
//    ;
//
//    /*
//    ...
//    */
//    byte repair_bunker_bytes_new[] =
//        { 0x90, 0x90, 0x90 }
//    ;
//
//    write_bytes
//    (
//        0x005262F5,
//        repair_bunker_bytes_old,
//        repair_bunker_bytes_new,
//        repair_bunker_bytes_length
//    )
//    ;
//
//}
//void patch_repair_base(int repair_base)
//{
//    int repair_base_bytes_length = 0x10;
//
//    /*
//    0:  8b 45 fc                mov    eax,DWORD PTR [ebp-0x4]
//    3:  66 8b 8e 32 28 95 00    mov    cx,WORD PTR [esi+0x952832]
//    a:  40                      inc    eax
//    b:  33 d2                   xor    edx,edx
//    d:  89 45 fc                mov    DWORD PTR [ebp-0x4],eax
//    */
//    byte repair_base_bytes_old[] =
//        { 0x8B, 0x45, 0xFC, 0x66, 0x8B, 0x8E, 0x32, 0x28, 0x95, 0x00, 0x40, 0x33, 0xD2, 0x89, 0x45, 0xFC }
//    ;
//
//    /*
//    0:  66 8b 8e 32 28 95 00    mov    cx,WORD PTR [esi+0x952832]
//    7:  31 d2                   xor    edx,edx
//    9:  83 45 fc 01             add    DWORD PTR [ebp-0x4],0x1
//    ...
//    */
//    byte repair_base_bytes_new[] =
//        { 0x66, 0x8B, 0x8E, 0x32, 0x28, 0x95, 0x00, 0x31, 0xD2, 0x83, 0x45, 0xFC, (byte)repair_base, 0x90, 0x90, 0x90 }
//    ;
//
//    write_bytes
//    (
//        0x0052632E,
//        repair_base_bytes_old,
//        repair_base_bytes_new,
//        repair_base_bytes_length
//    )
//    ;
//
//}
//void patch_repair_base_native(int repair_base_native)
//{
//    int repair_base_native_bytes_length = 0xB;
//
//    /*
//    0:  33 c9                   xor    ecx,ecx
//    2:  8a 8e 38 28 95 00       mov    cl,BYTE PTR [esi+0x952838]
//    8:  89 4d fc                mov    DWORD PTR [ebp-0x4],ecx
//    */
//    byte repair_base_native_bytes_old[] =
//        { 0x33, 0xC9, 0x8A, 0x8E, 0x38, 0x28, 0x95, 0x00, 0x89, 0x4D, 0xFC }
//    ;
//
//    /*
//    0:  83 45 fc 01             add    DWORD PTR [ebp-0x4],<repair_base_native>
//    ...
//    */
//    byte repair_base_native_bytes_new[] =
//        { 0x83, 0x45, 0xFC, (byte)repair_base_native, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
//    ;
//
//    write_bytes
//    (
//        0x00526376,
//        repair_base_native_bytes_old,
//        repair_base_native_bytes_new,
//        repair_base_native_bytes_length
//    )
//    ;
//
//}
//void patch_repair_base_facility(int repair_base_facility)
//{
//    int repair_base_facility_bytes_length = 0xB;
//
//    /*
//    0:  33 d2                   xor    edx,edx
//    2:  8a 96 38 28 95 00       mov    dl,BYTE PTR [esi+0x952838]
//    8:  89 55 fc                mov    DWORD PTR [ebp-0x4],edx
//    */
//    byte repair_base_facility_bytes_old[] =
//        { 0x33, 0xD2, 0x8A, 0x96, 0x38, 0x28, 0x95, 0x00, 0x89, 0x55, 0xFC }
//    ;
//
//    /*
//    0:  83 45 fc 01             add    DWORD PTR [ebp-0x4],<repair_base_facility>
//    */
//    byte repair_base_facility_bytes_new[] =
//        { 0x83, 0x45, 0xFC, (byte)repair_base_facility, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
//    ;
//
//    write_bytes
//    (
//        0x00526422,
//        repair_base_facility_bytes_old,
//        repair_base_facility_bytes_new,
//        repair_base_facility_bytes_length
//    )
//    ;
//
//}
//void patch_repair_nano_factory(int repair_nano_factory)
//{
//    int repair_nano_factory_bytes_length = 0xB;
//
//    /*
//    0:  33 c9                   xor    ecx,ecx
//    2:  8a 8e 38 28 95 00       mov    cl,BYTE PTR [esi+0x952838]
//    8:  89 4d fc                mov    DWORD PTR [ebp-0x4],ecx
//    */
//    byte repair_nano_factory_bytes_old[] =
//        { 0x33, 0xC9, 0x8A, 0x8E, 0x38, 0x28, 0x95, 0x00, 0x89, 0x4D, 0xFC }
//    ;
//
//    /*
//    0:  83 45 fc 01             add    DWORD PTR [ebp-0x4],<repair_nano_factory>
//    */
//    byte repair_nano_factory_bytes_new[] =
//        { 0x83, 0x45, 0xFC, (byte)repair_nano_factory, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
//    ;
//
//    write_bytes
//    (
//        0x00526540,
//        repair_nano_factory_bytes_old,
//        repair_nano_factory_bytes_new,
//        repair_nano_factory_bytes_length
//    )
//    ;
//
//}

// integrated into Thinker
///*
//Disables planetpearls collection and corresponding message.
//*/
//void patch_disable_planetpearls()
//{
//    // remove planetpearls calculation code (effectively sets its value to 0)
//
//    int disable_planetpearls_bytes_length = 0x1C;
//
//    /*
//    0:  8b 45 08                mov    eax,DWORD PTR [ebp+0x8]
//    3:  6a 00                   push   0x0
//    5:  6a 01                   push   0x1
//    7:  50                      push   eax
//    8:  e8 46 ad 0b 00          call   0xbad53
//    d:  83 c4 0c                add    esp,0xc
//    10: 40                      inc    eax
//    11: 8d 0c 80                lea    ecx,[eax+eax*4]
//    14: 8b 06                   mov    eax,DWORD PTR [esi]
//    16: d1 e1                   shl    ecx,1
//    18: 03 c1                   add    eax,ecx
//    1a: 89 06                   mov    DWORD PTR [esi],eax
//    */
//    byte disable_planetpearls_bytes_old[] =
//        { 0x8B, 0x45, 0x08, 0x6A, 0x00, 0x6A, 0x01, 0x50, 0xE8, 0x46, 0xAD, 0x0B, 0x00, 0x83, 0xC4, 0x0C, 0x40, 0x8D, 0x0C, 0x80, 0x8B, 0x06, 0xD1, 0xE1, 0x03, 0xC1, 0x89, 0x06 }
//    ;
//
//    /*
//    ...
//    */
//    byte disable_planetpearls_bytes_new[] =
//        { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
//    ;
//
//    write_bytes
//    (
//        0x005060ED,
//        disable_planetpearls_bytes_old,
//        disable_planetpearls_bytes_new,
//        disable_planetpearls_bytes_length
//    )
//    ;
//
//}

/*
Makes promotions probabilities uniform for all levels.
*/
// breaks with Thinker ignore_reactor_power=1
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
        halve_promotion_chances_for_all_levels_bytes_old,
        halve_promotion_chances_for_all_levels_bytes_new,
        halve_promotion_chances_for_all_levels_bytes_length
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
        disable_attacker_immediate_promotion_bytes_old,
        disable_attacker_immediate_promotion_bytes_new,
        disable_attacker_immediate_promotion_bytes_length
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
        disable_defender_immediate_promotion_bytes_old,
        disable_defender_immediate_promotion_bytes_new,
        disable_defender_immediate_promotion_bytes_length
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
        unify_attacker_1_reactor_power_multiplier_bytes_old,
        unify_attacker_1_reactor_power_multiplier_bytes_new,
        unify_attacker_1_reactor_power_multiplier_bytes_length
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
        unify_attacker_2_reactor_power_multiplier_bytes_old,
        unify_attacker_2_reactor_power_multiplier_bytes_new,
        unify_attacker_2_reactor_power_multiplier_bytes_length
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
        unify_defender_1_reactor_power_multiplier_bytes_old,
        unify_defender_1_reactor_power_multiplier_bytes_new,
        unify_defender_1_reactor_power_multiplier_bytes_length
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
        unify_defender_2_reactor_power_multiplier_bytes_old,
        unify_defender_2_reactor_power_multiplier_bytes_new,
        unify_defender_2_reactor_power_multiplier_bytes_length
    )
    ;

}

/*
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
        very_green_no_defense_bonus_bytes_old,
        very_green_no_defense_bonus_bytes_new,
        very_green_no_defense_bonus_bytes_length
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
        very_green_no_defense_bonus_display_bytes_old,
        very_green_no_defense_bonus_display_bytes_new,
        very_green_no_defense_bonus_display_bytes_length
    )
    ;

}

/*
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
        sea_territory_distance_same_as_land_bytes_old,
        sea_territory_distance_same_as_land_bytes_new,
        sea_territory_distance_same_as_land_bytes_length
    )
    ;

}

// integrated into Thinker
///*
//Makes coastal base territory distance same as sea one.
//*/
//void patch_coastal_territory_distance_same_as_sea()
//{
//    // wrap base_find3
//
//    write_call(0x00523ED7, (int)base_find3);
//
//}

/*
Alternative artillery damage.
*/
void patch_alternative_artillery_damage()
{
    // alternative_artillery_damage

    int alternative_artillery_damage_bytes_length = 0x35;

    /*
    0:  99                      cdq
    1:  f7 f9                   idiv   ecx
    3:  40                      inc    eax
    4:  83 f8 01                cmp    eax,0x1
    7:  7c 12                   jl     0x1b
    9:  3d e7 03 00 00          cmp    eax,0x3e7
    e:  7e 07                   jle    0x17
    10: be e7 03 00 00          mov    esi,0x3e7
    15: eb 09                   jmp    0x20
    17: 8b f0                   mov    esi,eax
    19: eb 05                   jmp    0x20
    1b: be 01 00 00 00          mov    esi,0x1
    20: 8d 4e ff                lea    ecx,[esi-0x1]
    23: 85 c9                   test   ecx,ecx
    25: 7f 04                   jg     0x2b
    27: 33 ff                   xor    edi,edi
    29: eb 0a                   jmp    0x35
    2b: e8 d7 d9 13 00          call   0x13da07
    30: 99                      cdq
    31: f7 fe                   idiv   esi
    33: 8b fa                   mov    edi,edx
    */
    byte alternative_artillery_damage_bytes_old[] =
        { 0x99, 0xF7, 0xF9, 0x40, 0x83, 0xF8, 0x01, 0x7C, 0x12, 0x3D, 0xE7, 0x03, 0x00, 0x00, 0x7E, 0x07, 0xBE, 0xE7, 0x03, 0x00, 0x00, 0xEB, 0x09, 0x8B, 0xF0, 0xEB, 0x05, 0xBE, 0x01, 0x00, 0x00, 0x00, 0x8D, 0x4E, 0xFF, 0x85, 0xC9, 0x7F, 0x04, 0x33, 0xFF, 0xEB, 0x0A, 0xE8, 0xD7, 0xD9, 0x13, 0x00, 0x99, 0xF7, 0xFE, 0x8B, 0xFA }
    ;
    
    // correctly set attacker firepower depending on whether reactor is ignored in combat
    
    int attackerFirepowerByte;
    
    if (conf.ignore_reactor_power != 0 || conf.ignore_reactor_power_in_combat)
	{
		// attacker firepower = defender reactor (same as with H2H combat with ignored reactor)
		
		attackerFirepowerByte = 0x98;
		
	}
	else
	{
		// attacker firepower = attacker reactor (vanilla)
		
		attackerFirepowerByte = 0x90;
		
	}

    /*
    0:  ff 75 90                push   DWORD PTR [ebp-0x70]
    3:  51                      push   ecx
    4:  50                      push   eax
    5:  e8 fc ff ff ff          call   6 <_main+0x6>
    a:  83 c4 0c                add    esp,0xc
    d:  89 c7                   mov    edi,eax
    ...
    */
    byte alternative_artillery_damage_bytes_new[] =
        { 0xFF, 0x75, (byte)attackerFirepowerByte, 0x51, 0x50, 0xE8, 0xFC, 0xFF, 0xFF, 0xFF, 0x83, 0xC4, 0x0C, 0x89, 0xC7, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00508616,
        alternative_artillery_damage_bytes_old,
        alternative_artillery_damage_bytes_new,
        alternative_artillery_damage_bytes_length
    )
    ;

    // custom artillery damage generator

    write_call(0x00508616 + 0x5, (int)roll_artillery_damage);

}

/*
Disables home base Children's Creche morale bonus.
Disables home base Brood Pit morale bonus.
*/
void patch_disable_home_base_cc_morale_bonus()
{
    int disable_home_base_cc_morale_bonus_bytes_length = 0x2;
    byte disable_home_base_cc_morale_bonus_bytes_old[] = { 0xF7, 0xDA };
    byte disable_home_base_cc_morale_bonus_bytes_new[] = { 0x31, 0xD2 };

    write_bytes
    (
        0x005C1037,
        disable_home_base_cc_morale_bonus_bytes_old,
        disable_home_base_cc_morale_bonus_bytes_new,
        disable_home_base_cc_morale_bonus_bytes_length
    )
    ;

    int disable_home_base_bp_morale_bonus_bytes_length = 0x2;
    byte disable_home_base_bp_morale_bonus_bytes_old[] = { 0xF7, 0xD9 };
    byte disable_home_base_bp_morale_bonus_bytes_new[] = { 0x31, 0xC9 };

    write_bytes
    (
        0x005C1073,
        disable_home_base_bp_morale_bonus_bytes_old,
        disable_home_base_bp_morale_bonus_bytes_new,
        disable_home_base_bp_morale_bonus_bytes_length
    )
    ;

}

/*
Disables current base Children's Creche morale bonus.
Disables current base Brood Pit morale bonus.
*/
void patch_disable_current_base_cc_morale_bonus()
{
    // disable_current_base_cc_morale_bonus display

    int disable_current_base_cc_morale_bonus_display_bytes_length = 0x2;
    byte disable_current_base_cc_morale_bonus_display_bytes_old[] = { 0xF7, 0xDA };
    byte disable_current_base_cc_morale_bonus_display_bytes_new[] = { 0x31, 0xD2 };

    write_bytes
    (
        0x004B4144,
        disable_current_base_cc_morale_bonus_display_bytes_old,
        disable_current_base_cc_morale_bonus_display_bytes_new,
        disable_current_base_cc_morale_bonus_display_bytes_length
    )
    ;

    // disable_current_base_bp_morale_bonus display

    int disable_current_base_bp_morale_bonus_display1_bytes_length = 0x2;
    byte disable_current_base_bp_morale_bonus_display1_bytes_old[] = { 0xF7, 0xDA };
    byte disable_current_base_bp_morale_bonus_display1_bytes_new[] = { 0x31, 0xD2 };

    write_bytes
    (
        0x004B41CA,
        disable_current_base_bp_morale_bonus_display1_bytes_old,
        disable_current_base_bp_morale_bonus_display1_bytes_new,
        disable_current_base_bp_morale_bonus_display1_bytes_length
    )
    ;

    int disable_current_base_bp_morale_bonus_display2_bytes_length = 0x2;
    byte disable_current_base_bp_morale_bonus_display2_bytes_old[] = { 0xF7, 0xD8 };
    byte disable_current_base_bp_morale_bonus_display2_bytes_new[] = { 0x31, 0xC0 };

    write_bytes
    (
        0x004B425D,
        disable_current_base_bp_morale_bonus_display2_bytes_old,
        disable_current_base_bp_morale_bonus_display2_bytes_new,
        disable_current_base_bp_morale_bonus_display2_bytes_length
    )
    ;

    // disable_current_base_cc_morale_bonus attack

    int disable_current_base_cc_morale_bonus_attack_bytes_length = 0x2;
    byte disable_current_base_cc_morale_bonus_attack_bytes_old[] = { 0xF7, 0xD9 };
    byte disable_current_base_cc_morale_bonus_attack_bytes_new[] = { 0x31, 0xC9 };

    write_bytes
    (
        0x00501669,
        disable_current_base_cc_morale_bonus_attack_bytes_old,
        disable_current_base_cc_morale_bonus_attack_bytes_new,
        disable_current_base_cc_morale_bonus_attack_bytes_length
    )
    ;

    // disable_current_base_bp_morale_bonus attack

    int disable_current_base_bp_morale_bonus_attack1_bytes_length = 0x2;
    byte disable_current_base_bp_morale_bonus_attack1_bytes_old[] = { 0xF7, 0xD8 };
    byte disable_current_base_bp_morale_bonus_attack1_bytes_new[] = { 0x31, 0xC0 };

    write_bytes
    (
        0x005016D4,
        disable_current_base_bp_morale_bonus_attack1_bytes_old,
        disable_current_base_bp_morale_bonus_attack1_bytes_new,
        disable_current_base_bp_morale_bonus_attack1_bytes_length
    )
    ;

    int disable_current_base_bp_morale_bonus_attack2_bytes_length = 0x2;
    byte disable_current_base_bp_morale_bonus_attack2_bytes_old[] = { 0xF7, 0xD8 };
    byte disable_current_base_bp_morale_bonus_attack2_bytes_new[] = { 0x31, 0xC0 };

    write_bytes
    (
        0x00501747,
        disable_current_base_bp_morale_bonus_attack2_bytes_old,
        disable_current_base_bp_morale_bonus_attack2_bytes_new,
        disable_current_base_bp_morale_bonus_attack2_bytes_length
    )
    ;

    // disable_current_base_cc_morale_bonus defense

    int disable_current_base_cc_morale_bonus_defense_bytes_length = 0x2;
    byte disable_current_base_cc_morale_bonus_defense_bytes_old[] = { 0xF7, 0xD9 };
    byte disable_current_base_cc_morale_bonus_defense_bytes_new[] = { 0x31, 0xC9 };

    write_bytes
    (
        0x005019F1,
        disable_current_base_cc_morale_bonus_defense_bytes_old,
        disable_current_base_cc_morale_bonus_defense_bytes_new,
        disable_current_base_cc_morale_bonus_defense_bytes_length
    )
    ;

    // disable_current_base_bp_morale_bonus defense

    int disable_current_base_bp_morale_bonus_defense1_bytes_length = 0x2;
    byte disable_current_base_bp_morale_bonus_defense1_bytes_old[] = { 0xF7, 0xD8 };
    byte disable_current_base_bp_morale_bonus_defense1_bytes_new[] = { 0x31, 0xC0 };

    write_bytes
    (
        0x00501A55,
        disable_current_base_bp_morale_bonus_defense1_bytes_old,
        disable_current_base_bp_morale_bonus_defense1_bytes_new,
        disable_current_base_bp_morale_bonus_defense1_bytes_length
    )
    ;

    int disable_current_base_bp_morale_bonus_defense2_bytes_length = 0x2;
    byte disable_current_base_bp_morale_bonus_defense2_bytes_old[] = { 0xF7, 0xD8 };
    byte disable_current_base_bp_morale_bonus_defense2_bytes_new[] = { 0x31, 0xC0 };

    write_bytes
    (
        0x00501AC8,
        disable_current_base_bp_morale_bonus_defense2_bytes_old,
        disable_current_base_bp_morale_bonus_defense2_bytes_new,
        disable_current_base_bp_morale_bonus_defense2_bytes_length
    )
    ;

}

/*
Sets default unit morale to Very Green.
*/
void patch_default_morale_very_green()
{
    // default_morale_very_green place 1

    int default_morale_very_green_1_bytes_length = 0x1;

    /*
    0:  46                      inc    esi
    */
    byte default_morale_very_green_1_bytes_old[] =
        { 0x46 }
    ;

    /*
    ...
    */
    byte default_morale_very_green_1_bytes_new[] =
        { 0x90 }
    ;

    write_bytes
    (
        0x004F13A6,
        default_morale_very_green_1_bytes_old,
        default_morale_very_green_1_bytes_new,
        default_morale_very_green_1_bytes_length
    )
    ;

    // default_morale_very_green place 2

    int default_morale_very_green_2_bytes_length = 0x2;

    /*
    0:  fe c2                   inc    dl
    */
    byte default_morale_very_green_2_bytes_old[] =
        { 0xFE, 0xC2 }
    ;

    /*
    ...
    */
    byte default_morale_very_green_2_bytes_new[] =
        { 0x90, 0x90 }
    ;

    write_bytes
    (
        0x005C039C,
        default_morale_very_green_2_bytes_old,
        default_morale_very_green_2_bytes_new,
        default_morale_very_green_2_bytes_length
    )
    ;

}

/*
Patch tile yield calculation.
*/
void patch_tile_yield()
{
	// nutrient yield
	write_call(0x00465DD6, (int)mod_nutrient_yield);
	write_call(0x004B6C44, (int)mod_nutrient_yield);
	write_call(0x004BCEEB, (int)mod_nutrient_yield);
	write_call(0x004E7DE4, (int)mod_nutrient_yield);
	write_call(0x004E7F04, (int)mod_nutrient_yield);
	write_call(0x004E8034, (int)mod_nutrient_yield);
	// this call overrides Thinker's
	write_call_over(0x004E888C, (int)mod_nutrient_yield);
	write_call(0x004E96F4, (int)mod_nutrient_yield);
	write_call(0x004ED7F1, (int)mod_nutrient_yield);
	write_call(0x00565878, (int)mod_nutrient_yield);

	// mineral yield
	write_call(0x004B6E4F, (int)mod_mineral_yield);
	write_call(0x004B6EF9, (int)mod_mineral_yield);
	write_call(0x004B6F84, (int)mod_mineral_yield);
	write_call(0x004E7E00, (int)mod_mineral_yield);
	write_call(0x004E7F14, (int)mod_mineral_yield);
	write_call(0x004E8044, (int)mod_mineral_yield);
	write_call(0x004E88AC, (int)mod_mineral_yield);
	write_call(0x004E970A, (int)mod_mineral_yield);

	// energy yield
	write_call(0x004B7028, (int)mod_energy_yield);
	write_call(0x004B7136, (int)mod_energy_yield);
	write_call(0x004D3E5C, (int)mod_energy_yield);
	write_call(0x004E7E1C, (int)mod_energy_yield);
	write_call(0x004E7F24, (int)mod_energy_yield);
	write_call(0x004E8054, (int)mod_energy_yield);
	write_call(0x004E88CA, (int)mod_energy_yield);
	write_call(0x004E971F, (int)mod_energy_yield);
	write_call(0x0056C856, (int)mod_energy_yield);

}

/*
SE GROWTH/INDUSTRY change accumulated nutrients/minerals proportionally.
*/
void patch_se_accumulated_resource_adjustment()
{
    // SE set upon SE dialog close
    write_call(0x004AF0F6, (int)se_accumulated_resource_adjustment);

}

/*
Patch mod_hex_cost.
*/
void patch_hex_cost()
{
    write_call(0x00467711, (int)mod_hex_cost);
    write_call(0x00572518, (int)mod_hex_cost);
    write_call(0x005772D7, (int)mod_hex_cost);
    write_call(0x005776F4, (int)mod_hex_cost);
    write_call(0x00577E2C, (int)mod_hex_cost);
    write_call(0x00577F0E, (int)mod_hex_cost);
    write_call(0x00597695, (int)mod_hex_cost);
    write_call(0x0059ACA4, (int)mod_hex_cost);
    write_call(0x0059B61A, (int)mod_hex_cost);
    write_call(0x0059C105, (int)mod_hex_cost);

}

/*
Displays additional base population info in F4 screen.
*/
void patch_display_base_population_info()
{
    write_call(0x0049DCB1, (int)sayBase);

}

/*
Hooks base init computation.
This is for The Planetary Transit System patch.
*/
void patch_base_init()
{
    write_call(0x004C9870, (int)baseInit);
    write_call(0x005AF926, (int)baseInit);
    write_call(0x005B2BB8, (int)baseInit);

}

/*
HSA does not kill probe.
*/
void patch_hsa_does_not_kill_probe()
{
	// HSA does not kill probe but exhausts its movement points instead.

    write_call(0x0059F6EA, (int)veh_skip);

}

/*
Exclude defensive faciliites from probe target lists.
*/
void patch_probe_not_destroy_defense()
{
	// reusing logic for FAC_PRESSURE_DOME to determine whether base is at sea
	// now when this logic is gone there is no choice but to exclude it too

	// exclude defensive facilities and FAC_PRESSURE_DOME from probe random list

	int exclude_defensive_facilities_random_bytes_length = 51;
	byte exclude_defensive_facilities_random_bytes_old[] = { 0x83, 0xFF, 0x1A, 0x75, 0x2E, 0x0F, 0xBF, 0x8E, 0x42, 0xD0, 0x97, 0x00, 0x0F, 0xAF, 0x0D, 0xF0, 0xFA, 0x68, 0x00, 0x0F, 0xBF, 0x96, 0x40, 0xD0, 0x97, 0x00, 0xD1, 0xFA, 0x03, 0xCA, 0x8B, 0x15, 0x0C, 0xA3, 0x94, 0x00, 0x6B, 0xC9, 0x2C, 0x33, 0xC0, 0x8A, 0x04, 0x11, 0x24, 0xE0, 0x83, 0xF8, 0x60, 0x7C, 0x34 };
	byte exclude_defensive_facilities_random_bytes_new[] = { 0x83, 0xFF, 0x1A, 0x74, 0x62, 0x83, 0xFF, 0x04, 0x74, 0x5D, 0x83, 0xFF, 0x05, 0x74, 0x58, 0x83, 0xFF, 0x1C, 0x74, 0x53, 0x83, 0xFF, 0x1D, 0x74, 0x4E, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	write_bytes
	(
		0x005A06C8,
		exclude_defensive_facilities_random_bytes_old,
		exclude_defensive_facilities_random_bytes_new,
		exclude_defensive_facilities_random_bytes_length
	);

	// exclude defensive facilities and FAC_PRESSURE_DOME from probe selected list

	int exclude_defensive_facilities_selected_bytes_length = 54;
	byte exclude_defensive_facilities_selected_bytes_old[] = { 0x83, 0xFF, 0x1A, 0x75, 0x31, 0x0F, 0xBF, 0x86, 0x42, 0xD0, 0x97, 0x00, 0x0F, 0xAF, 0x05, 0xF0, 0xFA, 0x68, 0x00, 0x0F, 0xBF, 0x8E, 0x40, 0xD0, 0x97, 0x00, 0xD1, 0xF9, 0x03, 0xC1, 0x8B, 0x0D, 0x0C, 0xA3, 0x94, 0x00, 0x6B, 0xC0, 0x2C, 0x33, 0xD2, 0x8A, 0x14, 0x08, 0x83, 0xE2, 0xE0, 0x83, 0xFA, 0x60, 0x7C, 0x56, 0xEB, 0x05 };
	byte exclude_defensive_facilities_selected_bytes_new[] = { 0x83, 0xFF, 0x1A, 0x0F, 0x84, 0x81, 0x00, 0x00, 0x00, 0x83, 0xFF, 0x04, 0x0F, 0x84, 0x78, 0x00, 0x00, 0x00, 0x83, 0xFF, 0x05, 0x0F, 0x84, 0x6F, 0x00, 0x00, 0x00, 0x83, 0xFF, 0x1C, 0x0F, 0x84, 0x66, 0x00, 0x00, 0x00, 0x83, 0xFF, 0x1D, 0x0F, 0x84, 0x5D, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	write_bytes
	(
		0x005A0920,
		exclude_defensive_facilities_selected_bytes_old,
		exclude_defensive_facilities_selected_bytes_new,
		exclude_defensive_facilities_selected_bytes_length
	);

}

/*
Hooks itoa call in display ability routine.
This is to expand single number packed ability cost into human readable text.
*/
void patch_help_ability_cost_text()
{
	// in Datalinks

    write_call(0x0042EF7A, (int)getAbilityCostText);

    // in workshop

    write_call(0x0043B202, (int)appendAbilityCostTextInWorkshop);
    write_call(0x0043B29D, (int)appendAbilityCostTextInWorkshop);
    write_call(0x0043B334, (int)appendAbilityCostTextInWorkshop);
    write_call(0x0043B168, (int)appendAbilityCostTextInWorkshop);
    write_call(0x0043B417, (int)appendAbilityCostTextInWorkshop);
    write_call(0x0043B4FC, (int)appendAbilityCostTextInWorkshop);
    write_call(0x0043B5E1, (int)appendAbilityCostTextInWorkshop);
    write_call(0x0043B6A7, (int)appendAbilityCostTextInWorkshop);

}

/*
Disabes Cloning Vats impunities.
*/
void patch_cloning_vats_impunities()
{
	// disable cloning vats impunities display

	int disable_cv_display_bytes_length = 5;
	byte disable_cv_display_bytes_old[] = { 0xA1, 0x74, 0x65, 0x9A, 0x00 };
	byte disable_cv_display_bytes_new[] = { 0xB8, 0xFF, 0xFF, 0xFF, 0xFF };
	write_bytes(0x004AF514, disable_cv_display_bytes_old, disable_cv_display_bytes_new, disable_cv_display_bytes_length);
	write_bytes(0x004AF686, disable_cv_display_bytes_old, disable_cv_display_bytes_new, disable_cv_display_bytes_length);

	// disable cloning vats impunities computation

	int disable_cv_compute_bytes_length = 6;
	byte disable_cv_compute_bytes_old[] = { 0x8B, 0x0D, 0x74, 0x65, 0x9A, 0x00 };
	byte disable_cv_compute_bytes_new[] = { 0xB9, 0xFF, 0xFF, 0xFF, 0xFF, 0x90 };
	write_bytes(0x005B4225, disable_cv_compute_bytes_old, disable_cv_compute_bytes_new, disable_cv_compute_bytes_length);
	write_bytes(0x005B434D, disable_cv_compute_bytes_old, disable_cv_compute_bytes_new, disable_cv_compute_bytes_length);

}

/*
Wraps social_calc.
This is initially to enable CV GROWTH effect.
*/
void patch_social_calc()
{
    write_call(0x004AEC6E, (int)modifiedSocialCalc); // sets SE_pending values
    write_call(0x004AEC45, (int)modifiedSocialCalc);
    write_call(0x004AECC4, (int)modifiedSocialCalc); // sets SE_pending values
    write_call(0x004AEE4B, (int)modifiedSocialCalc);
    write_call(0x004AEE74, (int)modifiedSocialCalc);
    write_call(0x004AEECD, (int)modifiedSocialCalc); // resets SE_pending values
    // this particular call is already used by se_accumulated_resource_adjustment function
    // se_accumulated_resource_adjustment will call modifiedSocialCalc instead of tx_social_calc
//    write_call(0x004AF0F6, (int)modifiedSocialCalc);
    write_call(0x004B25B6, (int)modifiedSocialCalc);
    write_call(0x004B25DF, (int)modifiedSocialCalc);
    write_call(0x004B2635, (int)modifiedSocialCalc);
    write_call(0x005B4515, (int)modifiedSocialCalc);
    write_call(0x005B4527, (int)modifiedSocialCalc); // sets SE values
    write_call(0x005B4539, (int)modifiedSocialCalc); // sets SE_2 values
    write_call(0x005B464B, (int)modifiedSocialCalc);
    write_call(0x005B4AE1, (int)modifiedSocialCalc);

}

/*
Patches cloning vats mechanics.
*/
void patch_cloning_vats_mechanics()
{
	// disable triggering population boom

	int disable_cv_population_boom_bytes_length = 1;
	byte disable_cv_population_boom_bytes_old[] = {0x7C};
	byte disable_cv_population_boom_bytes_new[] = {0xEB};
	write_bytes
	(
		0x004EF389,
		disable_cv_population_boom_bytes_old,
		disable_cv_population_boom_bytes_new,
		disable_cv_population_boom_bytes_length
	);

	// CV GROWTH effect is set in modifiedSocialCalc

}

/*
Modifies SE GROWTH rating max.
*/
void patch_se_growth_rating_max(int se_growth_rating_max)
{
	// patch population boom condition in base_growth

	int popboom_condition_base_growth_bytes_length = 7;
	byte popboom_condition_base_growth_bytes_old[] = { 0x83, 0x3D, 0x18, 0xE9, 0x90, 0x00, 0x06 };
	byte popboom_condition_base_growth_bytes_new[] = { 0x83, 0x3D, 0x18, 0xE9, 0x90, 0x00, (byte)(se_growth_rating_max + 1) };
	write_bytes
	(
		0x004EF3A8,
		popboom_condition_base_growth_bytes_old,
		popboom_condition_base_growth_bytes_new,
		popboom_condition_base_growth_bytes_length
	);

	// patch population boom condition in base_doctors

	int popboom_condition_base_doctorss_bytes_length = 7;
	byte popboom_condition_base_doctorss_bytes_old[] = { 0x83, 0x3D, 0x18, 0xE9, 0x90, 0x00, 0x06 };
	byte popboom_condition_base_doctorss_bytes_new[] = { 0x83, 0x3D, 0x18, 0xE9, 0x90, 0x00, (byte)(se_growth_rating_max + 1) };
	write_bytes
	(
		0x004F6070,
		popboom_condition_base_doctorss_bytes_old,
		popboom_condition_base_doctorss_bytes_new,
		popboom_condition_base_doctorss_bytes_length
	);

	// patch cost_factor nutrient max

	int cost_factor_nutrient_max_check_bytes_length = 3;
	byte cost_factor_nutrient_max_check_bytes_old[] = { 0x83, 0xFB, 0x05 };
	byte cost_factor_nutrient_max_check_bytes_new[] = { 0x83, 0xFB, (byte)se_growth_rating_max };
	write_bytes
	(
		0x004E4577,
		cost_factor_nutrient_max_check_bytes_old,
		cost_factor_nutrient_max_check_bytes_new,
		cost_factor_nutrient_max_check_bytes_length
	);

	int cost_factor_nutrient_max_set_bytes_length = 5;
	byte cost_factor_nutrient_max_set_bytes_old[] = { 0xB8, 0x05, 0x00, 0x00, 0x00 };
	byte cost_factor_nutrient_max_set_bytes_new[] = { 0xB8, (byte)se_growth_rating_max, 0x00, 0x00, 0x00 };
	write_bytes
	(
		0x004E457C,
		cost_factor_nutrient_max_set_bytes_old,
		cost_factor_nutrient_max_set_bytes_new,
		cost_factor_nutrient_max_set_bytes_length
	);

}

/*
Modifies SE GROWTH rating min.
*/
void patch_se_growth_rating_min(int se_growth_rating_min)
{
	// patch population boom condition in base_growth

	int popboom_condition_base_growth_bytes_length = 7;
	byte popboom_condition_base_growth_bytes_old[] = { 0x83, 0x3D, 0x18, 0xE9, 0x90, 0x00, 0xFD };
	byte popboom_condition_base_growth_bytes_new[] = { 0x83, 0x3D, 0x18, 0xE9, 0x90, 0x00, (byte)(se_growth_rating_min - 1) };
	write_bytes
	(
		0x004EF4F9,
		popboom_condition_base_growth_bytes_old,
		popboom_condition_base_growth_bytes_new,
		popboom_condition_base_growth_bytes_length
	);

	// patch cost_factor nutrient min

	int cost_factor_nutrient_min_check_bytes_length = 3;
	byte cost_factor_nutrient_min_check_bytes_old[] = { 0x83, 0xFB, 0xFE };
	byte cost_factor_nutrient_min_check_bytes_new[] = { 0x83, 0xFB, (byte)se_growth_rating_min };
	write_bytes
	(
		0x004E4572,
		cost_factor_nutrient_min_check_bytes_old,
		cost_factor_nutrient_min_check_bytes_new,
		cost_factor_nutrient_min_check_bytes_length
	);

	int cost_factor_nutrient_min_set_bytes_length = 5;
	byte cost_factor_nutrient_min_set_bytes_old[] = { 0xB8, 0xFE, 0xFF, 0xFF, 0xFF };
	byte cost_factor_nutrient_min_set_bytes_new[] = { 0xB8, (byte)se_growth_rating_min, 0x00, 0x00, 0x00 };
	write_bytes
	(
		0x004E4587,
		cost_factor_nutrient_min_set_bytes_old,
		cost_factor_nutrient_min_set_bytes_new,
		cost_factor_nutrient_min_set_bytes_length
	);

}

/*
Wraps _strcat call in base nutrient display to display nutrient cost factor.
*/
void patch_display_base_nutrient_cost_factor()
{
	// change color

	int base_nutrient_header_color_bytes_length = 0x5;
	/*
	0:  a1 a4 6c 8c 00          mov    eax,ds:0x8c6ca4
	*/
	byte base_nutrient_header_color_bytes_old[] = { 0xA1, 0xA4, 0x6C, 0x8C, 0x00 };
	/*
	0:  8b 15 c4 6c 8c 00       mov    edx,DWORD PTR ds:0x8c6cc4
	*/
	byte base_nutrient_header_color_bytes_new[] = { 0xA1, 0xC4, 0x6C, 0x8C, 0x00 };
	write_bytes
	(
		0x0041137A,
		base_nutrient_header_color_bytes_old,
		base_nutrient_header_color_bytes_new,
		base_nutrient_header_color_bytes_length
	);

	// overwrite text

	write_call(0x004113B0, (int)displayBaseNutrientCostFactor);

}

/*
Wraps _strcat call in base nutrient display to display correct indicator for pop boom and stagnation.
*/
void patch_growth_turns_population_boom()
{
	// change font

	int base_nutrient_turns_font_bytes_length = 0x6;
	/*
	0:  8d 85 48 ff ff ff       lea    eax,[ebp-0xb8]
	*/
	byte base_nutrient_turns_font_bytes_old[] = { 0x8D, 0x85, 0x48, 0xFF, 0xFF, 0xFF };
	/*
	0:  b8 b4 84 6e 00          mov    eax,0x6e84b4
	...
	*/
	byte base_nutrient_turns_font_bytes_new[] = { 0xB8, 0xB4, 0x84, 0x6E, 0x00, 0x90 };
	write_bytes
	(
		0x0041142A,
		base_nutrient_turns_font_bytes_old,
		base_nutrient_turns_font_bytes_new,
		base_nutrient_turns_font_bytes_length
	);

	// change color

	int base_nutrient_turns_color_bytes_length = 0x6;
	/*
	0:  8b 15 fc 6c 8c 00       mov    edx,DWORD PTR ds:0x8c6cfc
	*/
	byte base_nutrient_turns_color_bytes_old[] = { 0x8B, 0x15, 0xFC, 0x6C, 0x8C, 0x00 };
	/*
	0:  8b 15 c4 6c 8c 00       mov    edx,DWORD PTR ds:0x8c6cc4
	*/
	byte base_nutrient_turns_color_bytes_new[] = { 0x8B, 0x15, 0xC4, 0x6C, 0x8C, 0x00 };
	write_bytes
	(
		0x0041144D,
		base_nutrient_turns_color_bytes_old,
		base_nutrient_turns_color_bytes_new,
		base_nutrient_turns_color_bytes_length
	);

	// overwrite text

	write_call(0x004118BE, (int)correctGrowthTurnsIndicator);

}

/*
Wraps has_fac call in base_minerals to sneak in RT mineral increase.
*/
void patch_recycling_tank_minerals()
{
	write_call(0x004E9E68, (int)modifiedRecyclingTanksMinerals);

}

// integrated into Thinker
///*
//Modifies free minerals.
//*/
//void patch_free_minerals(int freeMinerals)
//{
//	int free_minerals_bytes_length = 3;
//	byte free_minerals_bytes_old[] = { 0x83, 0xC6, 0x10 };
//	byte free_minerals_bytes_new[] = { 0x83, 0xC6, (byte)freeMinerals };
//	write_bytes
//	(
//		0x004E9E41,
//		free_minerals_bytes_old,
//		free_minerals_bytes_new,
//		free_minerals_bytes_length
//	);
//
//}

/*
Fixes population box incorrectly drawn superdrones.
*/
void patch_base_scren_population_superdrones()
{
	int base_screen_population_superdrones_human_bytes_length = 6;
	byte base_screen_population_superdrones_human_bytes_old[] = { 0x2B, 0x8B, 0x20, 0x01, 0x00, 0x00 };
	byte base_screen_population_superdrones_human_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	write_bytes
	(
		0x00414ED6,
		base_screen_population_superdrones_human_bytes_old,
		base_screen_population_superdrones_human_bytes_new,
		base_screen_population_superdrones_human_bytes_length
	);

	int base_screen_population_superdrones_alien_bytes_length = 6;
	byte base_screen_population_superdrones_alien_bytes_old[] = { 0x2B, 0x8A, 0x20, 0x01, 0x00, 0x00 };
	byte base_screen_population_superdrones_alien_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	write_bytes
	(
		0x00414D43,
		base_screen_population_superdrones_alien_bytes_old,
		base_screen_population_superdrones_alien_bytes_new,
		base_screen_population_superdrones_alien_bytes_length
	);

}

// integrated into Thinker
///*
//Equally distant territory is given to base with smaller ID (presumably older one).
//*/
//void patch_conflicting_territory()
//{
//	int conflicting_territory_bytes_length = 2;
//	byte conflicting_territory_bytes_old[] = { 0x7F, 0x09 };
//	byte conflicting_territory_bytes_new[] = { 0x7D, 0x09 };
//	write_bytes
//	(
//		0x004E3EAE,
//		conflicting_territory_bytes_old,
//		conflicting_territory_bytes_new,
//		conflicting_territory_bytes_length
//	);
//
//}

/*
Modifies abundance of native life.
*/
void patch_native_life_frequency(int constant, int multiplier)
{
	// check allowed values

	if (constant < 0 || constant > 255 || !(multiplier == 2 || multiplier == 3 || multiplier == 5))
		return;

	// calculate multiplier code

	int multiplierCode;
	switch (multiplier)
	{
	case 2:
		multiplierCode = 0x00;
		break;
	case 3:
		multiplierCode = 0x40;
		break;
	case 5:
		multiplierCode = 0x80;
		break;
	default:
		return;
	}

	// modify number tries for natives appearance

	int native_life_multiplier1_bytes_length = 4;
	byte native_life_multiplier1_bytes_old[] = { 0x8D, 0x54, 0x09, 0x02 };
	byte native_life_multiplier1_bytes_new[] = { 0x8D, 0x54, (byte)(multiplierCode + 0x09), (byte)constant };
	write_bytes
	(
		0x00522C02,
		native_life_multiplier1_bytes_old,
		native_life_multiplier1_bytes_new,
		native_life_multiplier1_bytes_length
	);
	int native_life_multiplier2_bytes_length = 4;
	byte native_life_multiplier2_bytes_old[] = { 0x8D, 0x4C, 0x00, 0x02 };
	byte native_life_multiplier2_bytes_new[] = { 0x8D, 0x4C, (byte)(multiplierCode + 0x00), (byte)constant };
	write_bytes
	(
		0x00522C13,
		native_life_multiplier2_bytes_old,
		native_life_multiplier2_bytes_new,
		native_life_multiplier2_bytes_length
	);

}

/*
Makes sea native appear every turn.
*/
void patch_native_life_sea_creatures()
{
	int native_life_sea_ratio1_bytes_length = 5;
	byte native_life_sea_ratio1_bytes_old[] = { 0xBE, 0x05, 0x00, 0x00, 0x00 };
	byte native_life_sea_ratio1_bytes_new[] = { 0xBE, 0x01, 0x00, 0x00, 0x00 };
	write_bytes
	(
		0x0052261A,
		native_life_sea_ratio1_bytes_old,
		native_life_sea_ratio1_bytes_new,
		native_life_sea_ratio1_bytes_length
	);
	int native_life_sea_ratio2_bytes_length = 2;
	byte native_life_sea_ratio2_bytes_old[] = { 0x2B, 0xF2 };
	byte native_life_sea_ratio2_bytes_new[] = { 0x90, 0x90 };
	write_bytes
	(
		0x00522621,
		native_life_sea_ratio2_bytes_old,
		native_life_sea_ratio2_bytes_new,
		native_life_sea_ratio2_bytes_length
	);
	int native_life_sea_ratio3_bytes_length = 2;
	byte native_life_sea_ratio3_bytes_old[] = { 0xD1, 0xE6 };
	byte native_life_sea_ratio3_bytes_new[] = { 0x90, 0x90 };
	write_bytes
	(
		0x00522624,
		native_life_sea_ratio3_bytes_old,
		native_life_sea_ratio3_bytes_new,
		native_life_sea_ratio3_bytes_length
	);

}

/*
Disable native sudden death.
*/
void patch_native_disable_sudden_death()
{
	int native_life_die_ratio_bytes_length = 3;
	byte native_life_die_ratio_bytes_old[] = { 0xF6, 0xC2, 0x07 };
	byte native_life_die_ratio_bytes_new[] = { 0xF6, 0xC2, 0xFF };
	write_bytes
	(
		0x00566E2E,
		native_life_die_ratio_bytes_old,
		native_life_die_ratio_bytes_new,
		native_life_die_ratio_bytes_length
	);

}

/*
Modified inefficiency computation.
*/
void patch_alternative_inefficiency()
{
	write_call(0x004EB5AE, (int)modifiedInefficiency);
	write_call(0x004EB93C, (int)modifiedInefficiency);

}

/*
Wraps setup_player adding custom functionality.
*/
void patch_setup_player()
{
	write_call(0x00525CC7, (int)modifiedSetupPlayer);
	write_call(0x005A3C9B, (int)modifiedSetupPlayer);
	write_call(0x005B341C, (int)modifiedSetupPlayer);
	write_call(0x005B3C03, (int)modifiedSetupPlayer);
	write_call(0x005B3C4C, (int)modifiedSetupPlayer);

}

/*
Wraps veh_init to correctly generate number of colonies at start.
*/
void patch_veh_init_in_balance()
{
	write_call(0x005B08B1, (int)modifiedVehInitInBalance);

}

/*
Enables building Recycling Tanks even if Pressure Dome presents.
*/
void patch_recycling_tanks_not_disabled_by_pressure_dome()
{
	int recycling_tanks_disabled_by_pressure_dome_bytes_length = 3;
	byte recycling_tanks_disabled_by_pressure_dome_bytes_old[] = { 0x83, 0xFB, 0x03 };
	byte recycling_tanks_disabled_by_pressure_dome_bytes_new[] = { 0x83, 0xFB, 0xFF };
	write_bytes
	(
		0x005BA436,
		recycling_tanks_disabled_by_pressure_dome_bytes_old,
		recycling_tanks_disabled_by_pressure_dome_bytes_new,
		recycling_tanks_disabled_by_pressure_dome_bytes_length
	);

}

/*
Overrides world_build.
*/
void patch_world_build()
{
	write_call(0x004E1061, (int)modifiedWorldBuild);
	write_call(0x004E113B, (int)modifiedWorldBuild);
	write_call(0x0058B9BF, (int)modifiedWorldBuild);
	write_call(0x0058DDD8, (int)modifiedWorldBuild);

}

///*
//Overrides check for frendly unit in target tile.
//*/
//void patch_zoc_regular_army_sneaking_disabled()
//{
////	// disable same dst vehicle owner check
////	
////	int zoc_regular_army_sneaking_disabled_same_bytes_length = 0x3;
////	byte zoc_regular_army_sneaking_disabled_same_bytes_old[] = { 0x8B, 0x4D, 0xE4 };
////	byte zoc_regular_army_sneaking_disabled_same_bytes_new[] = { 0xB1, 0x08, 0x90 };
////	write_bytes
////	(
////		0x00595156,
////		zoc_regular_army_sneaking_disabled_same_bytes_old,
////		zoc_regular_army_sneaking_disabled_same_bytes_new,
////		zoc_regular_army_sneaking_disabled_same_bytes_length
////	);
////	
////	// disable pact dst vehicle owner check
////	
////	int zoc_regular_army_sneaking_disabled_pact_bytes_length = 0x8;
////	byte zoc_regular_army_sneaking_disabled_pact_bytes_old[] = { 0xF6, 0x04, 0x8D, 0xF8, 0xC9, 0x96, 0x00, 0x01 };
////	byte zoc_regular_army_sneaking_disabled_pact_bytes_new[] = { 0x39, 0xE4, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
////	write_bytes
////	(
////		0x0059517B,
////		zoc_regular_army_sneaking_disabled_pact_bytes_old,
////		zoc_regular_army_sneaking_disabled_pact_bytes_new,
////		zoc_regular_army_sneaking_disabled_pact_bytes_length
////	);
////	
//	// disable dst vehicle check
//	
//	int zoc_regular_army_sneaking_disabled_dst_veh_bytes_length = 0x5;
//	byte zoc_regular_army_sneaking_disabled_dst_veh_bytes_old[] = { 0x8B, 0x45, 0xE0, 0x85, 0xC0 };
//	byte zoc_regular_army_sneaking_disabled_dst_veh_bytes_new[] = { 0x31, 0xC0, 0x83, 0xF8, 0x08 };
//	write_bytes
//	(
//		0x00595385,
//		zoc_regular_army_sneaking_disabled_dst_veh_bytes_old,
//		zoc_regular_army_sneaking_disabled_dst_veh_bytes_new,
//		zoc_regular_army_sneaking_disabled_dst_veh_bytes_length
//	);
//	
//}
//
/*
Modifies biology lab research bonus.
*/
void patch_biology_lab_research_bonus(int biology_lab_research_bonus)
{
	int biology_lab_research_bonus_bytes_length = 7;
	byte biology_lab_research_bonus_bytes_old[] = { 0x83, 0x80, 0x08, 0x01, 0x00, 0x00, 0x02 };
	byte biology_lab_research_bonus_bytes_new[] = { 0x83, 0x80, 0x08, 0x01, 0x00, 0x00, (byte)biology_lab_research_bonus };
	write_bytes
	(
		0x004EBC85,
		biology_lab_research_bonus_bytes_old,
		biology_lab_research_bonus_bytes_new,
		biology_lab_research_bonus_bytes_length
	);
	
}

/*
Weapon help always shows cost even if it equals to firepower.
*/
void patch_weapon_help_always_show_cost()
{
	int weapon_help_show_cost_always_bytes_length = 2;
	byte weapon_help_show_cost_always_bytes_old[] = { 0x75, 0x24 };
	byte weapon_help_show_cost_always_bytes_new[] = { 0xEB, 0x24 };
	write_bytes
	(
		0x0042E5D5,
		weapon_help_show_cost_always_bytes_old,
		weapon_help_show_cost_always_bytes_new,
		weapon_help_show_cost_always_bytes_length
	);

}

/*
Disables restoring movement points to terraforming vehicle.
*/
void patch_veh_wake()
{
	int veh_wake_does_not_restore_movement_points_bytes_length = 2;
	byte veh_wake_does_not_restore_movement_points_bytes_old[] = { 0x75, 0x45 };
	byte veh_wake_does_not_restore_movement_points_bytes_new[] = { 0xEB, 0x45 };
	write_bytes
	(
		0x005C1D9A,
		veh_wake_does_not_restore_movement_points_bytes_old,
		veh_wake_does_not_restore_movement_points_bytes_new,
		veh_wake_does_not_restore_movement_points_bytes_length
	);

}

/*
Flattens extra prototype cost.
*/
void patch_flat_extra_prototype_cost()
{
	// in DesignWin::draw_unit_preview

	// prepare parameters for non prototyped components cost calculation

	int draw_unit_preview_flat_prototype_cost_bytes_length = 0x1e;
/*
0:  8b 86 0c 42 01 00       mov    eax,DWORD PTR [esi+0x1420c]
6:  8b 8e 04 42 01 00       mov    ecx,DWORD PTR [esi+0x14204]
c:  8b 96 00 42 01 00       mov    edx,DWORD PTR [esi+0x14200]
12: 50                      push   eax
13: 8b 86 fc 41 01 00       mov    eax,DWORD PTR [esi+0x141fc]
19: 6a 00                   push   0x0
1b: 51                      push   ecx
1c: 52                      push   edx
1d: 50                      push   eax
*/
	byte draw_unit_preview_flat_prototype_cost_bytes_old[] = { 0x8B, 0x86, 0x0C, 0x42, 0x01, 0x00, 0x8B, 0x8E, 0x04, 0x42, 0x01, 0x00, 0x8B, 0x96, 0x00, 0x42, 0x01, 0x00, 0x50, 0x8B, 0x86, 0xFC, 0x41, 0x01, 0x00, 0x6A, 0x00, 0x51, 0x52, 0x50 };
/*
0:  ff 75 cc                push   DWORD PTR [ebp-0x34]
3:  ff 75 d0                push   DWORD PTR [ebp-0x30]
6:  ff 75 d4                push   DWORD PTR [ebp-0x2c]
9:  ff b6 04 42 01 00       push   DWORD PTR [esi+0x14204]
f:  ff b6 00 42 01 00       push   DWORD PTR [esi+0x14200]
15: ff b6 fc 41 01 00       push   DWORD PTR [esi+0x141fc]
...
*/
	byte draw_unit_preview_flat_prototype_cost_bytes_new[] = { 0xFF, 0x75, 0xCC, 0xFF, 0x75, 0xD0, 0xFF, 0x75, 0xD4, 0xFF, 0xB6, 0x04, 0x42, 0x01, 0x00, 0xFF, 0xB6, 0x00, 0x42, 0x01, 0x00, 0xFF, 0xB6, 0xFC, 0x41, 0x01, 0x00, 0x90, 0x90, 0x90 };
	write_bytes
	(
		0x0043702E,
		draw_unit_preview_flat_prototype_cost_bytes_old,
		draw_unit_preview_flat_prototype_cost_bytes_new,
		draw_unit_preview_flat_prototype_cost_bytes_length
	);

	// call non prototyped components cost calculation

	write_call(0x0043704C, (int)calculateNotPrototypedComponentsCostForDesign);

	// correct stack pointer after the call as we passed 6 parameters instead of 5

	int calculateNotPrototypedComponentsCostForDesign_stack_pointer_bytes_length = 0x3;
/*
0:  83 c4 20                add    esp,0x20
*/
	byte calculateNotPrototypedComponentsCostForDesign_stack_pointer_bytes_old[] = { 0x83, 0xC4, 0x20 };
/*
0:  83 c4 24                add    esp,0x24
*/
	byte calculateNotPrototypedComponentsCostForDesign_stack_pointer_bytes_new[] = { 0x83, 0xC4, 0x24 };
	write_bytes
	(
		0x0043709E,
		calculateNotPrototypedComponentsCostForDesign_stack_pointer_bytes_old,
		calculateNotPrototypedComponentsCostForDesign_stack_pointer_bytes_new,
		calculateNotPrototypedComponentsCostForDesign_stack_pointer_bytes_length
	);

	// in veh_cost

	// change call for base_cost to calculate non prototyped components cost

	write_call(0x005C198D, (int)calculateNotPrototypedComponentsCostForProduction);

}

/*
Adjusts mineral contribution based on INDUSTRY rating.
*/
void patch_mineral_contribution()
{
	// adjust artifact mineral contribution

	int artifact_mineral_contribution_bytes_length = 0x1f;
/*
0:  8d 14 f6                lea    edx,[esi+esi*8]
3:  53                      push   ebx
4:  8d 0c 56                lea    ecx,[esi+edx*2]
7:  33 d2                   xor    edx,edx
9:  8a 90 91 b8 9a 00       mov    dl,BYTE PTR [eax+0x9ab891]
f:  8b c2                   mov    eax,edx
11: 8d 0c 8e                lea    ecx,[esi+ecx*4]
14: c1 e1 02                shl    ecx,0x2
17: 8d 04 80                lea    eax,[eax+eax*4]
1a: d1 e0                   shl    eax,1
1c: 99                      cdq
1d: 2b c2                   sub    eax,edx
*/
	byte artifact_mineral_contribution_bytes_old[] = { 0x8D, 0x14, 0xF6, 0x53, 0x8D, 0x0C, 0x56, 0x33, 0xD2, 0x8A, 0x90, 0x91, 0xB8, 0x9A, 0x00, 0x8B, 0xC2, 0x8D, 0x0C, 0x8E, 0xC1, 0xE1, 0x02, 0x8D, 0x04, 0x80, 0xD1, 0xE0, 0x99, 0x2B, 0xC2 };
/*
0:  53                      push   ebx
1:  31 d2                   xor    edx,edx
3:  8a 90 91 b8 9a 00       mov    dl,BYTE PTR [eax+0x9ab891]
9:  52                      push   edx
a:  e8 fd ff ff ff          call   c <_main+0xc>     ; getActiveFactionMineralCostFactor
f:  5a                      pop    edx
10: f7 e2                   mul    edx
12: 8d 14 f6                lea    edx,[esi+esi*8]
15: 8d 0c 56                lea    ecx,[esi+edx*2]
18: 8d 0c 8e                lea    ecx,[esi+ecx*4]
1b: c1 e1 02                shl    ecx,0x2
1e: 90                      nop
*/
	byte artifact_mineral_contribution_bytes_new[] = { 0x53, 0x31, 0xD2, 0x8A, 0x90, 0x91, 0xB8, 0x9A, 0x00, 0x52, 0xE8, 0xFD, 0xFF, 0xFF, 0xFF, 0x5A, 0xF7, 0xE2, 0x8D, 0x14, 0xF6, 0x8D, 0x0C, 0x56, 0x8D, 0x0C, 0x8E, 0xC1, 0xE1, 0x02, 0x90 };
	write_bytes
	(
		0x005996C9,
		artifact_mineral_contribution_bytes_old,
		artifact_mineral_contribution_bytes_new,
		artifact_mineral_contribution_bytes_length
	);

	write_call(0x005996D3, (int)getActiveFactionMineralCostFactor);

	// display artifact mineral contribution for project

	write_call(0x00594D14, (int)displayArtifactMineralContributionInformation);

	// display artifact mineral contribution for prototype

	write_call(0x00594D8C, (int)displayArtifactMineralContributionInformation);

}

/*
Displays base production for pact faction bases same way as for infiltration.
*/
void patch_pact_base_map_production_display()
{
	write_call(0x0046836E, (int)modifiedSpyingForPactBaseProductionDisplay);

}

/*
Introduce a hook to modify probe action risks.
*/
void patch_modified_probe_action_risks()
{
	int modified_probe_action_risk_hook_bytes_length = 0x13;
/*
0:  8b 55 08                mov    edx,DWORD PTR [ebp+0x8]
3:  6a 00                   push   0x0
5:  6a 01                   push   0x1
7:  52                      push   edx
8:  e8 f2 dc 01 00          call   0x1dcff
d:  83 c4 0c                add    esp,0xc
10: 89 45 b4                mov    DWORD PTR [ebp-0x4c],eax
*/
	byte modified_probe_action_risk_hook_bytes_old[] = { 0x8B, 0x55, 0x08, 0x6A, 0x00, 0x6A, 0x01, 0x52, 0xE8, 0xF2, 0xDC, 0x01, 0x00, 0x83, 0xC4, 0x0C, 0x89, 0x45, 0xB4 };
/*
0:  89 45 b4                mov    DWORD PTR [ebp-0x4c],eax
3:  8d 45 d0                lea    eax,[ebp-0x30]
6:  50                      push   eax
7:  ff 75 e8                push   DWORD PTR [ebp-0x18]
a:  e8 fd ff ff ff          call   c <_main+0xc>
f:  83 c4 08                add    esp,0x8
...
*/
	byte modified_probe_action_risk_hook_bytes_new[] = { 0x89, 0x45, 0xB4, 0x8D, 0x45, 0xD0, 0x50, 0xFF, 0x75, 0xE8, 0xE8, 0xFD, 0xFF, 0xFF, 0xFF, 0x83, 0xC4, 0x08, 0x90 };
	write_bytes
	(
		0x005A3141,
		modified_probe_action_risk_hook_bytes_old,
		modified_probe_action_risk_hook_bytes_new,
		modified_probe_action_risk_hook_bytes_length
	);

	write_call(0x005A314B, (int)modifiedProbeActionRisk);

}

/*
Patches Infiltrate Datalinks option for infiltration expiration.
*/
void patch_infiltrate_datalinks()
{
	// always display Infiltrate Datalinks option

	int infiltrate_datalinks_option_always_on_bytes_length = 0x2;
/*
0:  75 14                   jne    0x16
*/
	byte infiltrate_datalinks_option_always_on_bytes_old[] = { 0x75, 0x14 };
/*
...
*/
	byte infiltrate_datalinks_option_always_on_bytes_new[] = { 0x90, 0x90 };
	write_bytes
	(
		0x0059F90F,
		infiltrate_datalinks_option_always_on_bytes_old,
		infiltrate_datalinks_option_always_on_bytes_new,
		infiltrate_datalinks_option_always_on_bytes_length
	);

	// adds number of planted devices information to Infiltrate Datalinks option text

	write_call(0x0059F8ED, (int)modifiedInfiltrateDatalinksOptionTextGet);

	// always set infiltrator flag even if it is already set

	int always_set_infiltrator_flag_bytes_length = 0x2;
/*
0:  75 23                   jne    0x25
*/
	byte always_set_infiltrator_flag_bytes_old[] = { 0x75, 0x23 };
/*
...
*/
	byte always_set_infiltrator_flag_bytes_new[] = { 0x90, 0x90 };
	write_bytes
	(
		0x0059FB9C,
		always_set_infiltrator_flag_bytes_old,
		always_set_infiltrator_flag_bytes_new,
		always_set_infiltrator_flag_bytes_length
	);

	// wrap set_treaty call for infiltration flag to also set number of devices

	write_call(0x0059FBA7, (int)modifiedSetTreatyForInfiltrationExpiration);

}

/*
Disable setting knowledge price over the credits.
*/
void patch_min_knowledge_price()
{
	int disable_min_knowledge_price_bytes_length = 0x2;
/*
0:  8b c1                   mov    eax,ecx
*/
	byte disable_min_knowledge_price_bytes_old[] = { 0x8B, 0xC1 };
/*
...
*/
	byte disable_min_knowledge_price_bytes_new[] = { 0x90, 0x90 };
	write_bytes
	(
		0x00540A35,
		disable_min_knowledge_price_bytes_old,
		disable_min_knowledge_price_bytes_new,
		disable_min_knowledge_price_bytes_length
	);

}

/*
Search right base for returned sea probe.
*/
void patch_find_returned_probe_base()
{
	int find_returned_probe_base_bytes_length = 0x1;
	/*
	0:  53                      push   ebx
	*/
	byte find_returned_probe_base_bytes_old[] = { 0x53 };
	/*
	0:  50                      push   eax
	*/
	byte find_returned_probe_base_bytes_new[] = { 0x50 };
	write_bytes
	(
		0x005A430F,
		find_returned_probe_base_bytes_old,
		find_returned_probe_base_bytes_new,
		find_returned_probe_base_bytes_length
	);

	write_call(0x005A432C, (int)modifiedReturnProbeBaseFind2);

}

/*
Wraps best_defender call.
*/
void patch_best_defender()
{
	write_call(0x00506D07, (int)modifiedBestDefender);

}

/*
Fixes global variable when action_destroy is called.
*/
void patch_action_destroy()
{
	write_call(0x004CAA7C, (int)modifiedVehSkipForActionDestroy);
}

/*
Requests for break treaty before fight.
*/
void patch_break_treaty_before_fight()
{
	write_call(0x00506ADE, (int)modifiedBattleFight2);
	write_call(0x00568B1C, (int)modifiedBattleFight2);
	write_call(0x005697AC, (int)modifiedBattleFight2);
	write_call(0x0056A2E2, (int)modifiedBattleFight2);

}

/*
Displays TALENT icon in SE dialog.
*/
void patch_talent_display()
{
	// in SocialWin__draw_social

	// do not skip TALENT when counting icon display width

	int talent_count_width_bytes_length = 0xb;
/*
0:  3d d8 b2 94 00          cmp    eax,0x94b2d8
5:  0f 84 e9 00 00 00       je     0xf4
*/
	byte talent_count_width_bytes_old[] = { 0x3D, 0xD8, 0xB2, 0x94, 0x00, 0x0F, 0x84, 0xE9, 0x00, 0x00, 0x00 };
/*
...
*/
	byte talent_count_width_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	write_bytes
	(
		0x004AF509,
		talent_count_width_bytes_old,
		talent_count_width_bytes_new,
		talent_count_width_bytes_length
	);

	// do not skip TALENT when displaying icons

	int talent_display_icon_bytes_length = 0xb;
/*
0:  3d d8 b2 94 00          cmp    eax,0x94b2d8
5:  0f 84 d9 01 00 00       je     0x1e4
*/
	byte talent_display_icon_bytes_old[] = { 0x3D, 0xD8, 0xB2, 0x94, 0x00, 0x0F, 0x84, 0xD9, 0x01, 0x00, 0x00 };
/*
...
*/
	byte talent_display_icon_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	write_bytes
	(
		0x004AF67B,
		talent_display_icon_bytes_old,
		talent_display_icon_bytes_new,
		talent_display_icon_bytes_length
	);

	// do not shift icon index by -1 for all effects starting from TALENT and above

	int talent_index_shift_bytes_length = 0x7;
/*
0:  3d d8 b2 94 00          cmp    eax,0x94b2d8
5:  7d 25                   jge    0x2c
*/
	byte talent_index_shift_bytes_old[] = { 0x3D, 0xD8, 0xB2, 0x94, 0x00, 0x7D, 0x25 };
/*
...
*/
	byte talent_index_shift_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	write_bytes
	(
		0x004AF7F3,
		talent_index_shift_bytes_old,
		talent_index_shift_bytes_new,
		talent_index_shift_bytes_length
	);

	// calculate sprite offset

	int calculate_sprite_offset_bytes_length = 0x28;
/*
0:  33 c9                   xor    ecx,ecx
2:  83 fe 01                cmp    esi,0x1
5:  0f 9d c1                setge  cl
8:  41                      inc    ecx
9:  6a 01                   push   0x1
b:  03 c1                   add    eax,ecx
d:  6a 01                   push   0x1
f:  8d 0c 80                lea    ecx,[eax+eax*4]
12: 8d 04 48                lea    eax,[eax+ecx*2]
15: 8d 8c 87 20 2f 00 00    lea    ecx,[edi+eax*4+0x2f20]
1c: 8b 45 e8                mov    eax,DWORD PTR [ebp-0x18]
1f: 50                      push   eax
20: 33 c0                   xor    eax,eax
22: 8a 41 08                mov    al,BYTE PTR [ecx+0x8]
25: 53                      push   ebx
26: 50                      push   eax
27: 52                      push   edx
*/
	byte calculate_sprite_offset_bytes_old[] = { 0x33, 0xC9, 0x83, 0xFE, 0x01, 0x0F, 0x9D, 0xC1, 0x41, 0x6A, 0x01, 0x03, 0xC1, 0x6A, 0x01, 0x8D, 0x0C, 0x80, 0x8D, 0x04, 0x48, 0x8D, 0x8C, 0x87, 0x20, 0x2F, 0x00, 0x00, 0x8B, 0x45, 0xE8, 0x50, 0x33, 0xC0, 0x8A, 0x41, 0x08, 0x53, 0x50, 0x52 };
/*
0:  6a 01                   push   0x1
2:  6a 01                   push   0x1
4:  ff 75 e8                push   DWORD PTR [ebp-0x18]
7:  53                      push   ebx
8:  52                      push   edx
9:  56                      push   esi
a:  50                      push   eax
b:  e8 fd ff ff ff          call   d <_main+0xd>
10: 83 c4 08                add    esp,0x8
13: 89 c1                   mov    ecx,eax
15: 5a                      pop    edx
16: 31 c0                   xor    eax,eax
18: 8a 41 08                mov    al,BYTE PTR [ecx+0x8]
1b: 50                      push   eax
1c: 52                      push   edx
*/
	byte calculate_sprite_offset_bytes_new[] = { 0x6A, 0x01, 0x6A, 0x01, 0xFF, 0x75, 0xE8, 0x53, 0x52, 0x56, 0x50, 0xE8, 0xFD, 0xFF, 0xFF, 0xFF, 0x83, 0xC4, 0x08, 0x89, 0xC1, 0x5A, 0x31, 0xC0, 0x8A, 0x41, 0x08, 0x50, 0x52, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	write_bytes
	(
		0x004AF825,
		calculate_sprite_offset_bytes_old,
		calculate_sprite_offset_bytes_new,
		calculate_sprite_offset_bytes_length
	);

	write_call(0x004AF825 + 0xb, (int)modifiedSocialWinDrawSocialCalculateSpriteOffset);

	// do not skip TALENT when counting icons in description

	int talent_count_icon_description_bytes_length = 0xc;
/*
0:  81 fa d8 b2 94 00       cmp    edx,0x94b2d8
6:  0f 84 ca 01 00 00       je     0x1d6
*/
	byte talent_count_icon_description_bytes_old[] = { 0x81, 0xFA, 0xD8, 0xB2, 0x94, 0x00, 0x0F, 0x84, 0xCA, 0x01, 0x00, 0x00 };
/*
...
*/
	byte talent_count_icon_description_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	write_bytes
	(
		0x004B122E,
		talent_count_icon_description_bytes_old,
		talent_count_icon_description_bytes_new,
		talent_count_icon_description_bytes_length
	);

	// do not skip TALENT when displaying icons in description

	int talent_display_icon_description_bytes_length = 0xc;
/*
0:  81 fa d8 b2 94 00       cmp    edx,0x94b2d8
6:  0f 84 8d 02 00 00       je     0x299
*/
	byte talent_display_icon_description_bytes_old[] = { 0x81, 0xFA, 0xD8, 0xB2, 0x94, 0x00, 0x0F, 0x84, 0x8D, 0x02, 0x00, 0x00 };
/*
...
*/
	byte talent_display_icon_description_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	write_bytes
	(
		0x004B149B,
		talent_display_icon_description_bytes_old,
		talent_display_icon_description_bytes_new,
		talent_display_icon_description_bytes_length
	);

}

/*
Jams effect icons closer together for 5 icons to fit model box.
*/
void patch_compact_effect_icons()
{
	// === in SE dialog under model name ===

	// sprite display position shifts before and after

	int shiftBefore = -2;
	int shiftAfter = -4;
//	int shiftInitial = 0;
	int shiftCombined = shiftBefore + shiftAfter;

	// modify effect sprite width for the purpose total diplayed width

	int effect_sprite_width_bytes_length = 0x3;
	/*
	0:  8d 51 01                lea    edx,[ecx+0x1]
	*/
	byte effect_sprite_width_bytes_old[] = { 0x8D, 0x51, 0x01 };
	/*
	0:  8d 51 f7                lea    edx,[ecx+<1 + shiftCombined>]
	...
	*/
	byte effect_sprite_width_bytes_new[] = { 0x8D, 0x51, (byte)((1 + shiftCombined) & 0xFF) };
	write_bytes
	(
		0x004AF628,
		effect_sprite_width_bytes_old,
		effect_sprite_width_bytes_new,
		effect_sprite_width_bytes_length
	);

//	// modify starting horizontal position
//
//	int starting_horizontal_position_bytes_length = 0x9;
//	/*
//	0:  8b d8                   mov    ebx,eax
//	2:  8b 45 d4                mov    eax,DWORD PTR [ebp-0x2c]
//	5:  d1 fb                   sar    ebx,1
//	7:  03 d8                   add    ebx,eax
//	*/
//	byte starting_horizontal_position_bytes_old[] = { 0x8B, 0xD8, 0x8B, 0x45, 0xD4, 0xD1, 0xFB, 0x03, 0xD8 };
//	/*
//	0:  8b 5d d4                mov    ebx,DWORD PTR [ebp-0x2c]
//	3:  83 c3 1a                add    ebx,<shiftInitial>
//	...
//	*/
//	byte starting_horizontal_position_bytes_new[] = { 0x8B, 0x5D, 0xD4, 0x83, 0xC3, (byte)shiftInitial, 0x90, 0x90, 0x90 };
//	write_bytes
//	(
//		0x004AF64E,
//		starting_horizontal_position_bytes_old,
//		starting_horizontal_position_bytes_new,
//		starting_horizontal_position_bytes_length
//	);
//
	// modify current horizontal position before effect sprite

	int horizontal_position_before_effect_bytes_length = 0x7;
	/*
	...
	*/
	byte horizontal_position_before_effect_bytes_old[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	/*
	0:  8d 5b 01                lea    ebx,[ebx+<shiftBefore>]
	...
	*/
	byte horizontal_position_before_effect_bytes_new[] = { 0x8D, 0x5B, (byte)((shiftBefore) & 0xFF), 0x90, 0x90, 0x90, 0x90 };
	write_bytes
	(
		0x004AF7F3,
		horizontal_position_before_effect_bytes_old,
		horizontal_position_before_effect_bytes_new,
		horizontal_position_before_effect_bytes_length
	);

	// modify current horizontal position after effect sprite

	int horizontal_position_after_effect_bytes_length = 0x4;
	/*
	0:  8d 5c 0b 01             lea    ebx,[ebx+ecx*1+0x1]
	*/
	byte horizontal_position_after_effect_bytes_old[] = { 0x8D, 0x5C, 0x0B, 0x01 };
	/*
	0:  8d 5c 0b f7             lea    ebx,[ebx+ecx*1+<1 + shiftAfter>]
	*/
	byte horizontal_position_after_effect_bytes_new[] = { 0x8D, 0x5C, 0x0B, (byte)((1 + shiftAfter) & 0xFF) };
	write_bytes
	(
		0x004AF858,
		horizontal_position_after_effect_bytes_old,
		horizontal_position_after_effect_bytes_new,
		horizontal_position_after_effect_bytes_length
	);

	// === in description window when hovering mouse over SE choice ===

	// vertical positions and steps

	int descriptionVerticalStepOriginal = 26;
	int descriptionVerticalStep = 20;
	int topMarginExtra = -2;
	int topMargin = (descriptionVerticalStep - descriptionVerticalStepOriginal) / 2 + topMarginExtra;

	// set top margin to half of the vertical step difference

	int description_top_margin_bytes_length = 0x8;
	/*
	0:  99                      cdq
	1:  2b c2                   sub    eax,edx
	3:  8b 55 d4                mov    edx,DWORD PTR [ebp-0x2c]
	6:  d1 f8                   sar    eax,1
	*/
	byte description_top_margin_bytes_old[] = { 0x99, 0x2B, 0xC2, 0x8B, 0x55, 0xD4, 0xD1, 0xF8 };
	/*
	0:  b8 01 00 00 00          mov    eax,<top margin>
	5:  8b 55 d4                mov    edx,DWORD PTR [ebp-0x2c]
	*/
	byte description_top_margin_bytes_new[] = { 0xB8, (byte)((topMargin >> 0) & 0xFF), (byte)((topMargin >> 8) & 0xFF), (byte)((topMargin >> 16) & 0xFF), (byte)((topMargin >> 24) & 0xFF), 0x8B, 0x55, 0xD4 };
	write_bytes
	(
		0x004B1458,
		description_top_margin_bytes_old,
		description_top_margin_bytes_new,
		description_top_margin_bytes_length
	);

	// change vertical step in description

	int description_vertical_step_bytes_length = 0x7;
	/*
	0:  8b 97 34 2e 00 00       mov    edx,DWORD PTR [edi+0x2e34]
	*/
	byte description_vertical_step_bytes_old[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	/*
	0:  c7 45 d4 01 00 00 00    mov    DWORD PTR [ebp-0x2c],<vertical step>
	*/
	byte description_vertical_step_bytes_new[] = { 0xC7, 0x45, 0xD4, (byte)(descriptionVerticalStep & 0xFF), 0x00, 0x00, 0x00 };
	write_bytes
	(
		0x004B149B,
		description_vertical_step_bytes_old,
		description_vertical_step_bytes_new,
		description_vertical_step_bytes_length
	);

}

/*
Wraps tech_research to modify SE RESEARCH bonus.
*/
void patch_research_bonus_multiplier()
{
	write_call(0x004F4F9C, (int)modified_tech_research);

}

/*
Removes fungal tower defense bonus.
*/
void patch_remove_fungal_tower_defense_bonus()
{
	// fungal tower defense bonus

	int disable_fungal_tower_defense_bonus_bytes_length = 0x2;
/*
0:  75 09                   jne    0xb
*/
	byte disable_fungal_tower_defense_bonus_bytes_old[] = { 0x75, 0x09 };
/*
0:  eb 09                   jmp    0xb
*/
	byte disable_fungal_tower_defense_bonus_bytes_new[] = { 0xEB, 0x09 };
	write_bytes
	(
		0x00501D0F,
		disable_fungal_tower_defense_bonus_bytes_old,
		disable_fungal_tower_defense_bonus_bytes_new,
		disable_fungal_tower_defense_bonus_bytes_length
	);

	// fungal tower defense bonus display

	int disable_fungal_tower_defense_bonus_display_bytes_length = 0x2;
/*
0:  75 39                   jne    0x3b
*/
	byte disable_fungal_tower_defense_bonus_display_bytes_old[] = { 0x75, 0x39 };
/*
0:  eb 39                   jmp    0x3b
*/
	byte disable_fungal_tower_defense_bonus_display_bytes_new[] = { 0xEB, 0x39 };
	write_bytes
	(
		0x005020C7,
		disable_fungal_tower_defense_bonus_display_bytes_old,
		disable_fungal_tower_defense_bonus_display_bytes_new,
		disable_fungal_tower_defense_bonus_display_bytes_length
	);

}

/*
aliens fight at half strength until this turn.
*/
void patch_aliens_fight_half_strength_unit_turn(int turn)
{
       int aliens_fight_half_strength_unit_turn_bytes_length = 0x7;
/*
0:  83 3d d4 64 9a 00 0f    cmp    DWORD PTR ds:0x9a64d4,0xf
*/
       byte aliens_fight_half_strength_unit_turn_bytes_old[] = { 0x83, 0x3D, 0xD4, 0x64, 0x9A, 0x00, 0x0F };
/*
0:  83 3d d4 64 9a 00 01    cmp    DWORD PTR ds:0x9a64d4,<turn>
*/
       byte aliens_fight_half_strength_unit_turn_bytes_new[] = { 0x83, 0x3D, 0xD4, 0x64, 0x9A, 0x00, (byte)turn };
       write_bytes
       (
		   0x00507C22,
		   aliens_fight_half_strength_unit_turn_bytes_old,
		   aliens_fight_half_strength_unit_turn_bytes_new,
		   aliens_fight_half_strength_unit_turn_bytes_length
       );

}

/*
habitation facilities do not stop growth
*/
void patch_habitation_facility_disable_explicit_population_limit()
{
	Rules->pop_limit_wo_hab_complex = 1000;
	Rules->pop_limit_wo_hab_dome = 1000;
	int habitation_complex_does_not_stop_growth_bytes_length = 0x5;
	/*
	0:  a1 6c 97 94 00          mov    eax,ds:0x94976c
	*/
	byte habitation_complex_does_not_stop_growth_bytes_old[] = { 0xA1, 0x6C, 0x97, 0x94, 0x00 };
	/*
	0:  b8 00 10 00 00          mov    eax,0x1000
	*/
	byte habitation_complex_does_not_stop_growth_bytes_new[] = { 0xB8, 0x00, 0x10, 0x00, 0x00 };
	write_bytes
	(
		0x004EF577,
		habitation_complex_does_not_stop_growth_bytes_old,
		habitation_complex_does_not_stop_growth_bytes_new,
		habitation_complex_does_not_stop_growth_bytes_length
	);

	int habitation_dome_does_not_stop_growth_bytes_length = 0x6;
	/*
	0:  8b 0d 70 97 94 00       mov    ecx,DWORD PTR ds:0x949770
	*/
	byte habitation_dome_does_not_stop_growth_bytes_old[] = { 0x8B, 0x0D, 0x70, 0x97, 0x94, 0x00 };
	/*
	0:  b9 00 10 00 00          mov    ecx,0x1000
	...
	*/
	byte habitation_dome_does_not_stop_growth_bytes_new[] = { 0xB9, 0x00, 0x10, 0x00, 0x00, 0x90 };
	write_bytes
	(
		0x004EF5AC,
		habitation_dome_does_not_stop_growth_bytes_old,
		habitation_dome_does_not_stop_growth_bytes_new,
		habitation_dome_does_not_stop_growth_bytes_length
	);

}

/*
modified base GROWTH
*/
void patch_modified_base_growth()
{
	// patch current_base_GROWTH

	write_call(0x004E9BB1, (int)modifiedBaseGrowth);

	// patch nutrient cost_factor

	int nutrient_cost_factor_bytes_length = 0x1c;
	/*
	0:  8b cf                   mov    ecx,edi
	2:  c1 e1 06                shl    ecx,0x6
	5:  03 cf                   add    ecx,edi
	7:  8d 14 4f                lea    edx,[edi+ecx*2]
	a:  8d 04 d7                lea    eax,[edi+edx*8]
	d:  8d 0c 47                lea    ecx,[edi+eax*2]
	10: 8b 7d 10                mov    edi,DWORD PTR [ebp+0x10]
	13: 85 ff                   test   edi,edi
	15: 8b 1c 8d 44 cc 96 00    mov    ebx,DWORD PTR [ecx*4+0x96cc44]
	*/
	byte nutrient_cost_factor_bytes_old[] = { 0x8B, 0xCF, 0xC1, 0xE1, 0x06, 0x03, 0xCF, 0x8D, 0x14, 0x4F, 0x8D, 0x04, 0xD7, 0x8D, 0x0C, 0x47, 0x8B, 0x7D, 0x10, 0x85, 0xFF, 0x8B, 0x1C, 0x8D, 0x44, 0xCC, 0x96, 0x00 };
	/*
	0:  8b 4d 10                mov    ecx,DWORD PTR [ebp+0x10]
	3:  51                      push   ecx
	4:  57                      push   edi
	5:  e8 fd ff ff ff          call   7 <_main+0x7>
	a:  83 c4 08                add    esp,0x8
	d:  89 c3                   mov    ebx,eax
	f:  90                      nop
	10: 90                      nop
	11: 90                      nop
	12: 90                      nop
	13: 90                      nop
	14: 90                      nop
	15: 90                      nop
	16: 90                      nop
	17: 8b 7d 10                mov    edi,DWORD PTR [ebp+0x10]
	1a: 85 ff                   test   edi,edi
	*/
	byte nutrient_cost_factor_bytes_new[] = { 0x8B, 0x4D, 0x10, 0x51, 0x57, 0xE8, 0xFD, 0xFF, 0xFF, 0xFF, 0x83, 0xC4, 0x08, 0x89, 0xC3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x8B, 0x7D, 0x10, 0x85, 0xFF };
	write_bytes
	(
		0x004E450D,
		nutrient_cost_factor_bytes_old,
		nutrient_cost_factor_bytes_new,
		nutrient_cost_factor_bytes_length
	);

	write_call(0x004E450D + 0x5, (int)modifiedNutrientCostFactorSEGrowth);

}

/*
unit upgrade does not require whole turn
*/
void patch_unit_upgrade_ignores_movement()
{
	// disable no movement requirement

	int unit_upgrade_disable_no_movement_requirement_1_bytes_length = 0x2;

	/*
	0:  84 c9                   test   cl,cl
	*/
	byte unit_upgrade_disable_no_movement_requirement_1_bytes_old[] = { 0x84, 0xC9 };

	/*
	0:  30 c9                   xor    cl,cl
	*/
	byte unit_upgrade_disable_no_movement_requirement_1_bytes_new[] = { 0x30, 0xC9 };

	write_bytes
	(
		0x004D078A,
		unit_upgrade_disable_no_movement_requirement_1_bytes_old,
		unit_upgrade_disable_no_movement_requirement_1_bytes_new,
		unit_upgrade_disable_no_movement_requirement_1_bytes_length
	);

	int unit_upgrade_disable_no_movement_requirement_2_bytes_length = 0x2;

	/*
	0:  84 c9                   test   cl,cl
	*/
	byte unit_upgrade_disable_no_movement_requirement_2_bytes_old[] = { 0x84, 0xC9 };

	/*
	0:  30 c9                   xor    cl,cl
	*/
	byte unit_upgrade_disable_no_movement_requirement_2_bytes_new[] = { 0x30, 0xC9 };

	write_bytes
	(
		0x004D0928,
		unit_upgrade_disable_no_movement_requirement_2_bytes_old,
		unit_upgrade_disable_no_movement_requirement_2_bytes_new,
		unit_upgrade_disable_no_movement_requirement_2_bytes_length
	);

	// do not skip vehicle after upgrade

	int unit_upgrade_disable_skip_1_bytes_length = 0x5;

	/*
	0:  e8 d8 0a 0f 00          call   0xf0add
	*/
	byte unit_upgrade_disable_skip_1_bytes_old[] = { 0xE8, 0xD8, 0x0A, 0x0F, 0x00 };

	/*
	...
	*/
	byte unit_upgrade_disable_skip_1_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };

	write_bytes
	(
		0x004D1243,
		unit_upgrade_disable_skip_1_bytes_old,
		unit_upgrade_disable_skip_1_bytes_new,
		unit_upgrade_disable_skip_1_bytes_length
	);

	int unit_upgrade_disable_skip_2_bytes_length = 0x5;

	/*
	0:  e8 6e 04 0f 00          call   0xf0473
	*/
	byte unit_upgrade_disable_skip_2_bytes_old[] = { 0xE8, 0x6E, 0x04, 0x0F, 0x00 };

	/*
	...
	*/
	byte unit_upgrade_disable_skip_2_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };

	write_bytes
	(
		0x004D18AD,
		unit_upgrade_disable_skip_2_bytes_old,
		unit_upgrade_disable_skip_2_bytes_new,
		unit_upgrade_disable_skip_2_bytes_length
	);

	int unit_upgrade_disable_skip_3_bytes_length = 0x5;

	/*
	0:  e8 a2 02 0f 00          call   0xf02a7
	*/
	byte unit_upgrade_disable_skip_3_bytes_old[] = { 0xE8, 0xA2, 0x02, 0x0F, 0x00 };

	/*
	...
	*/
	byte unit_upgrade_disable_skip_3_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };

	write_bytes
	(
		0x004D1A79,
		unit_upgrade_disable_skip_3_bytes_old,
		unit_upgrade_disable_skip_3_bytes_new,
		unit_upgrade_disable_skip_3_bytes_length
	);

}

/*
; When one former starts terraforming action all idle formers in same tile do too.
; May have unwanted consequenses of intercepting idle formers.
*/
void patch_group_terraforming()
{
    write_call(0x004CF766, (int)modifiedActionTerraform);

}

/*
Disables land artillery bombard from sea.
*/
void patch_disable_land_artillery_bombard_from_sea()
{
    write_call(0x0046D42F, (int)modifiedActionMoveForArtillery);
}

/*
Disables air transport unload everywhere.
*/
void patch_disable_air_transport_unload_everywhere()
{
    write_call(0x004D0509, (int)modifiedVehicleCargoForAirTransportUnload);
}

//void patch_base_production()
//{
//    write_call_over(0x004E61D0, (int)modifiedBaseProductionChoice);
//}
//
void patch_enemy_move()
{
    write_call_over(0x00579362, (int)modified_enemy_move);
}

void patch_faction_upkeep()
{
    write_call(0x00528214, (int)modifiedFactionUpkeep);
    write_call(0x0052918F, (int)modifiedFactionUpkeep);
}

/*
Normally vanilla discards move_to order to not allowed territory.
Disabling this restriction to allow easier AI navigation.
*/
void patch_disable_move_territory_restrictions()
{
	int disable_move_territory_restriction_bytes_length = 0x7;

	/*
	0:  80 be 39 28 95 00 18    cmp    BYTE PTR [esi+0x952839],0x18
	*/
	byte disable_move_territory_restriction_bytes_old[] = { 0x80, 0xBE, 0x39, 0x28, 0x95, 0x00, 0x18 };

	/*
	0:  80 be 39 28 95 00 ff    cmp    BYTE PTR [esi+0x952839],0xff
	*/
	byte disable_move_territory_restriction_bytes_new[] = { 0x80, 0xBE, 0x39, 0x28, 0x95, 0x00, 0xFF };

	write_bytes
	(
		0x00594A0B,
		disable_move_territory_restriction_bytes_old,
		disable_move_territory_restriction_bytes_new,
		disable_move_territory_restriction_bytes_length
	);

}

void patch_tech_cost()
{
	// intercept tech_rate to substitute alternative tech cost
	
	write_call(0x4452D5, (int)modifiedTechRate);
	write_call(0x498D26, (int)modifiedTechRate);
	write_call(0x4A77DA, (int)modifiedTechRate);
	write_call(0x521872, (int)modifiedTechRate);
	write_call(0x5218BE, (int)modifiedTechRate);
	write_call(0x5581E9, (int)modifiedTechRate);
	write_call(0x5BEA4D, (int)modifiedTechRate);
	write_call(0x5BEAC7, (int)modifiedTechRate);
	
	// intercept tech_pick to recompute tech cost based on tech ID
	
    write_call(0x00515080, (int)modifiedTechPick);
    write_call(0x005BE4AC, (int)modifiedTechPick);
    
}

void patch_interceptor_scramble_fix_display()
{
	// modify the weapon/armor display
	
    write_call(0x00422600, (int)modifiedBattleReportItemNameDisplay);
    write_call(0x0042277A, (int)modifiedBattleReportItemValueDisplay);
    
}

void patch_scorched_earth()
{
	// wrap reset_territory when base is captured or killed
	
    write_call(0x0050D16A, (int)modifiedResetTerritory);
    write_call(0x004E5A40, (int)modifiedResetTerritory);
    
}

void patch_orbital_yield_limit()
{
	int orbital_yield_limit_bytes_length = 0x13;

	/*
	0:  8b 15 30 ea 90 00       mov    edx,DWORD PTR ds:0x90ea30
	6:  0f be 42 06             movsx  eax,BYTE PTR [edx+0x6]
	a:  8b 0c 17                mov    ecx,DWORD PTR [edi+edx*1]
	d:  3b c1                   cmp    eax,ecx
	f:  7c 02                   jl     0x13
	11: 8b c1                   mov    eax,ecx
	*/
	byte orbital_yield_limit_bytes_old[] = { 0x8B, 0x15, 0x30, 0xEA, 0x90, 0x00, 0x0F, 0xBE, 0x42, 0x06, 0x8B, 0x0C, 0x17, 0x3B, 0xC1, 0x7C, 0x02, 0x8B, 0xC1 };

	/*
	0:  e8 fd ff ff ff          call   2 <_main+0x2>
	5:  8b 15 30 ea 90 00       mov    edx,DWORD PTR ds:0x90ea30
	b:  8b 0c 17                mov    ecx,DWORD PTR [edi+edx*1]
	e:  39 c8                   cmp    eax,ecx
	10: 0f 4f c1                cmovg  eax,ecx
	*/
	byte orbital_yield_limit_bytes_new[] = { 0xE8, 0xFD, 0xFF, 0xFF, 0xFF, 0x8B, 0x15, 0x30, 0xEA, 0x90, 0x00, 0x8B, 0x0C, 0x17, 0x39, 0xC8, 0x0F, 0x4F, 0xC1 };

	write_bytes
	(
		0x004E8210,
		orbital_yield_limit_bytes_old,
		orbital_yield_limit_bytes_new,
		orbital_yield_limit_bytes_length
	);

    write_call(0x004E8210 + 0x0, (int)modifiedOrbitalYieldLimit);
    
}

void patch_silent_vendetta_warning()
{
    write_call(0x004A050D, (int)modifiedBreakTreaty);
    write_call(0x004CC766, (int)modifiedBreakTreaty);
    write_call(0x004D498F, (int)modifiedBreakTreaty);
    write_call(0x004D5A06, (int)modifiedBreakTreaty);
    write_call(0x004D8334, (int)modifiedBreakTreaty);
    write_call(0x00507117, (int)modifiedBreakTreaty);
    write_call(0x00597551, (int)modifiedBreakTreaty);
    
}

void patch_design_cost_in_rows()
{
	int design_cost_in_rows_bytes_length = 0x3;

	/*
	0:  0f af c7                imul   eax,edi
	*/
	byte design_cost_in_rows_bytes_old[] = { 0x0F, 0xAF, 0xC7 };

	/*
	0:  89 f8                   mov    eax,edi
	...
	*/
	byte design_cost_in_rows_bytes_new[] = { 0x89, 0xF8, 0x90 };

	write_bytes
	(
		0x00436BEF,
		design_cost_in_rows_bytes_old,
		design_cost_in_rows_bytes_new,
		design_cost_in_rows_bytes_length
	);

	int design_cost_in_rows_prototyping_bytes_length = 0x7;

	/*
	0:  0f af 96 14 42 01 00    imul   edx,DWORD PTR [esi+0x14214]
	*/
	byte design_cost_in_rows_prototyping_bytes_old[] = { 0x0F, 0xAF, 0x96, 0x14, 0x42, 0x01, 0x00 };

	/*
	...
	*/
	byte design_cost_in_rows_prototyping_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

	write_bytes
	(
		0x0043707E,
		design_cost_in_rows_prototyping_bytes_old,
		design_cost_in_rows_prototyping_bytes_new,
		design_cost_in_rows_prototyping_bytes_length
	);

	int design_cost_color_bytes_length = 0x5;

	/*
	0:  a1 9c 6d 8c 00          mov    eax,ds:0x8c6d9c
	*/
	byte design_cost_color_bytes_old[] = { 0xA1, 0x9C, 0x6D, 0x8C, 0x00 };

	/*
	0:  b8 ff 00 00 00          mov    eax,0xff
	*/
	byte design_cost_color_bytes_new[] = { 0xB8, 0xFF, 0x00, 0x00, 0x00 };

	write_bytes
	(
		0x00436A67,
		design_cost_color_bytes_old,
		design_cost_color_bytes_new,
		design_cost_color_bytes_length
	);

	int design_cost_color_prototyping_bytes_length = 0x5;

	/*
	0:  a1 ac 6d 8c 00          mov    eax,ds:0x8c6dac
	*/
	byte design_cost_color_prototyping_bytes_old[] = { 0xA1, 0xAC, 0x6D, 0x8C, 0x00 };

	/*
	0:  b8 ff 00 00 00          mov    eax,0xff
	*/
	byte design_cost_color_prototyping_bytes_new[] = { 0xB8, 0xFF, 0x00, 0x00, 0x00 };

	write_bytes
	(
		0x00436FF2,
		design_cost_color_prototyping_bytes_old,
		design_cost_color_prototyping_bytes_new,
		design_cost_color_prototyping_bytes_length
	);

}

void patch_energy_market_crash(int numerator)
{
	int energy_market_crash_bytes_length = 0x9;

	/*
	0:  99                      cdq
	1:  83 e2 03                and    edx,0x3
	4:  03 c2                   add    eax,edx
	6:  c1 f8 02                sar    eax,0x2
	*/
	byte energy_market_crash_bytes_old[] = { 0x99, 0x83, 0xE2, 0x03, 0x03, 0xC2, 0xC1, 0xF8, 0x02 };

	/*
	0:  c1 f8 04                sar    eax,0x4
	3:  6b c0 0c                imul   eax,eax,<numerator>
	...
	*/
	byte energy_market_crash_bytes_new[] = { 0xC1, 0xF8, 0x04, 0x6B, 0xC0, (byte)numerator, 0x90, 0x90, 0x90 };

	write_bytes
	(
		0x00520725,
		energy_market_crash_bytes_old,
		energy_market_crash_bytes_new,
		energy_market_crash_bytes_length
	);

}

void patch_carry_over_minerals()
{
	// disable cutting carry over minerals
	
	int carry_over_minerals_unit_bytes_length = 0x3;

	/*
	0:  39 56 40                cmp    DWORD PTR [esi+0x40],edx
	*/
	byte carry_over_minerals_unit_bytes_old[] = { 0x39, 0x56, 0x40 };

	/*
	0:  39 d2                   cmp    edx,edx
	...
	*/
	byte carry_over_minerals_unit_bytes_new[] = { 0x39, 0xD2, 0x90 };

	write_bytes
	(
		0x004F103E,
		carry_over_minerals_unit_bytes_old,
		carry_over_minerals_unit_bytes_new,
		carry_over_minerals_unit_bytes_length
	);
	
	int carry_over_minerals_facility_bytes_length = 0x3;

	/*
	0:  39 56 40                cmp    DWORD PTR [esi+0x40],edx
	*/
	byte carry_over_minerals_facility_bytes_old[] = { 0x39, 0x56, 0x40 };

	/*
	0:  39 d2                   cmp    edx,edx
	...
	*/
	byte carry_over_minerals_facility_bytes_new[] = { 0x39, 0xD2, 0x90 };

	write_bytes
	(
		0x004F1FC7,
		carry_over_minerals_facility_bytes_old,
		carry_over_minerals_facility_bytes_new,
		carry_over_minerals_facility_bytes_length
	);
	
	// do not clear production flag at production popup
	
	int retain_production_flag_bytes_length = 0xb;

	/*
	0:  8b 41 30                mov    eax,DWORD PTR [ecx+0x30]
	3:  25 ff ff 7f ff          and    eax,0xff7fffff
	8:  89 41 30                mov    DWORD PTR [ecx+0x30],eax
	*/
	byte retain_production_flag_bytes_old[] = { 0x8B, 0x41, 0x30, 0x25, 0xFF, 0xFF, 0x7F, 0xFF, 0x89, 0x41, 0x30 };

	/*
	...
	*/
	byte retain_production_flag_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

	write_bytes
	(
		0x004173AB,
		retain_production_flag_bytes_old,
		retain_production_flag_bytes_new,
		retain_production_flag_bytes_length
	);
	
	// disable penalty after base production
	
    write_call(0x004179A8, (int)modifiedBaseMaking);
    write_call(0x004179BC, (int)modifiedBaseMaking);
    write_call(0x00417A2E, (int)modifiedBaseMaking);
    write_call(0x00417A3F, (int)modifiedBaseMaking);
    write_call(0x004932D3, (int)modifiedBaseMaking);
    write_call(0x004932E5, (int)modifiedBaseMaking);
    write_call(0x0049365A, (int)modifiedBaseMaking);
    write_call(0x0049366B, (int)modifiedBaseMaking);
    write_call(0x004E4850, (int)modifiedBaseMaking);
    write_call(0x004E485F, (int)modifiedBaseMaking);
    write_call(0x004E5B06, (int)modifiedBaseMaking);
    write_call(0x004E5B1B, (int)modifiedBaseMaking);

}

void patch_subversion_allow_stacked_units()
{
	// ignore stack count in probe
	
	int ignore_stack_count_probe_bytes_length = 0x2;

	/*
	0:  3b c6                   cmp    eax,esi
	*/
	byte ignore_stack_count_probe_bytes_old[] = { 0x3B, 0xC6 };

	/*
	0:  39 f6                   cmp    esi,esi
	*/
	byte ignore_stack_count_probe_bytes_new[] = { 0x39, 0xF6 };

	write_bytes
	(
		0x005A1262,
		ignore_stack_count_probe_bytes_old,
		ignore_stack_count_probe_bytes_new,
		ignore_stack_count_probe_bytes_length
	);
	
	// ignore stack count in enemy_move
	
	int ignore_stack_count_enemy_move_bytes_length = 0x3;

	/*
	0:  83 f8 01                cmp    eax,0x1
	*/
	byte ignore_stack_count_enemy_move_bytes_old[] = { 0x83, 0xF8, 0x01 };

	/*
	0:  39 c0                   cmp    eax,eax
	..
	*/
	byte ignore_stack_count_enemy_move_bytes_new[] = { 0x39, 0xC0, 0x90 };

	write_bytes
	(
		0x005784CB,
		ignore_stack_count_enemy_move_bytes_old,
		ignore_stack_count_enemy_move_bytes_new,
		ignore_stack_count_enemy_move_bytes_length
	);
	
	// disable target tile ownership change
	
	int disable_tile_ownership_change_bytes_length = 0x5;

	/*
	0:  e8 18 d9 fe ff          call   0xfffed91d
	*/
	byte disable_tile_ownership_change_bytes_old[] = { 0xE8, 0x18, 0xD9, 0xFE, 0xFF };

	/*
	...
	*/
	byte disable_tile_ownership_change_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };

	write_bytes
	(
		0x005A41F3,
		disable_tile_ownership_change_bytes_old,
		disable_tile_ownership_change_bytes_new,
		disable_tile_ownership_change_bytes_length
	);
	
	// move subverted unit to probe tile
	
	int pull_subverted_vehicle_bytes_length = 0xe;

	/*
	0:  0f bf 86 2a 28 95 00    movsx  eax,WORD PTR [esi+0x95282a]
	7:  0f bf 8e 28 28 95 00    movsx  ecx,WORD PTR [esi+0x952828]
	*/
	byte pull_subverted_vehicle_bytes_old[] = { 0x0F, 0xBF, 0x86, 0x2A, 0x28, 0x95, 0x00, 0x0F, 0xBF, 0x8E, 0x28, 0x28, 0x95, 0x00 };

	/*
	0:  8b 45 10                mov    eax,DWORD PTR [ebp+0x10]
	3:  8b 4d 08                mov    ecx,DWORD PTR [ebp+0x8]
	...
	*/
	byte pull_subverted_vehicle_bytes_new[] = { 0x8B, 0x45, 0x10, 0x8B, 0x4D, 0x08, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

	write_bytes
	(
		0x005A423E,
		pull_subverted_vehicle_bytes_old,
		pull_subverted_vehicle_bytes_new,
		pull_subverted_vehicle_bytes_length
	);
	
    write_call(0x005A4250, (int)modifiedSubveredVehicleDrawTile);
    
}

void patch_mind_control_destroys_unsubverted()
{
    write_call_over(0x005A4AB0, (int)modifiedMindControlCaptureBase);

}

void patch_alternative_subversion_and_mind_control()
{
	// alternative subversion cost
	
	int alternative_subversion_cost_bytes_length = 0x36;

	/*
	0:  8b 80 00 cc 96 00       mov    eax,DWORD PTR [eax+0x96cc00]
	6:  05 20 03 00 00          add    eax,0x320
	b:  99                      cdq
	c:  f7 fe                   idiv   esi
	e:  33 d2                   xor    edx,edx
	10: 8b f0                   mov    esi,eax
	12: 8d 04 49                lea    eax,[ecx+ecx*2]
	15: 8d 0c 81                lea    ecx,[ecx+eax*4]
	18: a1 e8 64 9a 00          mov    eax,ds:0x9a64e8
	1d: 25 ff 00 00 00          and    eax,0xff
	22: 8a 14 8d 91 b8 9a 00    mov    dl,BYTE PTR [ecx*4+0x9ab891]
	29: 8b cb                   mov    ecx,ebx
	2b: 0f af f2                imul   esi,edx
	2e: ba 01 00 00 00          mov    edx,0x1
	33: 89 75 f0                mov    DWORD PTR [ebp-0x10],esi
	*/
	byte alternative_subversion_cost_bytes_old[] = { 0x8B, 0x80, 0x00, 0xCC, 0x96, 0x00, 0x05, 0x20, 0x03, 0x00, 0x00, 0x99, 0xF7, 0xFE, 0x33, 0xD2, 0x8B, 0xF0, 0x8D, 0x04, 0x49, 0x8D, 0x0C, 0x81, 0xA1, 0xE8, 0x64, 0x9A, 0x00, 0x25, 0xFF, 0x00, 0x00, 0x00, 0x8A, 0x14, 0x8D, 0x91, 0xB8, 0x9A, 0x00, 0x8B, 0xCB, 0x0F, 0xAF, 0xF2, 0xBA, 0x01, 0x00, 0x00, 0x00, 0x89, 0x75, 0xF0 };

	/*
	0:  ff 75 10                push   DWORD PTR [ebp+0x10]
	3:  e8 01 00 00 00          call   9 <_main+0x9>
	8:  83 c4 04                add    esp,0x4
	b:  89 c6                   mov    esi,eax
	d:  89 75 f0                mov    DWORD PTR [ebp-0x10],esi
	...
	25: a1 e8 64 9a 00          mov    eax,ds:0x9a64e8
	2a: 25 ff 00 00 00          and    eax,0xff
	2f: 89 d9                   mov    ecx,ebx
	31: ba 01 00 00 00          mov    edx,0x1
	*/
	byte alternative_subversion_cost_bytes_new[] = { 0xFF, 0x75, 0x10, 0xE8, 0x01, 0x00, 0x00, 0x00, 0x83, 0xC4, 0x04, 0x89, 0xC6, 0x89, 0x75, 0xF0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xA1, 0xE8, 0x64, 0x9A, 0x00, 0x25, 0xFF, 0x00, 0x00, 0x00, 0x89, 0xD9, 0xBA, 0x01, 0x00, 0x00, 0x00 };

	write_bytes
	(
		0x005A153A,
		alternative_subversion_cost_bytes_old,
		alternative_subversion_cost_bytes_new,
		alternative_subversion_cost_bytes_length
	);
	
    write_call(0x005A153A + 0x3, (int)getBasicAlternativeSubversionCost);
	
	// disable vanilla PE effect computation - it is already accounted for
	
	int disable_pe_computation_bytes_length = 0x2;

	/*
	0:  85 c0                   test   eax,eax
	*/
	byte disable_pe_computation_bytes_old[] = { 0x85, 0xC0 };

	/*
	0:  31 c0                   xor    eax,eax
	*/
	byte disable_pe_computation_bytes_new[] = { 0x31, 0xC0 };

	write_bytes
	(
		0x005A17CE,
		disable_pe_computation_bytes_old,
		disable_pe_computation_bytes_new,
		disable_pe_computation_bytes_length
	);
	
	// disable vanilla unit plan effect computation - it is already accounted for
	
	int disable_unit_type_computation_bytes_length = 0x2;

	/*
	0:  3c 08                   cmp    al,0x8
	*/
	byte disable_unit_type_computation_bytes_old[] = { 0x3C, 0x08 };

	/*
	0:  31 c0                   xor    eax,eax
	*/
	byte disable_unit_type_computation_bytes_new[] = { 0x31, 0xC0 };

	write_bytes
	(
		0x005A17ED,
		disable_unit_type_computation_bytes_old,
		disable_unit_type_computation_bytes_new,
		disable_unit_type_computation_bytes_length
	);
	
	// ignore units contribution in mind control cost
	
	int alternative_subversion_and_mind_control_ignore_units_bytes_length = 0x2;

	/*
	0:  03 f0                   add    esi,eax
	*/
	byte alternative_subversion_and_mind_control_ignore_units_bytes_old[] = { 0x03, 0xF0 };

	/*
	0:  31 f6                   xor    esi,esi
	...
	*/
	byte alternative_subversion_and_mind_control_ignore_units_bytes_new[] = { 0x31, 0xF6 };

	write_bytes
	(
		0x0059ECD6,
		alternative_subversion_and_mind_control_ignore_units_bytes_old,
		alternative_subversion_and_mind_control_ignore_units_bytes_new,
		alternative_subversion_and_mind_control_ignore_units_bytes_length
	);
	
	// wrap mind_cost to add plain unit cost
	
    write_call(0x0059EEA1, (int)modifiedMindControlCost);
    write_call(0x005A20E8, (int)modifiedMindControlCost);

}

void patch_disable_guaranteed_facilities_destruction()
{
	// alternative subversion cost
	
	int disable_guaranteed_facilities_destruction_bytes_length = 0x3;

	/*
	0:  8b 45 c8                mov    eax,DWORD PTR [ebp-0x38]
	*/
	byte disable_guaranteed_facilities_destruction_bytes_old[] = { 0x8B, 0x45, 0xC8 };

	/*
	0:  83 c8 ff                or     eax,0xffffffff
	*/
	byte disable_guaranteed_facilities_destruction_bytes_new[] = { 0x83, 0xC8, 0xFF };

	write_bytes
	(
		0x0050D05B,
		disable_guaranteed_facilities_destruction_bytes_old,
		disable_guaranteed_facilities_destruction_bytes_new,
		disable_guaranteed_facilities_destruction_bytes_length
	);
	
}

void patch_total_thought_control_no_diplomatic_consequences()
{
	// alternative subversion cost
	
	int total_thought_control_no_diplomatic_consequences_bytes_length = 0x5;

	/*
	0:  e8 9f 6f fb ff          call   0xfffb6fa4
	*/
	byte total_thought_control_no_diplomatic_consequences_bytes_old[] = { 0xE8, 0x9F, 0x6F, 0xFB, 0xFF };

	/*
	...
	*/
	byte total_thought_control_no_diplomatic_consequences_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };

	write_bytes
	(
		0x005A4B8C,
		total_thought_control_no_diplomatic_consequences_bytes_old,
		total_thought_control_no_diplomatic_consequences_bytes_new,
		total_thought_control_no_diplomatic_consequences_bytes_length
	);
	
}

void patch_supply_convoy_and_info_warfare_require_support()
{
	// Supply Convoy and Info Warfare require support
	
	int supply_convoy_and_info_warfare_requires_support_bytes_length = 0x3;

	/*
	0:  80 fa 09                cmp    dl,0x9
	*/
	byte supply_convoy_and_info_warfare_requires_support_bytes_old[] = { 0x80, 0xFA, 0x09 };

	/*
	0:  80 fa 0b                cmp    dl,0xb
	*/
	byte supply_convoy_and_info_warfare_requires_support_bytes_new[] = { 0x80, 0xFA, 0x0B };

	write_bytes
	(
		0x004E980E,
		supply_convoy_and_info_warfare_requires_support_bytes_old,
		supply_convoy_and_info_warfare_requires_support_bytes_new,
		supply_convoy_and_info_warfare_requires_support_bytes_length
	);
	
}

void patch_alternative_support()
{
	// support free units
	
	int support_free_units_bytes_length = 0x4;

	/*
	0:  3b 54 85 c4             cmp    edx,DWORD PTR [ebp+eax*4-0x3c]
	*/
	byte support_free_units_bytes_old[] = { 0x3B, 0x54, 0x85, 0xC4 };

	/*
	0:  83 fa 02                cmp    edx,<free units>
	...
	*/
	byte support_free_units_bytes_new[] = { 0x83, 0xFA, (byte)conf.alternative_support_free_units, 0x90 };

	write_bytes
	(
		0x004E98D3,
		support_free_units_bytes_old,
		support_free_units_bytes_new,
		support_free_units_bytes_length
	);
	
	// support minerals
	
	int support_minerals_bytes_length = 0x2b;

	/*
	0:  8b 56 de                mov    edx,DWORD PTR [esi-0x22]
	3:  8b 0d 1c e9 90 00       mov    ecx,DWORD PTR ds:0x90e91c
	9:  83 fb fc                cmp    ebx,0xfffffffc
	c:  8b 1d 08 ea 90 00       mov    ebx,DWORD PTR ds:0x90ea08
	12: 0f 9e c0                setle  al
	15: 40                      inc    eax
	16: 43                      inc    ebx
	17: 83 ca 10                or     edx,0x10
	1a: 03 c8                   add    ecx,eax
	1c: 89 1d 08 ea 90 00       mov    DWORD PTR ds:0x90ea08,ebx
	22: 89 56 de                mov    DWORD PTR [esi-0x22],edx
	25: 89 0d 1c e9 90 00       mov    DWORD PTR ds:0x90e91c,ecx
	*/
	byte support_minerals_bytes_old[] = { 0x8B, 0x56, 0xDE, 0x8B, 0x0D, 0x1C, 0xE9, 0x90, 0x00, 0x83, 0xFB, 0xFC, 0x8B, 0x1D, 0x08, 0xEA, 0x90, 0x00, 0x0F, 0x9E, 0xC0, 0x40, 0x43, 0x83, 0xCA, 0x10, 0x03, 0xC8, 0x89, 0x1D, 0x08, 0xEA, 0x90, 0x00, 0x89, 0x56, 0xDE, 0x89, 0x0D, 0x1C, 0xE9, 0x90, 0x00 };

	/*
	0:  83 4e de 10             or     DWORD PTR [esi-0x22],0x10
	4:  ff 05 08 ea 90 00       inc    DWORD PTR ds:0x90ea08
	a:  b8 04 00 00 00          mov    eax,0x4
	f:  29 d8                   sub    eax,ebx
	11: f7 25 08 ea 90 00       mul    DWORD PTR ds:0x90ea08
	17: c1 f8 02                sar    eax,0x2
	1a: a3 1c e9 90 00          mov    ds:0x90e91c,eax
	...
	*/
	byte support_minerals_bytes_new[] = { 0x83, 0x4E, 0xDE, 0x10, 0xFF, 0x05, 0x08, 0xEA, 0x90, 0x00, 0xB8, 0x04, 0x00, 0x00, 0x00, 0x29, 0xD8, 0xF7, 0x25, 0x08, 0xEA, 0x90, 0x00, 0xC1, 0xF8, 0x02, 0xA3, 0x1C, 0xE9, 0x90, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

	write_bytes
	(
		0x004E98EE,
		support_minerals_bytes_old,
		support_minerals_bytes_new,
		support_minerals_bytes_length
	);
	
	// intercept method in BaseWin::draw_support to set units support
	
    write_call(0x0040C9D3, (int)interceptBaseWinDrawSupport);
	
	// display unit support
	
	int display_unit_support_bytes_length = 0xc;

	/*
	0:  8b 55 e8                mov    edx,DWORD PTR [ebp-0x18]
	3:  33 c0                   xor    eax,eax
	5:  83 fa fc                cmp    edx,0xfffffffc
	8:  0f 9e c0                setle  al
	b:  40                      inc    eax
	*/
	byte display_unit_support_bytes_old[] = { 0x8B, 0x55, 0xE8, 0x33, 0xC0, 0x83, 0xFA, 0xFC, 0x0F, 0x9E, 0xC0, 0x40 };

	/*
	0:  8b 4d f0                mov    ecx,DWORD PTR [ebp-0x10]
	3:  0f bf 41 08             movsx  eax,WORD PTR [ecx+0x8]
	...
	*/
	byte display_unit_support_bytes_new[] = { 0x8B, 0x4D, 0xF0, 0x0F, 0xBF, 0x41, 0x08, 0x90, 0x90, 0x90, 0x90, 0x90 };

	write_bytes
	(
		0x0040CDF8,
		display_unit_support_bytes_old,
		display_unit_support_bytes_new,
		display_unit_support_bytes_length
	);
	
}

void patch_exact_odds()
{
	int exact_odds_1_bytes_length = 0x3;

	/*
	0:  c1 fa 06                sar    edx,0x6
	*/
	byte exact_odds_1_bytes_old[] = { 0xC1, 0xFA, 0x06 };

	/*
	0:  c1 fa 02                sar    edx,0x2
	...
	*/
	byte exact_odds_1_bytes_new[] = { 0xC1, 0xFA, 0x02 };

	write_bytes
	(
		0x005081CA,
		exact_odds_1_bytes_old,
		exact_odds_1_bytes_new,
		exact_odds_1_bytes_length
	);
	
	write_bytes
	(
		0x005081DD,
		exact_odds_1_bytes_old,
		exact_odds_1_bytes_new,
		exact_odds_1_bytes_length
	);
	
	int exact_odds_2_bytes_length = 0x3;

	/*
	0:  c1 fa 06                sar    edx,0x5
	*/
	byte exact_odds_2_bytes_old[] = { 0xC1, 0xFA, 0x05 };

	/*
	0:  c1 fa 02                sar    edx,0x1
	...
	*/
	byte exact_odds_2_bytes_new[] = { 0xC1, 0xFA, 0x01 };

	write_bytes
	(
		0x00508208,
		exact_odds_2_bytes_old,
		exact_odds_2_bytes_new,
		exact_odds_2_bytes_length
	);
	
	write_bytes
	(
		0x0050821B,
		exact_odds_2_bytes_old,
		exact_odds_2_bytes_new,
		exact_odds_2_bytes_length
	);
	
	int exact_odds_3_bytes_length = 0x3;

	/*
	0:  c1 fa 06                sar    edx,0x5
	*/
	byte exact_odds_3_bytes_old[] = { 0xC1, 0xFA, 0x05 };

	/*
	0:  c1 fa 02                sar    edx,0x1
	...
	*/
	byte exact_odds_3_bytes_new[] = { 0xC1, 0xFA, 0x01 };

	write_bytes
	(
		0x00508246,
		exact_odds_3_bytes_old,
		exact_odds_3_bytes_new,
		exact_odds_3_bytes_length
	);
	
	write_bytes
	(
		0x00508259,
		exact_odds_3_bytes_old,
		exact_odds_3_bytes_new,
		exact_odds_3_bytes_length
	);
	
	int exact_odds_4_bytes_length = 0x3;

	/*
	0:  c1 fa 06                sar    edx,0x4
	*/
	byte exact_odds_4_bytes_old[] = { 0xC1, 0xFA, 0x04 };

	/*
	0:  c1 fa 02                sar    edx,0x0
	...
	*/
	byte exact_odds_4_bytes_new[] = { 0xC1, 0xFA, 0x00 };

	write_bytes
	(
		0x0050827E,
		exact_odds_4_bytes_old,
		exact_odds_4_bytes_new,
		exact_odds_4_bytes_length
	);
	
	write_bytes
	(
		0x00508291,
		exact_odds_4_bytes_old,
		exact_odds_4_bytes_new,
		exact_odds_4_bytes_length
	);
	
}

/*
Vanilla obsoletes unit if there is similar one but with and ability.
With exception of: AMPHIBIOUS | DROP_POD | ANTIGRAV_STRUTS | ARTILLERY | POLICE_2X.
This patch adds more exception to the above list.
ABL_CLOAKED
ABL_AIR_SUPERIORITY
ABL_DEEP_PRESSURE_HULL
ABL_CARRIER
ABL_AAA
ABL_COMM_JAMMER
ABL_EMPATH
ABL_POLY_ENCRYPTION
ABL_BLINK_DISPLACER
ABL_TRANCE
ABL_NERVE_GAS
ABL_DISSOCIATIVE_WAVE
*/
void patch_obsoletion()
{
	int obsolete_ability_exceptions_bytes_length = 0x5;

	/*
	0:  a9 18 84 40 00          test   eax,0x408418
	*/
	byte obsolete_ability_exceptions_bytes_old[] = { 0xA9, 0x18, 0x84, 0x40, 0x00 };

	/*
	0:  a9 fc 9f 56 02          test   eax,0x2569ffc
	...
	*/
	byte obsolete_ability_exceptions_bytes_new[] = { 0xA9, 0xFC, 0x9F, 0x56, 0x02 };

	write_bytes
	(
		0x0057FC8C,
		obsolete_ability_exceptions_bytes_old,
		obsolete_ability_exceptions_bytes_new,
		obsolete_ability_exceptions_bytes_length
	);
	
}

void patch_instant_completion_fixed_minerals(int minerals)
{
	int instant_completion_fixed_minerals_bytes_length = 0xf;

	/*
	0:  8d 04 f6                lea    eax,[esi+esi*8]
	3:  8d 14 46                lea    edx,[esi+eax*2]
	6:  8b 45 ec                mov    eax,DWORD PTR [ebp-0x14]
	9:  8d 34 96                lea    esi,[esi+edx*4]
	c:  c1 e6 02                shl    esi,0x2
	*/
	byte instant_completion_fixed_minerals_bytes_old[] = { 0x8D, 0x04, 0xF6, 0x8D, 0x14, 0x46, 0x8B, 0x45, 0xEC, 0x8D, 0x34, 0x96, 0xC1, 0xE6, 0x02 };

	/*
	0:  89 d6                   mov    esi,edx
	2:  8b 8e 80 d0 97 00       mov    ecx,DWORD PTR [esi+0x97d080]
	8:  83 c1 14                add    ecx,0x14
	b:  8b 45 ec                mov    eax,DWORD PTR [ebp-0x14]
	...
	*/
	byte instant_completion_fixed_minerals_bytes_new[] = { 0x89, 0xD6, 0x8B, 0x8E, 0x80, 0xD0, 0x97, 0x00, 0x83, 0xC1, (byte)minerals, 0x8B, 0x45, 0xEC, 0x90 };

	write_bytes
	(
		0x0057B824,
		instant_completion_fixed_minerals_bytes_old,
		instant_completion_fixed_minerals_bytes_new,
		instant_completion_fixed_minerals_bytes_length
	);
	
}

void patch_turn_upkeep()
{
    write_call_over(0x52768A, (int)modifiedTurnUpkeep);
    write_call_over(0x52A4AD, (int)modifiedTurnUpkeep);
}

void patch_disable_sensor_destroying()
{
	int disable_sensor_destroying_select_bytes_length = 0x1f;

	/*
	0:  83 fe 05                cmp    esi,0x5
	3:  75 08                   jne    0xd
	5:  f6 45 ec 08             test   BYTE PTR [ebp-0x14],0x8
	9:  75 5a                   jne    0x65
	b:  eb 14                   jmp    0x21
	d:  85 f6                   test   esi,esi
	f:  75 0b                   jne    0x1c
	11: f7 45 ec 00 00 08 00    test   DWORD PTR [ebp-0x14],0x80000
	18: 75 4b                   jne    0x65
	1a: eb 05                   jmp    0x21
	1c: 83 fe 0b                cmp    esi,0xb
	*/
	byte disable_sensor_destroying_select_bytes_old[] = { 0x83, 0xFE, 0x05, 0x75, 0x08, 0xF6, 0x45, 0xEC, 0x08, 0x75, 0x5A, 0xEB, 0x14, 0x85, 0xF6, 0x75, 0x0B, 0xF7, 0x45, 0xEC, 0x00, 0x00, 0x08, 0x00, 0x75, 0x4B, 0xEB, 0x05, 0x83, 0xFE, 0x0B };

	/*
	0:  ff 75 ec                push   DWORD PTR [ebp-0x14]
	3:  56                      push   esi
	4:  e8 fd ff ff ff          call   6 <_main+0x6>
	9:  83 c4 08                add    esp,0x8
	c:  85 c0                   test   eax,eax
	...
	*/
	byte disable_sensor_destroying_select_bytes_new[] = { 0xFF, 0x75, 0xEC, 0x56, 0xE8, 0xFD, 0xFF, 0xFF, 0xFF, 0x83, 0xC4, 0x08, 0x85, 0xC0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

	write_bytes
	(
		0x004D57FD,
		disable_sensor_destroying_select_bytes_old,
		disable_sensor_destroying_select_bytes_new,
		disable_sensor_destroying_select_bytes_length
	);
	
    write_call(0x004D57FD + 0x4, (int)isDestroyableImprovement);
    
	int disable_sensor_destroying_exclude_bytes_length = 0x8;

	/*
	0:  8b 45 14                mov    eax,DWORD PTR [ebp+0x14]
	3:  24 df                   and    al,0xdf
	5:  89 45 14                mov    DWORD PTR [ebp+0x14],eax
	*/
	byte disable_sensor_destroying_exclude_bytes_old[] = { 0x8B, 0x45, 0x14, 0x24, 0xDF, 0x89, 0x45, 0x14 };

	/*
	0:  81 65 14 df ff ff 7f    and    DWORD PTR [ebp+0x14],0x7fffffdf
	...
	*/
	byte disable_sensor_destroying_exclude_bytes_new[] = { 0x81, 0x65, 0x14, 0xDF, 0xFF, 0xFF, 0x7F, 0x90 };

	write_bytes
	(
		0x004CAE0A,
		disable_sensor_destroying_exclude_bytes_old,
		disable_sensor_destroying_exclude_bytes_new,
		disable_sensor_destroying_exclude_bytes_length
	);
	
}

void patch_tech_value()
{
	write_call_over(0x005BDC4C, (int)modified_tech_value);
}

void patch_disable_vanilla_base_hurry()
{
	int disable_vanilla_base_hurry_bytes_length = 0x5;

	/*
	0:  e8 a3 c5 ff ff          call   0xffffc5a8
	*/
	byte disable_vanilla_base_hurry_bytes_old[] = { 0xE8, 0xA3, 0xC5, 0xFF, 0xFF };
	
	/*
	...
	*/
	byte disable_vanilla_base_hurry_bytes_new[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };

	write_bytes
	(
		0x004F7A38,
		disable_vanilla_base_hurry_bytes_old,
		disable_vanilla_base_hurry_bytes_new,
		disable_vanilla_base_hurry_bytes_length
	);
	
}

/*
Vehicle moves entry point.
*/
void patch_enemy_units_check()
{
	write_call(0x00528289, (int)modified_enemy_units_check);
	write_call(0x005295C0, (int)modified_enemy_units_check);
	
}

/*
Shows enemies treaty message as popup.
*/
void patch_enemies_treaty_popup()
{
    int popup_bytes_length = 0x5;

    /*
    0:  68 88 13 00 00          push   0x1388
    */
    byte popup_bytes_old[] =
        { 0x68, 0x88, 0x13, 0x00, 0x00 }
    ;

    /*
    0:  6a ff                   push   0xffffffff
    ...
    */
    byte popup_bytes_new[] =
        { 0x6A, 0xFF, 0x90, 0x90, 0x90 }
    ;
    
	// ENEMIESTEAMUP
    write_bytes
    (
        0x0055E355,
        popup_bytes_old,
        popup_bytes_new,
        popup_bytes_length
    )
    ;
    
	// TEAM
    write_bytes
    (
        0x0055D389,
        popup_bytes_old,
        popup_bytes_new,
        popup_bytes_length
    )
    ;
    
    // PACTTRUCE
    write_bytes
    (
        0x0055DACC,
        popup_bytes_old,
        popup_bytes_new,
        popup_bytes_length
    )
    ;
    
    // PACTTREATY
    write_bytes
    (
        0x0055DDD0,
        popup_bytes_old,
        popup_bytes_new,
        popup_bytes_length
    )
    ;
    
    // PACTATTACKS
    write_bytes
    (
        0x0055C9FF,
        popup_bytes_old,
        popup_bytes_new,
        popup_bytes_length
    )
    ;
    
    // PACTUNPACT
    write_bytes
    (
        0x0055CE01,
        popup_bytes_old,
        popup_bytes_new,
        popup_bytes_length
    )
    ;
    
    // ENEMYTREATY
    write_bytes
    (
        0x0055DEF5,
        popup_bytes_old,
        popup_bytes_new,
        popup_bytes_length
    )
    ;
    
    // ENEMYPACTENDS
    write_bytes
    (
        0x0055CFEC,
        popup_bytes_old,
        popup_bytes_new,
        popup_bytes_length
    )
    ;
    
    // ENEMYTRUCE
    write_bytes
    (
        0x0055DBF1,
        popup_bytes_old,
        popup_bytes_new,
        popup_bytes_length
    )
    ;
    
    // ENEMYWAR
    write_bytes
    (
        0x0055CB24,
        popup_bytes_old,
        popup_bytes_new,
        popup_bytes_length
    )
    ;
    
}

/*
Replaces short random with int random because short is too small for tech value.
*/
void patch_tech_ai_randomization()
{
    int tech_ai_randomization_bytes_length = 0x3;

    /*
    0:  c1 e0 08                shl    eax,0x8
    */
    byte tech_ai_randomization_bytes_old[] =
        { 0xC1, 0xE0, 0x08 }
    ;

    /*
    0:  c1 e0 02                shl    eax,0x2
    */
    byte tech_ai_randomization_bytes_new[] =
        { 0xC1, 0xE0, 0x02 }
    ;

    write_bytes
    (
        0x005BDCE7,
        tech_ai_randomization_bytes_old,
        tech_ai_randomization_bytes_new,
        tech_ai_randomization_bytes_length
    )
    ;

}

void patch_pact_withdraw()
{
	write_call(0x0053C69E, (int)modified_pact_withdraw);
	write_call(0x0053C6A8, (int)modified_pact_withdraw);
	
}

void patch_propose_proto()
{
	write_call(0x004CEF64, (int)modified_propose_proto);
	write_call(0x004F59B2, (int)modified_propose_proto);
	write_call(0x0058188D, (int)modified_propose_proto);
	write_call(0x00581C1A, (int)modified_propose_proto);
	write_call(0x00581CA3, (int)modified_propose_proto);
	write_call(0x00581D16, (int)modified_propose_proto);
	write_call(0x0058215E, (int)modified_propose_proto);
	write_call(0x005821C5, (int)modified_propose_proto);
	write_call(0x00582210, (int)modified_propose_proto);
	write_call(0x005825F3, (int)modified_propose_proto);
	write_call(0x00582961, (int)modified_propose_proto);
	write_call(0x00582C22, (int)modified_propose_proto);
	write_call(0x0058306D, (int)modified_propose_proto);
	write_call(0x00583183, (int)modified_propose_proto);
	write_call(0x005832D1, (int)modified_propose_proto);
	write_call(0x00583401, (int)modified_propose_proto);
	write_call(0x00583437, (int)modified_propose_proto);
	write_call(0x00583631, (int)modified_propose_proto);
	write_call(0x005836F9, (int)modified_propose_proto);
	write_call(0x005838F3, (int)modified_propose_proto);
	write_call(0x005839B9, (int)modified_propose_proto);
	write_call(0x005839EC, (int)modified_propose_proto);
	write_call(0x00583A1A, (int)modified_propose_proto);
	write_call(0x00583A64, (int)modified_propose_proto);
	write_call(0x00583B6A, (int)modified_propose_proto);
	write_call(0x00583C80, (int)modified_propose_proto);
	write_call(0x005A4173, (int)modified_propose_proto);
	write_call(0x005A49F5, (int)modified_propose_proto);
	write_call(0x005BB364, (int)modified_propose_proto);
	write_call(0x005BB5CD, (int)modified_propose_proto);
	
}

void patch_science_projects_alternative_labs_bonus()
{
    int science_projects_alternative_labs_bonus_bytes_length = 0x43;

    /*
    0:  8b 15 44 65 9a 00       mov    edx,DWORD PTR ds:0x9a6544
	6:  3b 15 70 93 68 00       cmp    edx,DWORD PTR ds:0x689370
	c:  75 13                   jne    0x21
	e:  a1 30 ea 90 00          mov    eax,ds:0x90ea30
	13: 8b 88 08 01 00 00       mov    ecx,DWORD PTR [eax+0x108]
	19: d1 e1                   shl    ecx,1
	1b: 89 88 08 01 00 00       mov    DWORD PTR [eax+0x108],ecx
	21: 8b 15 70 93 68 00       mov    edx,DWORD PTR ds:0x689370
	27: a1 5c 65 9a 00          mov    eax,ds:0x9a655c
	2c: 3b c2                   cmp    eax,edx
	2e: 75 13                   jne    0x43
	30: a1 30 ea 90 00          mov    eax,ds:0x90ea30
	35: 8b 88 08 01 00 00       mov    ecx,DWORD PTR [eax+0x108]
	3b: d1 e1                   shl    ecx,1
	3d: 89 88 08 01 00 00       mov    DWORD PTR [eax+0x108],ecx
    */
    byte science_projects_alternative_labs_bonus_bytes_old[] =
		{ 0x8B, 0x15, 0x44, 0x65, 0x9A, 0x00, 0x3B, 0x15, 0x70, 0x93, 0x68, 0x00, 0x75, 0x13, 0xA1, 0x30, 0xEA, 0x90, 0x00, 0x8B, 0x88, 0x08, 0x01, 0x00, 0x00, 0xD1, 0xE1, 0x89, 0x88, 0x08, 0x01, 0x00, 0x00, 0x8B, 0x15, 0x70, 0x93, 0x68, 0x00, 0xA1, 0x5C, 0x65, 0x9A, 0x00, 0x3B, 0xC2, 0x75, 0x13, 0xA1, 0x30, 0xEA, 0x90, 0x00, 0x8B, 0x88, 0x08, 0x01, 0x00, 0x00, 0xD1, 0xE1, 0x89, 0x88, 0x08, 0x01, 0x00, 0x00 }
    ;

    /*
    0:  ff b1 08 01 00 00       push   DWORD PTR [ecx+0x108]
	6:  e8 fd ff ff ff          call   8 <_main+0x8>
	b:  83 c4 04                add    esp,0x4
	e:  8b 0d 30 ea 90 00       mov    ecx,DWORD PTR ds:0x90ea30
	14: 89 81 08 01 00 00       mov    DWORD PTR [ecx+0x108],eax
	...
    */
    byte science_projects_alternative_labs_bonus_bytes_new[] =
        { 0xFF, 0xB1, 0x08, 0x01, 0x00, 0x00, 0xE8, 0xFD, 0xFF, 0xFF, 0xFF, 0x83, 0xC4, 0x04, 0x8B, 0x0D, 0x30, 0xEA, 0x90, 0x00, 0x89, 0x81, 0x08, 0x01, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x004EBFC0,
        science_projects_alternative_labs_bonus_bytes_old,
        science_projects_alternative_labs_bonus_bytes_new,
        science_projects_alternative_labs_bonus_bytes_length
    )
    ;
    
   	write_call(0x004EBFC0 + 0x6, (int)modified_base_double_labs);
	
}

void patch_disengagement_from_stack()
{
    int disengagement_from_stack_bytes_length = 0x3;

    /*
    0:  83 f8 01                cmp    eax,0x1
    */
    byte disengagement_from_stack_bytes_old[] =
		{ 0x83, 0xF8, 0x01 }
    ;

    /*
    0:  39 c0                   cmp    eax,eax
	...
    */
    byte disengagement_from_stack_bytes_new[] =
        { 0x39, 0xC0, 0x90 }
    ;

    write_bytes
    (
        0x00508DCA,
        disengagement_from_stack_bytes_old,
        disengagement_from_stack_bytes_new,
        disengagement_from_stack_bytes_length
    )
    ;
    
}

void patch_eco_damage_alternative_industry_effect_reduction_formula()
{
    int eco_damage_alternative_industry_effect_reduction_formula_bytes_length = 0x9;

    /*
	0:  2b c6                   sub    eax,esi
	2:  8b 71 48                mov    esi,DWORD PTR [ecx+0x48]
	5:  99                      cdq
	6:  f7 7d fc                idiv   DWORD PTR [ebp-0x4]
    */
    byte eco_damage_alternative_industry_effect_reduction_formula_bytes_old[] =
		{ 0x2B, 0xC6, 0x8B, 0x71, 0x48, 0x99, 0xF7, 0x7D, 0xFC }
    ;

    /*
	0:  99                      cdq
	1:  f7 7d fc                idiv   DWORD PTR [ebp-0x4]
	4:  29 f0                   sub    eax,esi
	6:  8b 71 48                mov    esi,DWORD PTR [ecx+0x48]
    */
    byte eco_damage_alternative_industry_effect_reduction_formula_bytes_new[] =
        { 0x99, 0xF7, 0x7D, 0xFC, 0x29, 0xF0, 0x8B, 0x71, 0x48 }
    ;

    write_bytes
    (
        0x004E9F7E,
        eco_damage_alternative_industry_effect_reduction_formula_bytes_old,
        eco_damage_alternative_industry_effect_reduction_formula_bytes_new,
        eco_damage_alternative_industry_effect_reduction_formula_bytes_length
    )
    ;
    
}

void patch_unfinished_needlejet_turn()
{
    int unfinished_needlejet_turn_bytes_length = 0x2;

    /*
	0:  3b c8                   cmp    ecx,eax
	*/
    byte unfinished_needlejet_turn_bytes_old[] =
		{ 0x3B, 0xC8 }
    ;

    /*
	0:  39 c9                   cmp    ecx,ecx
    */
    byte unfinished_needlejet_turn_bytes_new[] =
        { 0x39, 0xC9 }
    ;

    write_bytes
    (
        0x005259CE,
        unfinished_needlejet_turn_bytes_old,
        unfinished_needlejet_turn_bytes_new,
        unfinished_needlejet_turn_bytes_length
    )
    ;
    
}

void patch_stockpile_energy_bug()
{
   	write_call(0x004F7A2F, (int)modified_base_production);
   	write_call(0x004F7BBD, (int)modified_base_ecology);
	
}

void patch_limit_orbit_intake()
{
   	write_call(0x004E5227, (int)modified_base_yield);
   	write_call(0x004EC3D1, (int)modified_base_yield);
   	write_call(0x004EF4AA, (int)modified_base_yield);
   	write_call(0x004EF69A, (int)modified_base_yield);
   	write_call(0x004F0FF9, (int)modified_base_yield);
   	write_call(0x004F3976, (int)modified_base_yield);
   	write_call(0x004F5C10, (int)modified_base_yield);
   	write_call(0x004F6222, (int)modified_base_yield);
   	write_call(0x004F62D8, (int)modified_base_yield);
   	write_call(0x004F7A1B, (int)modified_base_yield);
   	write_call(0x004F7A8F, (int)modified_base_yield);
   	write_call(0x004F7BA9, (int)modified_base_yield);
   	
}
	
void patch_ai_transport_change()
{
   	write_call(0x004CB3BA, (int)modified_order_veh);
   	
   	write_call(0x0053179F, (int)modified_order_veh);
}

/*
Game sometimes crashes when last base defender is killed.
*/
void patch_base_attack()
{
    int base_attack_bytes_length = 0x26;
    
    /*
	0:  8d 14 7f                lea    edx,[edi+edi*2]
	3:  8d 04 97                lea    eax,[edi+edx*4]
	6:  0f bf 04 85 32 28 95    movsx  eax,WORD PTR [eax*4+0x952832]
	d:  00
	e:  8d 0c 40                lea    ecx,[eax+eax*2]
	11: 8d 14 88                lea    edx,[eax+ecx*4]
	14: 33 c0                   xor    eax,eax
	16: 8a 04 95 8d b8 9a 00    mov    al,BYTE PTR [edx*4+0x9ab88d]
	1d: c1 e0 04                shl    eax,0x4
	20: 8a 88 68 ae 94 00       mov    cl,BYTE PTR [eax+0x94ae68]
	*/
    byte base_attack_bytes_old[] =
		{ 0x8D, 0x14, 0x7F, 0x8D, 0x04, 0x97, 0x0F, 0xBF, 0x04, 0x85, 0x32, 0x28, 0x95, 0x00, 0x8D, 0x0C, 0x40, 0x8D, 0x14, 0x88, 0x33, 0xC0, 0x8A, 0x04, 0x95, 0x8D, 0xB8, 0x9A, 0x00, 0xC1, 0xE0, 0x04, 0x8A, 0x88, 0x68, 0xAE, 0x94, 0x00 }
    ;
    
    /*
    ...
	24: b1 01                   mov    cl,0x1
	*/
    byte base_attack_bytes_new[] =
        { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xB1, 0x01 }
    ;

    write_bytes
    (
        0x0050AD1A,
        base_attack_bytes_old,
        base_attack_bytes_new,
        base_attack_bytes_length
    )
    ;
    
}

/*
Game sometimes crashes when drawing fireball over vehicle out of range.
*/
void patch_vehicle_boom()
{
    int boom1_bytes_length = 0x20;
    
    /*
	0:  8d 04 7f                lea    eax,[edi+edi*2]
	3:  8d 04 87                lea    eax,[edi+eax*4]
	6:  c1 e0 02                shl    eax,0x2
	9:  0f bf 88 32 28 95 00    movsx  ecx,WORD PTR [eax+0x952832]
	10: 8d 14 49                lea    edx,[ecx+ecx*2]
	13: 8d 0c 91                lea    ecx,[ecx+edx*4]
	16: c1 e1 02                shl    ecx,0x2
	19: 80 b9 8c b8 9a 00 08    cmp    BYTE PTR [ecx+0x9ab88c],0x8
	*/
    byte boom1_bytes_old[] =
		{ 0x8D, 0x04, 0x7F, 0x8D, 0x04, 0x87, 0xC1, 0xE0, 0x02, 0x0F, 0xBF, 0x88, 0x32, 0x28, 0x95, 0x00, 0x8D, 0x14, 0x49, 0x8D, 0x0C, 0x91, 0xC1, 0xE1, 0x02, 0x80, 0xB9, 0x8C, 0xB8, 0x9A, 0x00, 0x08 }
    ;
    
    /*
    0:  83 ff 00                cmp    edi,0x0
	3:  7c 47                   jl     4c <continue>
	5:  90                      nop
	6:  31 c0                   xor    eax,eax
	8:  b0 34                   mov    al,0x34
	a:  f7 e7                   mul    edi
	c:  0f bf 88 32 28 95 00    movsx  ecx,WORD PTR [eax+0x952832]
	13: 31 c0                   xor    eax,eax
	15: b0 34                   mov    al,0x34
	17: f7 e1                   mul    ecx
	19: 80 b9 8c b8 9a 00 08    cmp    BYTE PTR [ecx+0x9ab88c],0x8
	*/
    byte boom1_bytes_new[] =
        { 0x83, 0xFF, 0x00, 0x7C, 0x47, 0x90, 0x31, 0xC0, 0xB0, 0x34, 0xF7, 0xE7, 0x0F, 0xBF, 0x88, 0x32, 0x28, 0x95, 0x00, 0x31, 0xC0, 0xB0, 0x34, 0xF7, 0xE1, 0x80, 0xB9, 0x8C, 0xB8, 0x9A, 0x00, 0x08 }
    ;

    write_bytes
    (
        0x00504C76,
        boom1_bytes_old,
        boom1_bytes_new,
        boom1_bytes_length
    )
    ;
    
    int boom2_bytes_length = 0x1f;
    
    /*
	0:  8b 45 e4                mov    eax,DWORD PTR [ebp-0x1c]
	3:  8d 0c 40                lea    ecx,[eax+eax*2]
	6:  8d 14 88                lea    edx,[eax+ecx*4]
	9:  0f bf 04 95 32 28 95    movsx  eax,WORD PTR [edx*4+0x952832]
	10: 00
	11: 8d 0c 40                lea    ecx,[eax+eax*2]
	14: 8d 14 88                lea    edx,[eax+ecx*4]
	17: 80 3c 95 8c b8 9a 00    cmp    BYTE PTR [edx*4+0x9ab88c],0x8
	1e: 08
	*/
    byte boom2_bytes_old[] =
		{ 0x8B, 0x45, 0xE4, 0x8D, 0x0C, 0x40, 0x8D, 0x14, 0x88, 0x0F, 0xBF, 0x04, 0x95, 0x32, 0x28, 0x95, 0x00, 0x8D, 0x0C, 0x40, 0x8D, 0x14, 0x88, 0x80, 0x3C, 0x95, 0x8C, 0xB8, 0x9A, 0x00, 0x08 }
    ;
    
    /*
    0:  8b 4d e4                mov    ecx,DWORD PTR [ebp-0x1c]
	3:  83 f9 00                cmp    ecx,0x0
	6:  7c 1d                   jl     25 <continue>
	8:  90                      nop
	9:  90                      nop
	a:  90                      nop
	b:  6b c9 34                imul   ecx,ecx,0x34
	e:  0f bf 89 32 28 95 00    movsx  ecx,WORD PTR [ecx+0x952832]
	15: 6b c9 34                imul   ecx,ecx,0x34
	18: 80 b9 8c b8 9a 00 08    cmp    BYTE PTR [ecx+0x9ab88c],0x8
	*/
    byte boom2_bytes_new[] =
        { 0x8B, 0x4D, 0xE4, 0x83, 0xF9, 0x00, 0x7C, 0x1D, 0x90, 0x90, 0x90, 0x6B, 0xC9, 0x34, 0x0F, 0xBF, 0x89, 0x32, 0x28, 0x95, 0x00, 0x6B, 0xC9, 0x34, 0x80, 0xB9, 0x8C, 0xB8, 0x9A, 0x00, 0x08 }
    ;
    
    write_bytes
    (
        0x00505327,
        boom2_bytes_old,
        boom2_bytes_new,
        boom2_bytes_length
    )
    ;
    
}

void patch_disable_vehicle_reassignment()
{
    int disable_vehicle_reassignment_bytes_length = 0x7;
    
    /*
	0:  66 8b 9b 56 28 95 00    mov    bx,WORD PTR [ebx+0x952856]
	*/
    byte disable_vehicle_reassignment_bytes_old[] =
		{ 0x66, 0x8B, 0x9B, 0x56, 0x28, 0x95, 0x00 }
    ;
    
    /*
	0:  66 bb ff ff             mov    bx,0xffff
    ...
	*/
    byte disable_vehicle_reassignment_bytes_new[] =
        { 0x66, 0xBB, 0xFF, 0xFF, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00561FA5,
        disable_vehicle_reassignment_bytes_old,
        disable_vehicle_reassignment_bytes_new,
        disable_vehicle_reassignment_bytes_length
    )
    ;
    
}

void patch_always_support_natives()
{
    int always_support_natives_bytes_length = 0x4;
    
    /*
	0:  f6 40 08 20             test   BYTE PTR [eax+0x8],0x20
	*/
    byte always_support_natives_bytes_old[] =
		{ 0xF6, 0x40, 0x08, 0x20 }
    ;
    
    /*
	0:  f6 40 08 00             test   BYTE PTR [eax+0x8],0x0
	*/
    byte always_support_natives_bytes_new[] =
        { 0xF6, 0x40, 0x08, 0x00 }
    ;

    write_bytes
    (
        0x004E987E,
        always_support_natives_bytes_old,
        always_support_natives_bytes_new,
        always_support_natives_bytes_length
    )
    ;
    
}

/*
Disables transport to pick not boarded vehicles.
*/
void patch_transport_pick_everybody()
{
   	write_call(0x005980AE, (int)modified_stack_veh_disable_transport_pick_everybody);
	
}

/*
Disables non transport vehicles to stop turn in base.
*/
void patch_non_transport_stop_in_base()
{
   	write_call(0x004CB53C, (int)modified_veh_skip_disable_non_transport_stop_in_base);
	
}

void patch_disable_alien_ranged_from_transport()
{
   	write_call(0x0056B65E, (int)modified_alien_move);
   	write_call(0x00566ED9, (int)modified_can_arty_in_alien_move);
	
}

void patch_disable_kill_ai()
{
   	write_call(0x00561C32, (int)modified_kill);
   	write_call(0x00561D3A, (int)modified_kill);
	
}

void patch_no_stagnation()
{
    int no_stagnation_length = 0x31;
    
    /*
	0:  8b 3d 38 e9 90 00       mov    edi,DWORD PTR ds:0x90e938
	6:  8b 35 30 ea 90 00       mov    esi,DWORD PTR ds:0x90ea30
	c:  8b c7                   mov    eax,edi
	e:  c7 45 b4 00 00 00 00    mov    DWORD PTR [ebp-0x4c],0x0
	15: 99                      cdq
	16: f7 3d 3c 97 94 00       idiv   DWORD PTR ds:0x94973c
	1c: 0f be 5e 06             movsx  ebx,BYTE PTR [esi+0x6]
	20: 0f af 1d 3c 97 94 00    imul   ebx,DWORD PTR ds:0x94973c
	27: 8b 8e c0 00 00 00       mov    ecx,DWORD PTR [esi+0xc0]
	2d: 03 c1                   add    eax,ecx
	2f: 3b c3                   cmp    eax,ebx
	*/
    byte no_stagnation_old[] =
		{ 0x8B, 0x3D, 0x38, 0xE9, 0x90, 0x00, 0x8B, 0x35, 0x30, 0xEA, 0x90, 0x00, 0x8B, 0xC7, 0xC7, 0x45, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x99, 0xF7, 0x3D, 0x3C, 0x97, 0x94, 0x00, 0x0F, 0xBE, 0x5E, 0x06, 0x0F, 0xAF, 0x1D, 0x3C, 0x97, 0x94, 0x00, 0x8B, 0x8E, 0xC0, 0x00, 0x00, 0x00, 0x03, 0xC1, 0x3B, 0xC3 }
    ;
	
    /*
	0:  c7 45 b4 00 00 00 00    mov    DWORD PTR [ebp-0x4c],0x0
	7:  8b 3d 38 e9 90 00       mov    edi,DWORD PTR ds:0x90e938
	d:  8b 35 30 ea 90 00       mov    esi,DWORD PTR ds:0x90ea30
	13: 0f be 5e 06             movsx  ebx,BYTE PTR [esi+0x6]
	17: 0f af 1d 3c 97 94 00    imul   ebx,DWORD PTR ds:0x94973c
	1e: 0f be 46 06             movsx  eax,BYTE PTR [esi+0x6]
	22: 83 c0 02                add    eax,0x2
	25: d1 e8                   shr    eax,1
	27: 01 d8                   add    eax,ebx
	29: 8b 8e c0 00 00 00       mov    ecx,DWORD PTR [esi+0xc0]
	2f: 39 c1                   cmp    ecx,eax
	*/
    byte no_stagnation_new[] =
        { 0xC7, 0x45, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x3D, 0x38, 0xE9, 0x90, 0x00, 0x8B, 0x35, 0x30, 0xEA, 0x90, 0x00, 0x0F, 0xBE, 0x5E, 0x06, 0x0F, 0xAF, 0x1D, 0x3C, 0x97, 0x94, 0x00, 0x0F, 0xBE, 0x46, 0x06, 0x83, 0xC0, 0x02, 0xD1, 0xE8, 0x01, 0xD8, 0x8B, 0x8E, 0xC0, 0x00, 0x00, 0x00, 0x39, 0xC1 }
    ;
	
    write_bytes
    (
        0x004E88E1,
        no_stagnation_old,
        no_stagnation_new,
        no_stagnation_length
    )
    ;
	
}

/**
Fixes vanilla bug when faction bonus is applied to other base.
*/
void patch_capture_base()
{
    int capture_base_length = 0x6;
    
    /*
	0:  8b 15 70 93 68 00       mov    edx,DWORD PTR ds:0x689370
	*/
    byte capture_base_old[] =
		{ 0x8B, 0x15, 0x70, 0x93, 0x68, 0x00 }
    ;
	
    /*
    0:  8b 55 08                mov    edx,DWORD PTR [ebp+0x8]
	...
	*/
    byte capture_base_new[] =
        { 0x8B, 0x55, 0x08, 0x90, 0x90, 0x90 }
    ;
	
    write_bytes
    (
        0x0050D34A,
        capture_base_old,
        capture_base_new,
        capture_base_length
    )
    ;
	
}

void patch_accelerate_order_veh()
{
    int disable_order_veh_draw_length = 0x5;

    /*
	0:  3b c8                   cmp    ecx,eax
	*/
    byte disable_order_veh_draw_old[] =
		{ 0xE8, 0xD6, 0x1F, 0xFC, 0xFF }
    ;

    /*
	0:  39 c9                   cmp    ecx,ecx
    */
    byte disable_order_veh_draw_new[] =
        { 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;

    write_bytes
    (
        0x00598175,
        disable_order_veh_draw_old,
        disable_order_veh_draw_new,
        disable_order_veh_draw_length
    )
    ;
    
}

// =======================================================
// main patch option selection
// =======================================================

void patch_setup_wtp(Config* cf)
{
	// debug mode game speedup
	
	if (DEBUG)
	{
//		patch_disable_boom_delay();
//		patch_disable_battle_refresh();
//		patch_accelerate_order_veh();
//		patch_disable_boom();
//		patch_disable_battle_calls();
//		patch_disable_focus();
	}

	// integrated into Thinker
//	// patch AI vehicle home base reassignment bug
//	// originally it reassigned to bases with mineral_surplus < 2
//	// this patch reverses the condition
//	{
//		const byte old_bytes[] = {0x0F, 0x8D};
//		const byte new_bytes[] = {0x0F, 0x8C};
//		write_bytes(0x00562094, old_bytes, new_bytes, sizeof(old_bytes));
//	}

	// read basic rules

	patch_read_basic_rules();

	// patch battle_compute

	patch_battle_compute();

	// integrated into Thinker
	// but it is not 1:1 match
	// so I disabled Thinker's setting and keep using mine instead
	// patch ignore_reactor_power_in_combat_processing

	if (cf->ignore_reactor_power_in_combat)
	{
		patch_ignore_reactor_power_in_combat_processing();
	}

	// patch combat roll

	patch_combat_roll();
	
	// patch odds display
	
	patch_display_odds();
	
	// integrated into Thinker
//	// patch ALIEN guaranteed technologies
//
//	if (cf->disable_alien_guaranteed_technologies)
//	{
//		patch_alien_guaranteed_technologies();
//
//	}

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

	// patch flat hurry cost

	if (cf->flat_hurry_cost)
	{
		patch_flat_hurry_cost();

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

	// integrated into Thinker
//	// patch collateral damage value
//
//	if (cf->collateral_damage_value >= 0)
//	{
//		patch_collateral_damage_value(cf->collateral_damage_value);
//
//	}

	// integrated into Thinker
//	// patch disable_aquatic_bonus_minerals
//
//	if (cf->disable_aquatic_bonus_minerals)
//	{
//		patch_disable_aquatic_bonus_minerals();
//
//	}

	// integrated into Thinker
//	// patch repair rate
//
//	if (cf->repair_minimal != 1)
//	{
//		patch_repair_minimal(cf->repair_minimal);
//
//	}
//	if (cf->repair_fungus != 2)
//	{
//		patch_repair_fungus(cf->repair_fungus);
//
//	}
//	if (!cf->repair_friendly)
//	{
//		patch_repair_friendly();
//
//	}
//	if (!cf->repair_airbase)
//	{
//		patch_repair_airbase();
//
//	}
//	if (!cf->repair_bunker)
//	{
//		patch_repair_bunker();
//
//	}
//	if (cf->repair_base != 1)
//	{
//		patch_repair_base(cf->repair_base);
//
//	}
//	if (cf->repair_base_native != 10)
//	{
//		patch_repair_base_native(cf->repair_base_native);
//
//	}
//	if (cf->repair_base_facility != 10)
//	{
//		patch_repair_base_facility(cf->repair_base_facility);
//
//	}
//	if (cf->repair_nano_factory != 10)
//	{
//		patch_repair_nano_factory(cf->repair_nano_factory);
//
//	}

	// integrated into Thinker
//	// patch disable_planetpearls
//
//	if (cf->disable_planetpearls)
//	{
//		patch_disable_planetpearls();
//
//	}

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

	// integrated into Thinker
//	// patch coastal_territory_distance_same_as_sea
//
//	if (cf->coastal_territory_distance_same_as_sea)
//	{
//		patch_coastal_territory_distance_same_as_sea();
//
//	}

	// patch alternative_artillery_damage

	if (cf->alternative_artillery_damage)
	{
		patch_alternative_artillery_damage();

	}

	// patch disable_home_base_cc_morale_bonus

	if (cf->disable_home_base_cc_morale_bonus)
	{
		patch_disable_home_base_cc_morale_bonus();

	}

	// patch disable_current_base_cc_morale_bonus

	if (cf->disable_current_base_cc_morale_bonus)
	{
		patch_disable_current_base_cc_morale_bonus();

	}

	// patch default_morale_very_green

	if (cf->default_morale_very_green)
	{
		patch_default_morale_very_green();

	}

	// patch tile_yield

	patch_tile_yield();

	// se_accumulated_resource_adjustment

	patch_se_accumulated_resource_adjustment();

	// hex cost

	patch_hex_cost();

	// base population info

	patch_display_base_population_info();

	// base init

	patch_base_init();

	// HSA does not kill probe

	if (cf->hsa_does_not_kill_probe)
	{
		patch_hsa_does_not_kill_probe();
	}

	// probe does not destroy defense

	patch_probe_not_destroy_defense();

	// help ability

	patch_help_ability_cost_text();

	// cloning vats impunities

	if (cf->cloning_vats_disable_impunities)
	{
		patch_cloning_vats_impunities();
	}

	// social_calc
	// This is initially for cloning vats GROWTH effect but can be used for other modifications

	patch_social_calc();

	// cloning vats effect

	if (cf->cloning_vats_se_growth != 0)
	{
		patch_cloning_vats_mechanics();
	}

	// patch GROWTH rating max

	if (cf->se_growth_rating_max != 5)
	{
		patch_se_growth_rating_max(cf->se_growth_rating_max);
	}

	// patch GROWTH rating min

	if (cf->se_growth_rating_min != -2)
	{
		patch_se_growth_rating_min(cf->se_growth_rating_min);
	}

	patch_display_base_nutrient_cost_factor();

	patch_growth_turns_population_boom();

	// patch RT -> 50% minerals

	if (cf->recycling_tanks_mineral_multiplier)
	{
		patch_recycling_tank_minerals();
	}

	// integrated into Thinker
//	// patch free minerals
//
//	if (cf->free_minerals != 16)
//	{
//		patch_free_minerals(cf->free_minerals);
//	}

	// patch population incorrect superdrones in base screen population

	patch_base_scren_population_superdrones();

	// integrated into Thinker
//	// patch conflicting territory claim
//
//	patch_conflicting_territory();

	// patch native life

	if (cf->native_life_generator_constant != 2 || cf->native_life_generator_multiplier != 2)
	{
		patch_native_life_frequency(cf->native_life_generator_constant, cf->native_life_generator_multiplier);
	}

	if (cf->native_life_generator_more_sea_creatures)
	{
		patch_native_life_sea_creatures();
	}

	if (cf->native_disable_sudden_death)
	{
		patch_native_disable_sudden_death();
	}

	if (cf->alternative_inefficiency)
	{
		patch_alternative_inefficiency();
	}

	// integrated in Thinker
//	patch_setup_player();
	patch_veh_init_in_balance();

	if (cf->recycling_tanks_mineral_multiplier)
	{
		patch_recycling_tanks_not_disabled_by_pressure_dome();
	}

	patch_world_build();

//	if (cf->zoc_regular_army_sneaking_disabled)
//	{
//		patch_zoc_regular_army_sneaking_disabled();
//	}
//
	if (cf->biology_lab_research_bonus != 2)
	{
		patch_biology_lab_research_bonus(cf->biology_lab_research_bonus);
	}

	patch_weapon_help_always_show_cost();

	if (cf->fix_former_wake)
	{
		patch_veh_wake();
	}

	if (cf->flat_extra_prototype_cost)
	{
		patch_flat_extra_prototype_cost();
	}

	if (cf->fix_mineral_contribution)
	{
		patch_mineral_contribution();
	}

	patch_pact_base_map_production_display();

	if (cf->modified_probe_action_risks)
	{
		patch_modified_probe_action_risks();
	}

	if (cf->infiltration_expire)
	{
		patch_infiltrate_datalinks();
	}

	patch_min_knowledge_price();

	patch_find_returned_probe_base();

	patch_best_defender();

	// integrated into Thinker
//	patch_action_destroy();

	if (cf->break_treaty_before_fight)
	{
		patch_break_treaty_before_fight();
	}

	patch_talent_display();

	if (cf->compact_effect_icons)
	{
		patch_compact_effect_icons();
	}

	if (cf->se_research_bonus_percentage != 10)
	{
		patch_research_bonus_multiplier();
	}

	if (cf->remove_fungal_tower_defense_bonus)
	{
		patch_remove_fungal_tower_defense_bonus();
	}

	if (cf->aliens_fight_half_strength_unit_turn != 15)
	{
		patch_aliens_fight_half_strength_unit_turn(cf->aliens_fight_half_strength_unit_turn);
	}

	if (cf->habitation_facility_disable_explicit_population_limit)
	{
		patch_habitation_facility_disable_explicit_population_limit();
	}

	if (cf->habitation_facility_present_growth_bonus_max != 0.0 || cf->habitation_facility_absent_growth_penalty != 0.0)
	{
		patch_modified_base_growth();
	}

	if (cf->unit_upgrade_ignores_movement)
	{
		patch_unit_upgrade_ignores_movement();
	}

	if (cf->group_terraforming)
	{
		patch_group_terraforming();
	}

	patch_disable_land_artillery_bombard_from_sea();

	patch_disable_air_transport_unload_everywhere();
	
//	patch_base_production();
//	
	patch_enemy_move();

	patch_faction_upkeep();
	
	patch_disable_move_territory_restrictions();
	
	if (cf->alternative_tech_cost)
	{
		patch_tech_cost();
	}
	
	if (cf->interceptor_scramble_fix)
	{
		patch_interceptor_scramble_fix_display();
	}
	
	if (cf->scorched_earth)
	{
		patch_scorched_earth();
	}
	
	if (cf->orbital_yield_limit != 1.0)
	{
		patch_orbital_yield_limit();
	}
	
	if (cf->silent_vendetta_warning)
	{
		patch_silent_vendetta_warning();
	}
	
	if (cf->design_cost_in_rows)
	{
		patch_design_cost_in_rows();
	}
	
	if (cf->energy_market_crash_numerator != 4)
	{
		patch_energy_market_crash(cf->energy_market_crash_numerator);
	}
	
	if (cf->carry_over_minerals)
	{
		patch_carry_over_minerals();
	}
	
	if (cf->subversion_allow_stacked_units)
	{
		patch_subversion_allow_stacked_units();
	}
	
	if (cf->mind_control_destroys_unsubverted)
	{
		patch_mind_control_destroys_unsubverted();
	}
	
	if (cf->alternative_subversion_and_mind_control)
	{
		patch_alternative_subversion_and_mind_control();
	}
	
	if (cf->disable_guaranteed_facilities_destruction)
	{
		patch_disable_guaranteed_facilities_destruction();
	}
	
	if (cf->total_thought_control_no_diplomatic_consequences)
	{
		patch_total_thought_control_no_diplomatic_consequences();
	}
	
	if (cf->supply_convoy_and_info_warfare_require_support)
	{
		patch_supply_convoy_and_info_warfare_require_support();
	}
	
	if (cf->alternative_support)
	{
		patch_alternative_support();
	}
	
	patch_exact_odds();
	
	patch_obsoletion();
	
	if (cf->instant_completion_fixed_minerals > 0)
	{
		patch_instant_completion_fixed_minerals(cf->instant_completion_fixed_minerals);
	}
	
	patch_turn_upkeep();
	
	if (cf->disable_sensor_destroying)
	{
		patch_disable_sensor_destroying();
	}
	
	patch_tech_value();
	
	if (cf->disable_vanilla_base_hurry)
	{
		patch_disable_vanilla_base_hurry();
	}
	
	patch_enemy_units_check();
	
	patch_enemies_treaty_popup();
	
	patch_tech_ai_randomization();
	
	patch_pact_withdraw();
	
	patch_propose_proto();
	
	if (cf->science_projects_alternative_labs_bonus)
	{
		patch_science_projects_alternative_labs_bonus();
	}
	
	if (cf->disengagement_from_stack)
	{
		patch_disengagement_from_stack();
	}
	
	if (cf->eco_damage_alternative_industry_effect_reduction_formula)
	{
		patch_eco_damage_alternative_industry_effect_reduction_formula();
	}
	
	patch_unfinished_needlejet_turn();
	
	patch_stockpile_energy_bug();
	
	patch_limit_orbit_intake();
	
	patch_ai_transport_change();
	
	patch_base_attack();
	
	patch_vehicle_boom();
	
	patch_disable_vehicle_reassignment();
	
	patch_always_support_natives();
	
	patch_transport_pick_everybody();
	
	patch_non_transport_stop_in_base();
	
	patch_disable_alien_ranged_from_transport();
	
	patch_disable_kill_ai();
	
	patch_no_stagnation();
	
	patch_capture_base();
	
}

