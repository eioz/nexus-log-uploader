#pragma once

#include <Windows.h>
#include <cstdint>
#include <imgui.h>

enum cbtstatechange : uint8_t
{
	CBTS_NONE,
	CBTS_ENTERCOMBAT,
	CBTS_EXITCOMBAT,
	CBTS_CHANGEUP,
	CBTS_CHANGEDEAD,
	CBTS_CHANGEDOWN,
	CBTS_SPAWN,
	CBTS_DESPAWN,
	CBTS_HEALTHUPDATE,
	CBTS_LOGSTART,
	CBTS_LOGEND,
	CBTS_WEAPSWAP,
	CBTS_MAXHEALTHUPDATE,
	CBTS_POINTOFVIEW,
	CBTS_LANGUAGE,
	CBTS_GWBUILD,
	CBTS_SHARDID,
	CBTS_REWARD,
	CBTS_BUFFINITIAL,
	CBTS_POSITION,
	CBTS_VELOCITY,
	CBTS_FACING,
	CBTS_TEAMCHANGE,
	CBTS_ATTACKTARGET,
	CBTS_TARGETABLE,
	CBTS_MAPID,
	CBTS_REPLINFO,
	CBTS_STACKACTIVE,
	CBTS_STACKRESET,
	CBTS_GUILD,
	CBTS_BUFFINFO,
	CBTS_BUFFFORMULA,
	CBTS_SKILLINFO,
	CBTS_SKILLTIMING,
	CBTS_BREAKBARSTATE,
	CBTS_BREAKBARPERCENT,
	CBTS_ERROR,
	CBTS_TAG,
	CBTS_BARRIERUPDATE,
	CBTS_STATRESET,
	CBTS_EXTENSION,
	CBTS_APIDELAYED,
	CBTS_INSTANCESTART,
	CBTS_TICKRATE,
	CBTS_LAST90BEFOREDOWN,
	CBTS_EFFECT,
	CBTS_IDTOGUID,
	CBTS_LOGNPCUPDATE,
	CBTS_UNKNOWN,
};

enum cbtactivation : uint8_t
{
	ACTV_NONE,
	ACTV_START,
	ACTV_QUICKNESS_UNUSED,
	ACTV_CANCEL_FIRE,
	ACTV_CANCEL_CANCEL,
	ACTV_RESET,
	ACTV_UNKNOWN,
};

enum cbtbuffremove : uint8_t
{
	CBTB_NONE,
	CBTB_ALL,
	CBTB_SINGLE,
	CBTB_MANUAL,
	CBTB_UNKNOWN,
};

struct cbtevent
{
	uint64_t time;
	uintptr_t src_agent;
	uintptr_t dst_agent;
	int32_t value;
	int32_t buff_dmg;
	uint32_t overstack_value;
	uint32_t skillid;
	uint16_t src_instid;
	uint16_t dst_instid;
	uint16_t src_master_instid;
	uint16_t dst_master_instid;
	uint8_t iff;
	uint8_t buff;
	uint8_t result;
	cbtactivation is_activation;
	cbtbuffremove is_buffremove;
	uint8_t is_ninety;
	uint8_t is_fifty;
	uint8_t is_moving;
	cbtstatechange is_statechange;
	uint8_t is_flanking;
	uint8_t is_shields;
	uint8_t is_offcycle;
	uint8_t pad61;
	uint8_t pad62;
	uint8_t pad63;
	uint8_t pad64;
};

struct ag
{
	const char* name;
	uintptr_t id;
	uint32_t prof;
	uint32_t elite;
	uint32_t self;
	uint16_t team;
};

struct arcdps_exports
{
	uintptr_t size;
	uint32_t sig;
	uint32_t imguivers;
	const char* out_name;
	const char* out_build;
	uintptr_t (*wnd_nofilter)(HWND, UINT, WPARAM, LPARAM);
	uintptr_t (*combat)(cbtevent*, ag*, ag*, const char*, uint64_t, uint64_t);
	uintptr_t (*imgui)(uint32_t);
	uintptr_t (*options_end)();
	uintptr_t (*combat_local)(cbtevent*, ag*, ag*, const char*, uint64_t, uint64_t);
	uintptr_t (*wnd_filter)(HWND, UINT, WPARAM, LPARAM);
	uintptr_t (*options_windows)(const char*);
};
