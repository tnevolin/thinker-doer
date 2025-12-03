
#include "wtp_patch.h"
#include "wtp_game.h"
#include "wtp_mod.h"
#include "wtp_ai.h"
#include "wtp_aiProduction.h"
#include "wtp_aiMove.h"

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
	
    write_call_over(0x0050474C, (int)wtp_mod_battle_compute);
    write_call_over(0x00506EA6, (int)wtp_mod_battle_compute);
    write_call_over(0x005085E0, (int)wtp_mod_battle_compute);
	
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
    write_call_over(0x00436ADD, (int)wtp_mod_proto_cost);
	write_call_over(0x0043704C, (int)wtp_mod_proto_cost);
	write_call_over(0x005817C9, (int)wtp_mod_proto_cost);
    write_call_over(0x00581833, (int)wtp_mod_proto_cost);
    write_call_over(0x00581BB3, (int)wtp_mod_proto_cost);
    write_call_over(0x00581BCB, (int)wtp_mod_proto_cost);
    write_call_over(0x00582339, (int)wtp_mod_proto_cost);
    write_call_over(0x00582359, (int)wtp_mod_proto_cost);
    write_call_over(0x00582378, (int)wtp_mod_proto_cost);
    write_call_over(0x00582398, (int)wtp_mod_proto_cost);
    write_call_over(0x005823B0, (int)wtp_mod_proto_cost);
    write_call_over(0x00582482, (int)wtp_mod_proto_cost);
    write_call_over(0x0058249A, (int)wtp_mod_proto_cost);
    write_call_over(0x0058254A, (int)wtp_mod_proto_cost);
    write_call_over(0x005827E4, (int)wtp_mod_proto_cost);
    write_call_over(0x00582EC5, (int)wtp_mod_proto_cost);
    write_call_over(0x00582FEC, (int)wtp_mod_proto_cost);
    write_call_over(0x005A5D35, (int)wtp_mod_proto_cost);
    write_call_over(0x005A5F15, (int)wtp_mod_proto_cost);
	
}

void patch_hurry_popup()
{
    write_call(0x41916B, (int)wtp_BaseWin_popup_start);
    write_call(0x4195A6, (int)wtp_BaseWin_ask_number);
	
}

/*
Hurries minimal mineral amount to complete production on next turn.
*/
void patch_hurry_minimal_minerals()
{
	// BaseWin__hurry
	
	// push item cost instead of first parameter
	
	int hurry_minimal_minerals1_bytes_length = 0x1;
	
/*
0:  51                      push   ecx
*/
	byte hurry_minimal_minerals1_bytes_old[] =
		{ 0x51 }
	;
	
/*
0:  56                      push   esi
*/
	byte hurry_minimal_minerals1_bytes_new[] =
		{ 0x56 }
	;
	
	write_bytes
	(
		0x00418F9C,
		hurry_minimal_minerals1_bytes_old,
		hurry_minimal_minerals1_bytes_new,
		hurry_minimal_minerals1_bytes_length
	)
	;
	
	// intercept call
	
	write_call(0x00418F9D, (int)wpt_mod_Base_hurry_cost_factor_mineral_cost);
	
	// do not mulitpy by item cost - it is already done in the call above
	
	int hurry_minimal_minerals2_bytes_length = 0x3;
	
/*
0:  0f af ce                imul   ecx,esi
*/
	byte hurry_minimal_minerals2_bytes_old[] =
		{ 0x0F, 0xAF, 0xCE }
	;
	
/*
...
*/
	byte hurry_minimal_minerals2_bytes_new[] =
		{ 0x90, 0x90, 0x90 }
	;
	
	write_bytes
	(
		0x00418FA9,
		hurry_minimal_minerals2_bytes_old,
		hurry_minimal_minerals2_bytes_new,
		hurry_minimal_minerals2_bytes_length
	)
	;
	
}

/*
Applies flat hurry cost mechanics.
*/
void patch_flat_hurry_cost()
{
	// BaseWin__hurry
	
    int flat_hurry_cost_bytes_length = 0x2f;
	
/*
0:  8d 04 9b                lea    eax,[ebx+ebx*4]
3:  8d 14 c0                lea    edx,[eax+eax*8]
6:  c1 e2 03                shl    edx,0x3
9:  2b d3                   sub    edx,ebx
b:  8b 04 95 44 6f 94 00    mov    eax,DWORD PTR [edx*4+0x946f44]
12: 83 f8 64                cmp    eax,0x64
15: 74 18                   je     0x2f
17: 0f af c7                imul   eax,edi
1a: 8b c8                   mov    ecx,eax
1c: b8 1f 85 eb 51          mov    eax,0x51eb851f
21: f7 e9                   imul   ecx
23: c1 fa 05                sar    edx,0x5
26: 8b c2                   mov    eax,edx
28: c1 e8 1f                shr    eax,0x1f
2b: 03 d0                   add    edx,eax
2d: 8b fa                   mov    edi,edx
*/
    byte flat_hurry_cost_bytes_old[] =
        { 0x8D, 0x04, 0x9B, 0x8D, 0x14, 0xC0, 0xC1, 0xE2, 0x03, 0x2B, 0xD3, 0x8B, 0x04, 0x95, 0x44, 0x6F, 0x94, 0x00, 0x83, 0xF8, 0x64, 0x74, 0x18, 0x0F, 0xAF, 0xC7, 0x8B, 0xC8, 0xB8, 0x1F, 0x85, 0xEB, 0x51, 0xF7, 0xE9, 0xC1, 0xFA, 0x05, 0x8B, 0xC2, 0xC1, 0xE8, 0x1F, 0x03, 0xD0, 0x8B, 0xFA }
    ;
	
/*
0:  a1 30 ea 90 00          mov    eax,ds:0x90ea30
5:  8b 55 dc                mov    edx,DWORD PTR [ebp-0x24]
8:  2b 50 40                sub    edx,DWORD PTR [eax+0x40]
b:  52                      push   edx
c:  ff 70 50                push   DWORD PTR [eax+0x50]
f:  ff 35 70 93 68 00       push   DWORD PTR ds:0x689370
15: e8 fd ff ff ff          call   17 <_main+0x17>
1a: 83 c4 0c                add    esp,0xc
1d: 89 c7                   mov    edi,eax
...
*/
    byte flat_hurry_cost_bytes_new[] =
        { 0xA1, 0x30, 0xEA, 0x90, 0x00, 0x8B, 0x55, 0xDC, 0x2B, 0x50, 0x40, 0x52, 0xFF, 0x70, 0x50, 0xFF, 0x35, 0x70, 0x93, 0x68, 0x00, 0xE8, 0xFD, 0xFF, 0xFF, 0xFF, 0x83, 0xC4, 0x0C, 0x89, 0xC7, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }
    ;
	
    write_bytes
    (
        0x0041901C,
        flat_hurry_cost_bytes_old,
        flat_hurry_cost_bytes_new,
        flat_hurry_cost_bytes_length
    )
    ;
	
    write_call(0x0041901C + 0x15, (int)hurry_cost);
	
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
	
    if (conf.ignore_reactor_power != 0 || conf.ignore_reactor_power)
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
	
    write_call(0x00508616 + 0x5, (int)modified_artillery_damage);
	
}

/*
Disables current base Children's Creche morale bonus.
Disables current base Brood Pit morale bonus.
*/
void patch_disable_current_base_morale_effect()
{
	// in say_morale2
	
    // disable_current_base_morale_effect display
    // Children Creche
	
    int disable_current_base_morale_effect_display_bytes_length = 0x2;
    byte disable_current_base_morale_effect_display_bytes_old[] = { 0xF7, 0xDA };
    byte disable_current_base_morale_effect_display_bytes_new[] = { 0x31, 0xD2 };
	
    write_bytes
    (
        0x004B4144,
        disable_current_base_morale_effect_display_bytes_old,
        disable_current_base_morale_effect_display_bytes_new,
        disable_current_base_morale_effect_display_bytes_length
    )
    ;
	
    // disable_current_base_bp_morale_bonus display
    // Brood Pit
	
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
	
}

/*
Sets default unit morale to Very Green.
*/
void patch_default_morale_very_green()
{
	// in base_production
	
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
    write_call(0x004C9870, (int)wtp_mod_base_init);
    write_call(0x005AF926, (int)wtp_mod_base_init);
    write_call(0x005B2BB8, (int)wtp_mod_base_init);

}

/*
HSA does not kill probe.
*/
void patch_hsa_does_not_kill_probe()
{
	// HSA does not kill probe but exhausts its movement points instead.

    write_call(0x0059F6EA, (int)mod_veh_skip);

}

/*
Exclude defensive faciliites from probe target lists.
*/
void patch_probe_not_destroy_defense()
{
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
	write_call(0x004AF0F6, (int)modifiedSocialCalc);
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
Modifies SE GROWTH rating max.
*/
void patch_se_growth_rating_max(int se_growth_rating_max)
{
	// patch population boom condition in base_doctors
	
	int popboom_condition_base_doctors_bytes_length = 7;
	byte popboom_condition_base_doctors_bytes_old[] = { 0x83, 0x3D, 0x18, 0xE9, 0x90, 0x00, 0x06 };
	byte popboom_condition_base_doctors_bytes_new[] = { 0x83, 0x3D, 0x18, 0xE9, 0x90, 0x00, (byte)(se_growth_rating_max + 1) };
	write_bytes
	(
		0x004F6070,
		popboom_condition_base_doctors_bytes_old,
		popboom_condition_base_doctors_bytes_new,
		popboom_condition_base_doctors_bytes_length
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
void patch_modified_probe_risks()
{
	int modified_probe_risk_hook_bytes_length = 0x13;
/*
0:  8b 55 08                mov    edx,DWORD PTR [ebp+0x8]
3:  6a 00                   push   0x0
5:  6a 01                   push   0x1
7:  52                      push   edx
8:  e8 f2 dc 01 00          call   0x1dcff
d:  83 c4 0c                add    esp,0xc
10: 89 45 b4                mov    DWORD PTR [ebp-0x4c],eax
*/
	byte modified_probe_risk_hook_bytes_old[] = { 0x8B, 0x55, 0x08, 0x6A, 0x00, 0x6A, 0x01, 0x52, 0xE8, 0xF2, 0xDC, 0x01, 0x00, 0x83, 0xC4, 0x0C, 0x89, 0x45, 0xB4 };
/*
0:  89 45 b4                mov    DWORD PTR [ebp-0x4c],eax
3:  8d 45 d0                lea    eax,[ebp-0x30]
6:  50                      push   eax
7:  ff 75 e8                push   DWORD PTR [ebp-0x18]
a:  e8 fd ff ff ff          call   c <_main+0xc>
f:  83 c4 08                add    esp,0x8
...
*/
	byte modified_probe_risk_hook_bytes_new[] = { 0x89, 0x45, 0xB4, 0x8D, 0x45, 0xD0, 0x50, 0xFF, 0x75, 0xE8, 0xE8, 0xFD, 0xFF, 0xFF, 0xFF, 0x83, 0xC4, 0x08, 0x90 };
	write_bytes
	(
		0x005A3141,
		modified_probe_risk_hook_bytes_old,
		modified_probe_risk_hook_bytes_new,
		modified_probe_risk_hook_bytes_length
	);
	
	write_call(0x005A314B, (int)modifiedProbeActionRisk);
	
	// procure research data success_rates
	
	write_call(0x0059FEBB, (int)wtp_mod_probe_success_rates_procure_research_data);
	
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
Wraps best_defender call.
*/
void patch_best_defender()
{
	write_call(0x00506D07, (int)modifiedBestDefender);
	
}

/*
Requests for break treaty before fight.
*/
void patch_break_treaty_before_fight()
{
	write_call(0x00506ADE, (int)wtp_mod_battle_fight_2);
	write_call(0x00568B1C, (int)wtp_mod_battle_fight_2);
	write_call(0x005697AC, (int)wtp_mod_battle_fight_2);
	write_call(0x0056A2E2, (int)wtp_mod_battle_fight_2);
	
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
Disables air transport unload everywhere.
*/
void patch_disable_air_transport_unload_everywhere()
{
    write_call(0x004D0509, (int)modifiedVehicleCargoForAirTransportUnload);
}

void patch_enemy_move()
{
//	// in Console::give_hints
//	write_call(0x00512842, (int)wtp_mod_enemy_move);
	// in enemy_move_check
	write_call(0x00579362, (int)wtp_mod_enemy_move);
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
	int disable_move_territory_restriction_bytes_length = 0x1;
	
	/*
	0:  80 be 39 28 95 00 18    cmp    BYTE PTR [esi+0x952839],0x18
	*/
	byte disable_move_territory_restriction_bytes_old[] = { 0x18 };
	
	/*
	0:  80 be 39 28 95 00 ff    cmp    BYTE PTR [esi+0x952839],0xff
	*/
	byte disable_move_territory_restriction_bytes_new[] = { 0xFF };
	
	write_bytes
	(
		0x00594A0B + 0x6,
		disable_move_territory_restriction_bytes_old,
		disable_move_territory_restriction_bytes_new,
		disable_move_territory_restriction_bytes_length
	);
	
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

void patch_carry_over_nutrients()
{
	// in BaseWin::draw_nutrients
	// do not add 1 in grow turn computation due to same turn base growth
	
	int carry_over_nutrients_grow_turns_bytes_length = 0x1;
	
	/*
	0:  46                      inc    esi
	*/
	byte carry_over_nutrients_grow_turns_bytes_old[] = { 0x46 };
	
	/*
	...
	*/
	byte carry_over_nutrients_grow_turns_bytes_new[] = { 0x90 };
	
	write_bytes
	(
		0x0041176B,
		carry_over_nutrients_grow_turns_bytes_old,
		carry_over_nutrients_grow_turns_bytes_new,
		carry_over_nutrients_grow_turns_bytes_length
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
	
    write_call(0x0056E19C, (int)wtp_mod_probe);
    write_call(0x00578512, (int)wtp_mod_probe);
    write_call(0x00595026, (int)wtp_mod_probe);
    write_call(0x00596615, (int)wtp_mod_probe);
	
    write_call(0x005A4250, (int)modifiedSubveredVehicleDrawTile);
    
    // intercept enemy_move is done in patch_enemy_move
    
//    write_call(0x00512842, (int)wtp_mod_enemy_move);
//    write_call(0x00579362, (int)wtp_mod_enemy_move);
    
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

void patch_capture_base_does_not_destroy_facilities()
{
	// in capture_base
	// disable other facilities destruction
	
	int capture_base_does_not_destroy_facilities1_bytes_length = 0x2;
	byte capture_base_does_not_destroy_facilities1_bytes_old[] = { 0x85, 0xC0 }; // 0:  85 c0                   test   eax,eax
	byte capture_base_does_not_destroy_facilities1_bytes_new[] = { 0x85, 0xFF }; // 0:  85 ff                   test   edi,edi
	write_bytes(0x0050D01B, capture_base_does_not_destroy_facilities1_bytes_old, capture_base_does_not_destroy_facilities1_bytes_new, capture_base_does_not_destroy_facilities1_bytes_length);
	
	// in capture_base
	// disable guaranteed facilities destruction
	
	int capture_base_does_not_destroy_facilities2_bytes_length = 0x2;
	byte capture_base_does_not_destroy_facilities2_bytes_old[] = { 0x85, 0xC0 }; // 0:  85 c0                   test   eax,eax
	byte capture_base_does_not_destroy_facilities2_bytes_new[] = { 0x85, 0xFF }; // 0:  85 ff                   test   edi,edi
	write_bytes(0x0050D061, capture_base_does_not_destroy_facilities2_bytes_old, capture_base_does_not_destroy_facilities2_bytes_new, capture_base_does_not_destroy_facilities2_bytes_length);
	
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

void patch_turn_upkeep()
{
    write_call_over(0x52768A, (int)modifiedTurnUpkeep);
    write_call_over(0x52A4AD, (int)modifiedTurnUpkeep);
}

void patch_sensor_indestructible()
{
	// Console::destroy
	// disable sensor destruction
	
    write_call(0x0046D4D6, (int)wtp_mod_Console_destroy);
    write_call(0x00517589, (int)wtp_mod_Console_destroy);
    write_call(0x0051C8EF, (int)wtp_mod_Console_destroy);
	
	// action_destroy
	// disable sensor destruction
	
    write_call(0x004CD181, (int)wtp_mod_action_destroy);
    write_call(0x004D5B71, (int)wtp_mod_action_destroy);
    write_call(0x00536A7C, (int)wtp_mod_action_destroy);
    write_call(0x00536AB6, (int)wtp_mod_action_destroy);
    write_call(0x005671B4, (int)wtp_mod_action_destroy);
    write_call(0x0056778F, (int)wtp_mod_action_destroy);
    write_call(0x00570DDF, (int)wtp_mod_action_destroy);
    write_call(0x00570EF0, (int)wtp_mod_action_destroy);
    write_call(0x0057108E, (int)wtp_mod_action_destroy);
    write_call(0x00578D94, (int)wtp_mod_action_destroy);
	
}

void patch_tech_value()
{
	write_call_over(0x005BDC4C, (int)modified_tech_value);
}

void patch_disable_vanilla_base_hurry()
{
	write_call(0x4F7A38, (int)wtp_mod_base_hurry);
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

void patch_order_veh()
{
   	write_call(0x004CB3BA, (int)wtp_mod_order_veh);
   	write_call(0x004CBA24, (int)wtp_mod_order_veh);
   	write_call(0x0053179F, (int)wtp_mod_order_veh);
   	write_call(0x00531AC5, (int)wtp_mod_order_veh);
   	write_call(0x005367EC, (int)wtp_mod_order_veh);
	
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
    shiftVehicleAddress(base_attack_bytes_old, 0xA);
	
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
	shiftVehicleAddress(boom1_bytes_old, 0xC);
	
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
	shiftVehicleAddress(boom2_bytes_old, 0xD);
	
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

void patch_tech_trade_likeability()
{
    int tech_trade_likeability_length = 0x7;
	
    /*
	*/
    byte tech_trade_likeability_old[] =
		{ 0x83, 0x3D, 0x74, 0xFA, 0x93, 0x00, 0x12 }
    ;
	
    /*
    */
    byte tech_trade_likeability_new[] =
        { 0x83, 0x3D, 0x74, 0xFA, 0x93, 0x00, (byte)conf.tech_trade_likeability }
    ;
	
    write_bytes
    (
        0x005411DA,
        tech_trade_likeability_old,
        tech_trade_likeability_new,
        tech_trade_likeability_length
    )
    ;
	
}

/**
Shortens status labels to make them fit the status window.
*/
void patch_status_win_info()
{
//	write_call(0x004B78D5, (int)modStatusWinLandmark_strlen);
//	write_call(0x004B7A0C, (int)modStatusWinRiverVegetation_strlen);
	write_call(0x004B7C2E, (int)modStatusWinBonus_bonus_at);
//	write_call(0x004B7CA8, (int)modStatusWinBonus_strlen);
	
}

/**
Sets moved factions = 8 if there is no human player.
*/
void patch_load_game()
{
	write_call(0x0047D5CE, (int)modified_load_game);
	write_call(0x0051BBE3, (int)modified_load_game);
	write_call(0x0058E7A1, (int)modified_load_game);
	write_call(0x0058E879, (int)modified_load_game);
	
}

/**
Disables formers running away from aliens.
*/
void patch_scary_former()
{
	write_call(0x0056B8C5, (int)modified_zoc_veh);
	
}

/**
Deletes supported units in more sane order.
*/
void patch_base_check_support()
{
	write_call(0x004F7A75, (int)modified_base_check_support);
	
}

void patch_zoc_disabled()
{
	write_call(0x005969B7, (int)wtp_mod_quick_zoc);
	write_call(0x005969D2, (int)wtp_mod_quick_zoc);
	write_call(0x00522A70, (int)wtp_mod_zoc_any);
	write_call(0x005668E2, (int)wtp_mod_zoc_any);
	write_call(0x00566DE0, (int)wtp_mod_zoc_any);
	write_call(0x00579351, (int)wtp_mod_zoc_any);
	write_call(0x0057B638, (int)wtp_mod_zoc_any);
	write_call(0x005998FA, (int)wtp_mod_zoc_any);
	write_call(0x0056702F, (int)wtp_mod_zoc_veh);
	write_call(0x0056B8C5, (int)wtp_mod_zoc_veh);
	write_call(0x0056BB82, (int)wtp_mod_zoc_veh);
	write_call(0x005716B0, (int)wtp_mod_zoc_veh);
	write_call(0x005758FA, (int)wtp_mod_zoc_veh);
	write_call(0x00577393, (int)wtp_mod_zoc_veh);
	write_call(0x00577963, (int)wtp_mod_zoc_veh);
	write_call(0x00577B27, (int)wtp_mod_zoc_veh);
	write_call(0x00577E4E, (int)wtp_mod_zoc_veh);
	write_call(0x00577E80, (int)wtp_mod_zoc_veh);
	write_call(0x00598F8E, (int)wtp_mod_zoc_veh);
	write_call(0x0059A089, (int)wtp_mod_zoc_veh);
	write_call(0x0059CDF9, (int)wtp_mod_zoc_veh);
	write_call(0x004EC828, (int)wtp_mod_zoc_sea);
	write_call(0x00560C91, (int)wtp_mod_zoc_sea);
	write_call(0x00561A7F, (int)wtp_mod_zoc_sea);
	write_call(0x00561D21, (int)wtp_mod_zoc_sea);
	write_call(0x0056C5A3, (int)wtp_mod_zoc_sea);
	write_call(0x00571820, (int)wtp_mod_zoc_sea);
	write_call(0x005724F2, (int)wtp_mod_zoc_sea);
	write_call(0x00577281, (int)wtp_mod_zoc_sea);
	write_call(0x0057745B, (int)wtp_mod_zoc_sea);
	write_call(0x00577681, (int)wtp_mod_zoc_sea);
	write_call(0x00577FA2, (int)wtp_mod_zoc_sea);
	write_call(0x005C8D8C, (int)wtp_mod_zoc_sea);
	write_call(0x004CC5E3, (int)wtp_mod_zoc_move);
	write_call(0x004ED6EE, (int)wtp_mod_zoc_move);
	write_call(0x00509478, (int)wtp_mod_zoc_move);
	write_call(0x0056B8B1, (int)wtp_mod_zoc_move);
	write_call(0x0056BBA1, (int)wtp_mod_zoc_move);
	write_call(0x0056C1FF, (int)wtp_mod_zoc_move);
	write_call(0x005723FE, (int)wtp_mod_zoc_move);
	write_call(0x005725EE, (int)wtp_mod_zoc_move);
	write_call(0x00573D7D, (int)wtp_mod_zoc_move);
	write_call(0x00577614, (int)wtp_mod_zoc_move);
	write_call(0x00595395, (int)wtp_mod_zoc_move);
	write_call(0x005953AA, (int)wtp_mod_zoc_move);
	write_call(0x0059BEB2, (int)wtp_mod_zoc_move);
	write_call(0x0059C0B7, (int)wtp_mod_zoc_move);
	write_call(0x0059A937, (int)wtp_mod_zoc_path);
	write_call(0x0059AF1A, (int)wtp_mod_zoc_path);
	write_call(0x0059B45F, (int)wtp_mod_zoc_path);
	write_call(0x0059B7B1, (int)wtp_mod_zoc_path);
	
}

void patch_scroll_other_faction_bases()
{
	// in BaseWin::prev
	// disable check for base PC faction
	
    int scroll_other_faction_bases_prev_length = 0x2;
	
/*
0:  3b c8                   cmp    ecx,eax
*/
    byte scroll_other_faction_bases_prev_old[] =
		{ 0x3B, 0xC8 }
    ;
	
/*
0:  39 c9                   cmp    ecx,ecx
*/
    byte scroll_other_faction_bases_prev_new[] =
        { 0x39, 0xC9 }
    ;
	
    write_bytes
    (
        0x0041A03B,
        scroll_other_faction_bases_prev_old,
        scroll_other_faction_bases_prev_new,
        scroll_other_faction_bases_prev_length
    )
    ;
	
	// in BaseWin::next
	// disable check for base PC faction
	
    int scroll_other_faction_bases_next_length = 0x2;
	
/*
0:  3b c8                   cmp    ecx,eax
*/
    byte scroll_other_faction_bases_next_old[] =
		{ 0x3B, 0xC8 }
    ;
	
/*
0:  39 c9                   cmp    ecx,ecx
*/
    byte scroll_other_faction_bases_next_new[] =
        { 0x39, 0xC9 }
    ;
	
    write_bytes
    (
        0x0041A18B,
        scroll_other_faction_bases_next_old,
        scroll_other_faction_bases_next_new,
        scroll_other_faction_bases_next_length
    )
    ;
	
}

void patch_psych_specialist_content()
{
	// in BaseWin__draw_psych
	// intercept BaseWin__psych_raw call to disable drawing specialists
	
	write_call(0x00408E3B, (int)wtp_mod_BaseWin_psych_row);
	
	// in BaseWin__pop_click
	// intercept call to display popup for bases with pop_size >= min_base_size_specialists
	
	write_call(0x0040A9EB, (int)wtp_mod_BaseWin_pop_click_popup_start);
	
}

/*
Vanilla does not initialize bonus array with zeroes, which causes crash.
*/
void patch_revolt()
{
	// in drone_riot
	// initialize storage variables
	
    int revolt_length = 0x42;
/*
0:  8b 35 70 93 68 00       mov    esi,DWORD PTR ds:0x689370
6:  8d 55 dc                lea    edx,[ebp-0x24]
9:  8d 45 d8                lea    eax,[ebp-0x28]
c:  52                      push   edx
d:  50                      push   eax
e:  6a 23                   push   0x23
10: e8 f4 66 01 00          call   0x16709
15: 8d 0c f6                lea    ecx,[esi+esi*8]
18: 8a 5d dc                mov    bl,BYTE PTR [ebp-0x24]
1b: 83 c4 0c                add    esp,0xc
1e: 8d 14 4e                lea    edx,[esi+ecx*2]
21: 8b 4d d8                mov    ecx,DWORD PTR [ebp-0x28]
24: 8d 04 96                lea    eax,[esi+edx*4]
27: 8b 55 ec                mov    edx,DWORD PTR [ebp-0x14]
2a: 8a 84 81 cc d0 97 00    mov    al,BYTE PTR [ecx+eax*4+0x97d0cc]
31: 8b 8a 40 cc 96 00       mov    ecx,DWORD PTR [edx+0x96cc40]
37: 22 c3                   and    al,bl
39: f6 d8                   neg    al
3b: 1b c0                   sbb    eax,eax
3d: f7 d8                   neg    eax
3f: 8d 14 41                lea    edx,[ecx+eax*2]
*/
    byte revolt_old[] = { 0x8B, 0x35, 0x70, 0x93, 0x68, 0x00, 0x8D, 0x55, 0xDC, 0x8D, 0x45, 0xD8, 0x52, 0x50, 0x6A, 0x23, 0xE8, 0xF4, 0x66, 0x01, 0x00, 0x8D, 0x0C, 0xF6, 0x8A, 0x5D, 0xDC, 0x83, 0xC4, 0x0C, 0x8D, 0x14, 0x4E, 0x8B, 0x4D, 0xD8, 0x8D, 0x04, 0x96, 0x8B, 0x55, 0xEC, 0x8A, 0x84, 0x81, 0xCC, 0xD0, 0x97, 0x00, 0x8B, 0x8A, 0x40, 0xCC, 0x96, 0x00, 0x22, 0xC3, 0xF6, 0xD8, 0x1B, 0xC0, 0xF7, 0xD8, 0x8D, 0x14, 0x41 };
/*
0:  c7 45 a8 00 00 00 00    mov    DWORD PTR [ebp-0x58],0x0
7:  c7 45 ac 00 00 00 00    mov    DWORD PTR [ebp-0x54],0x0
e:  c7 45 b0 00 00 00 00    mov    DWORD PTR [ebp-0x50],0x0
15: c7 45 b4 00 00 00 00    mov    DWORD PTR [ebp-0x4c],0x0
1c: c7 45 b8 00 00 00 00    mov    DWORD PTR [ebp-0x48],0x0
23: c7 45 bc 00 00 00 00    mov    DWORD PTR [ebp-0x44],0x0
2a: c7 45 c0 00 00 00 00    mov    DWORD PTR [ebp-0x40],0x0
...
3b: e8 fd ff ff ff          call   3d <_main+0x3d>
40: 89 c2                   mov    edx,eax
*/
    byte revolt_new[] = { 0xC7, 0x45, 0xA8, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x45, 0xAC, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x45, 0xB0, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x45, 0xB4, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x45, 0xB8, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x45, 0xBC, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x45, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xE8, 0xFD, 0xFF, 0xFF, 0xFF, 0x89, 0xC2 };
    write_bytes(0x004F52F7, revolt_old, revolt_new, revolt_length);
	
	write_call(0x004F52F7 + 0x3b, (int)wtp_mod_base_police_pending);
	
}

void patch_drone_riot_does_not_intensify()
{
	// in drone_riot
	// pretend there were no drone riot on previous turn
	
    int drone_riot_does_not_intensify_length = 0x2;
    byte drone_riot_does_not_intensify_old[] = { 0x85, 0xF6 }; // 0:  85 f6                   test   esi,esi
    byte drone_riot_does_not_intensify_new[] = { 0x85, 0xC0 }; // 0:  85 c0                   test   eax,eax
    write_bytes(0x004F50E7, drone_riot_does_not_intensify_old, drone_riot_does_not_intensify_new, drone_riot_does_not_intensify_length);
	
}

void patch_disable_popb()
{
	write_call(0x004EF2D9, (int)mod_popb);
	write_call(0x004EF64C, (int)mod_popb);
	write_call(0x004EF667, (int)mod_popb);
	write_call(0x004EF9A0, (int)mod_popb);
	write_call(0x004EF9BF, (int)mod_popb);
	write_call(0x004EF9FB, (int)mod_popb);
	write_call(0x004EFA25, (int)mod_popb);
	write_call(0x004EFA44, (int)mod_popb);
	write_call(0x004EFA99, (int)mod_popb);
	write_call(0x004EFAB8, (int)mod_popb);
	write_call(0x004EFAF4, (int)mod_popb);
	write_call(0x004EFB1E, (int)mod_popb);
	write_call(0x004EFB3D, (int)mod_popb);
	write_call(0x004F1825, (int)mod_popb);
	write_call(0x004F1A12, (int)mod_popb);
	write_call(0x004F1B95, (int)mod_popb);
	write_call(0x004F1C8D, (int)mod_popb);
	write_call(0x004F1CA8, (int)mod_popb);
	write_call(0x004F261C, (int)mod_popb);
	write_call(0x004F2747, (int)mod_popb);
	write_call(0x004F4A43, (int)mod_popb);
	write_call(0x004F50D0, (int)mod_popb);
	write_call(0x004F5400, (int)mod_popb);
	write_call(0x004F541D, (int)mod_popb);
	write_call(0x004F5D4A, (int)mod_popb);
	write_call(0x004F5DBE, (int)mod_popb);
	write_call(0x004F5EDD, (int)mod_popb);
	write_call(0x004F642B, (int)mod_popb);
	write_call(0x004F6448, (int)mod_popb);
	write_call(0x004F67B2, (int)mod_popb);
	
}

void patch_console_human_turn()
{
	write_call(0x00527661, (int)wtp_mod_Console_human_turn);
	write_call(0x0052826E, (int)wtp_mod_Console_human_turn);
	write_call(0x005282C9, (int)wtp_mod_Console_human_turn);
	write_call(0x0052A765, (int)wtp_mod_Console_human_turn);
	
}

void patch_enemy_diplomacy()
{
	write_call(0x005272E4, (int)wtp_mod_enemy_diplomacy);
	
}

void patch_diplomacy_probe_action_vendetta_global_friction()
{
	write_call(0x005A4C2F, (int)wtp_mod_probe_treaty_on);
	write_call(0x005A4EB8, (int)wtp_mod_probe_treaty_on);
	write_call(0x005A5375, (int)wtp_mod_probe_treaty_on);
	write_call(0x005A53C2, (int)wtp_mod_probe_treaty_on);
	write_call(0x005A56AA, (int)wtp_mod_probe_treaty_on);
	write_call(0x005A5801, (int)wtp_mod_probe_treaty_on);
	
}

void patch_steal_energy()
{
	write_call(0x005A371C, (int)wtp_mod_steal_energy);
	
}

void patch_diplomacy_caption_display_numeric_mood()
{
	write_call(0x00539B20, (int)wtp_mod_diplomacy_caption_say_fac_special);
	
}

void patch_infiltrate_datalinks_no_promotion()
{
	write_call(0x005A4688, (int)wtp_mod_probe_veh_skip);
	
}

void patch_tidal_harness_terraform_rate()
{
	write_call(0x0040E216, (int)wtp_mod_action_terraform);
	write_call(0x00461AD9, (int)wtp_mod_action_terraform);
	write_call(0x004B5C71, (int)wtp_mod_action_terraform);
	write_call(0x004B85A7, (int)wtp_mod_action_terraform);
	write_call(0x004CB70A, (int)wtp_mod_action_terraform);
	write_call(0x004CB7FA, (int)wtp_mod_action_terraform);
	write_call(0x004CB88D, (int)wtp_mod_action_terraform);
	write_call(0x004CF766, (int)wtp_mod_action_terraform);
	write_call(0x004D31DE, (int)wtp_mod_action_terraform);
	write_call(0x00536A1E, (int)wtp_mod_action_terraform);
	
}

void patch_psi_gate_unlimited()
{
	write_call(0x0046D368, (int)wtp_mod_Console_go_to);
	write_call(0x00516833, (int)wtp_mod_Console_go_to);
	write_call(0x00517397, (int)wtp_mod_Console_go_to);
	write_call(0x005173FE, (int)wtp_mod_Console_go_to);
	write_call(0x0051840D, (int)wtp_mod_Console_go_to);
	write_call(0x0051ABF6, (int)wtp_mod_Console_go_to);
	write_call(0x0051C68D, (int)wtp_mod_Console_go_to);
	write_call(0x0051C8DC, (int)wtp_mod_Console_go_to);
	write_call(0x0051CA49, (int)wtp_mod_Console_go_to);
	write_call(0x0051CA73, (int)wtp_mod_Console_go_to);
	
}

void patch_tech_achieved()
{
	write_call(0x00428136, (int)wtp_mod_tech_achieved);
	write_call(0x00428145, (int)wtp_mod_tech_achieved);
	write_call(0x004DFD3B, (int)wtp_mod_tech_achieved);
	write_call(0x0050C469, (int)wtp_mod_tech_achieved);
	write_call(0x005371C4, (int)wtp_mod_tech_achieved);
	write_call(0x00539652, (int)wtp_mod_tech_achieved);
	write_call(0x0053A3F1, (int)wtp_mod_tech_achieved);
	write_call(0x0053A450, (int)wtp_mod_tech_achieved);
	write_call(0x0053FC48, (int)wtp_mod_tech_achieved);
	write_call(0x0053FC9C, (int)wtp_mod_tech_achieved);
	write_call(0x0053FCF0, (int)wtp_mod_tech_achieved);
	write_call(0x0053FD44, (int)wtp_mod_tech_achieved);
	write_call(0x005405E2, (int)wtp_mod_tech_achieved);
	write_call(0x00540C16, (int)wtp_mod_tech_achieved);
	write_call(0x00540C7F, (int)wtp_mod_tech_achieved);
	write_call(0x00541F9F, (int)wtp_mod_tech_achieved);
	write_call(0x00541FF8, (int)wtp_mod_tech_achieved);
	write_call(0x00542A2A, (int)wtp_mod_tech_achieved);
	write_call(0x00542B4F, (int)wtp_mod_tech_achieved);
	write_call(0x00542CFD, (int)wtp_mod_tech_achieved);
	write_call(0x00542F80, (int)wtp_mod_tech_achieved);
	write_call(0x00542FCE, (int)wtp_mod_tech_achieved);
	write_call(0x005447F3, (int)wtp_mod_tech_achieved);
	write_call(0x00544850, (int)wtp_mod_tech_achieved);
	write_call(0x005457A6, (int)wtp_mod_tech_achieved);
	write_call(0x00546ED6, (int)wtp_mod_tech_achieved);
	write_call(0x00546F38, (int)wtp_mod_tech_achieved);
	write_call(0x00546F95, (int)wtp_mod_tech_achieved);
	write_call(0x00546FEE, (int)wtp_mod_tech_achieved);
	write_call(0x005489F4, (int)wtp_mod_tech_achieved);
	write_call(0x00548B48, (int)wtp_mod_tech_achieved);
	write_call(0x0054A3A2, (int)wtp_mod_tech_achieved);
	write_call(0x0054A49C, (int)wtp_mod_tech_achieved);
	write_call(0x0054DA5F, (int)wtp_mod_tech_achieved);
	write_call(0x00551274, (int)wtp_mod_tech_achieved);
	write_call(0x0055D667, (int)wtp_mod_tech_achieved);
	write_call(0x0055D674, (int)wtp_mod_tech_achieved);
	write_call(0x0055D6FB, (int)wtp_mod_tech_achieved);
	write_call(0x0055D744, (int)wtp_mod_tech_achieved);
	write_call(0x0057C239, (int)wtp_mod_tech_achieved);
	write_call(0x0057D1D4, (int)wtp_mod_tech_achieved);
	write_call(0x005A33B1, (int)wtp_mod_tech_achieved);
	write_call(0x005B29D0, (int)wtp_mod_tech_achieved);
	write_call(0x005B2A01, (int)wtp_mod_tech_achieved);
	write_call(0x005B2A0D, (int)wtp_mod_tech_achieved);
	write_call(0x005B2A79, (int)wtp_mod_tech_achieved);
	write_call(0x005BC24B, (int)wtp_mod_tech_achieved);
	write_call(0x005BC3FF, (int)wtp_mod_tech_achieved);
	write_call(0x005BE603, (int)wtp_mod_tech_achieved);
	
}

void patch_alien_veh_init()
{
	write_call(0x004CEB0A, (int)wtp_mod_alien_veh_init); // action_fungal
	write_call(0x004CECBF, (int)wtp_mod_alien_veh_init); // action_fungal
	write_call(0x004F7143, (int)wtp_mod_alien_veh_init); // base_ecology
	write_call(0x004F7407, (int)wtp_mod_alien_veh_init); // base_ecology
	write_call(0x004F74FC, (int)wtp_mod_alien_veh_init); // base_ecology
	write_call(0x00522789, (int)wtp_mod_alien_veh_init); // alien_fauna
	write_call(0x005227C7, (int)wtp_mod_alien_veh_init); // alien_fauna
	write_call(0x00522820, (int)wtp_mod_alien_veh_init); // alien_fauna
	write_call(0x00522858, (int)wtp_mod_alien_veh_init); // alien_fauna
	write_call(0x00522ABB, (int)wtp_mod_alien_veh_init); // alien_fauna
	write_call(0x00522BA3, (int)wtp_mod_alien_veh_init); // alien_fauna
	write_call(0x00522C49, (int)wtp_mod_alien_veh_init); // alien_fauna
	write_call(0x00522D9B, (int)wtp_mod_alien_veh_init); // alien_fauna
	write_call(0x00522DB2, (int)wtp_mod_alien_veh_init); // alien_fauna
	write_call(0x0059571C, (int)wtp_mod_alien_veh_init); // order_veh
	
}

void patch_capture_base()
{
	// wrap reset_territory when base is captured or killed
	
    write_call(0x004CCF13, (int)wtp_mod_capture_base); // action_airdrop
    write_call(0x00598778, (int)wtp_mod_capture_base); // order_veh
    write_call(0x005A4AB0, (int)wtp_mod_capture_base); // probe
    
}

void patch_retire_proto()
{
    write_call(0x00438322, (int)wtp_mod_retire_proto); // action_airdrop
    write_call(0x0043890A, (int)wtp_mod_retire_proto); // action_airdrop
    write_call(0x00536C68, (int)wtp_mod_retire_proto); // action_airdrop
    write_call(0x00580F4F, (int)wtp_mod_retire_proto); // action_airdrop
	
}

void patch_land_air_superiority_attack_needlejet_at_sea()
{
    write_call(0x005950E4, (int)wtp_mod_has_abil_land_air_superiority_attack_needlejet_at_sea); // order_veh
    
}

void patch_air_superiority_attack_needlejet()
{
    write_call(0x00595274, (int)wtp_mod_has_abil_air_superiority_attack_needlejet); // order_veh
    write_call(0x00578456, (int)wtp_mod_has_abil_air_superiority_attack_needlejet); // enemy_move
    write_call(0x005A132A, (int)wtp_mod_has_abil_air_superiority_attack_needlejet); // probe
    
}


// =======================================================
// main patch option selection
// =======================================================

void patch_setup_wtp(Config* cf)
{
//	// debug mode game speedup
//	
//	if (DEBUG)
//	{
//		patch_disable_boom_delay();
//		patch_disable_battle_refresh();
//		patch_accelerate_order_veh();
//		patch_disable_boom();
//		patch_disable_battle_calls();
//		patch_disable_focus();
//	}
//	
	// patch battle_compute
	
	patch_battle_compute();
	
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
	
	// hurry popup
	
	patch_hurry_popup();
	
	// hurry minimal minerals
	
	if (cf->hurry_minimal_minerals)
	{
		patch_hurry_minimal_minerals();
	}
	
	// patch flat hurry cost
	
	if (cf->flat_hurry_cost)
	{
		patch_flat_hurry_cost();
	}
	
	// patch collateral damage defender reactor
	
	if (cf->collateral_damage_defender_reactor)
	{
		patch_collateral_damage_defender_reactor();
	}
	
	// patch uniform_promotions
	
	if (cf->uniform_promotions)
	{
		patch_uniform_promotions();
	}
	
	// patch sea_territory_distance_same_as_land
	
	if (cf->sea_territory_distance_same_as_land)
	{
		patch_sea_territory_distance_same_as_land();
	}
	
	// patch alternative_artillery_damage
	
	if (cf->alternative_artillery_damage)
	{
		patch_alternative_artillery_damage();
	}
	
	if (cf->disable_current_base_morale_effect)
	{
		patch_disable_current_base_morale_effect();
	}
	
	// patch default_morale_very_green
	
	if (cf->default_morale_very_green)
	{
		patch_default_morale_very_green();
	}
	
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
	
	// feature disabled
//	patch_probe_not_destroy_defense();
	
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
	
	// patch GROWTH rating max
	
	if (cf->se_growth_rating_max != 5)
	{
		patch_se_growth_rating_max(cf->se_growth_rating_max);
	}
	
	patch_display_base_nutrient_cost_factor();
	
	patch_growth_turns_population_boom();
	
	// patch population incorrect superdrones in base screen population
	
//	patch_base_scren_population_superdrones();
	
	patch_weapon_help_always_show_cost();
	
	if (cf->fix_mineral_contribution)
	{
		patch_mineral_contribution();
	}
	
	patch_pact_base_map_production_display();
	
	if (cf->modified_probe_risks)
	{
		patch_modified_probe_risks();
	}
	
	patch_min_knowledge_price();
	
	patch_best_defender();
	
	if (cf->break_treaty_before_fight)
	{
		patch_break_treaty_before_fight();
	}
	
	patch_talent_display();
	
	if (cf->unit_upgrade_ignores_movement)
	{
		patch_unit_upgrade_ignores_movement();
	}
	
	patch_disable_air_transport_unload_everywhere();
	
	patch_enemy_move();
	
	patch_faction_upkeep();
	
	patch_disable_move_territory_restrictions();
	
	if (cf->silent_vendetta_warning)
	{
		patch_silent_vendetta_warning();
	}
	
	if (cf->design_cost_in_rows)
	{
		patch_design_cost_in_rows();
	}
	
	if (cf->carry_over_nutrients)
	{
		patch_carry_over_nutrients();
	}
	
	if (cf->carry_over_minerals)
	{
		patch_carry_over_minerals();
	}
	
	if (cf->subversion_allow_stacked_units)
	{
		patch_subversion_allow_stacked_units();
	}
	
	if (cf->alternative_subversion_and_mind_control)
	{
		patch_alternative_subversion_and_mind_control();
	}
	
	if (!cf->capture_base_destroys_facilities)
	{
		patch_capture_base_does_not_destroy_facilities();
	}
	
	patch_exact_odds();
	
	patch_obsoletion();
	
	patch_turn_upkeep();
	
	if (cf->sensor_indestructible)
	{
		patch_sensor_indestructible();
	}
	
	patch_tech_value();
	
	if (cf->disable_vanilla_base_hurry)
	{
		patch_disable_vanilla_base_hurry();
	}
	
	patch_enemy_units_check();
	
	patch_tech_ai_randomization();
	
	patch_pact_withdraw();
	
	if (cf->disengagement_from_stack)
	{
		patch_disengagement_from_stack();
	}
	
	patch_order_veh();
	
	patch_base_attack();
	
	patch_vehicle_boom();
	
	patch_transport_pick_everybody();
	
	patch_non_transport_stop_in_base();
	
	patch_disable_alien_ranged_from_transport();
	
	patch_disable_kill_ai();
	
	if (conf.tech_trade_likeability != 0x12)
	{
		patch_tech_trade_likeability();
	}
	
	patch_status_win_info();
	
	patch_load_game();
	
	patch_scary_former();
	
	patch_base_check_support();
	
	if (!conf.zoc_enabled)
	{
		patch_zoc_disabled();
	}
	
	patch_scroll_other_faction_bases();
	
	if (conf.base_psych && conf.base_psych_improved)
	{
		patch_psych_specialist_content();
	}
	
	patch_revolt();
	
	if (!conf.drone_riot_intensifies)
	{
		patch_drone_riot_does_not_intensify();
	}
	
	patch_disable_popb();
	
	patch_console_human_turn();
	
	patch_enemy_diplomacy();
	
	if (conf.diplomacy_probe_action_vendetta_global_friction > 0)
	{
		patch_diplomacy_probe_action_vendetta_global_friction();
	}
	
	patch_steal_energy();
	
	if (conf.display_numeric_mood)
	{
		patch_diplomacy_caption_display_numeric_mood();
	}
	
	if (conf.display_numeric_mood)
	{
		patch_diplomacy_caption_display_numeric_mood();
	}
	
	patch_infiltrate_datalinks_no_promotion();
	
	patch_tidal_harness_terraform_rate();
	
	patch_psi_gate_unlimited();
	
	patch_tech_achieved();
	
	patch_alien_veh_init();
	
	patch_alien_veh_init();
	
	patch_capture_base();
	
	patch_retire_proto();
	
	if (conf.air_superiority_not_required_to_attack_needlejet)
	{
		patch_air_superiority_attack_needlejet();
	}
	
}

void shiftVehicleAddress(byte *bytes, int position)
{
	if (!conf.modify_unit_limit)
		return;
	
	int address = 0x1000000 * bytes[position + 3] + 0x10000 * bytes[position + 2] + 0x100 * bytes[position + 1] + 0x1 * bytes[position + 0];
	address += ((int)VehsMod - (int)VehsDef);
	bytes[position + 3] = (address >> 24) % 0x100;
	bytes[position + 2] = (address >> 16) % 0x100;
	bytes[position + 1] = (address >>  8) % 0x100;
	bytes[position + 0] = (address >>  0) % 0x100;
	
}

