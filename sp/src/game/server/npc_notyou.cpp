//
// Player-mirroring miniboss.
// Capable of dashing, experiences impatience against hiding players
//

#include "cbase.h"
#include "ai_basenpc.h"
#include "ai_basehumanoid.h"
#include "ai_behavior.h"
#include "ai_default.h"
#include "ai_task.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_squadslot.h"
#include "ai_squad.h"
#include "soundent.h"
#include "game.h"
#include "npcevent.h"
#include "entitylist.h"
#include "activitylist.h"
#include "player.h"
#include "basecombatweapon.h"
#include "basegrenade_shared.h"
#include "npc_notyou.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "globals.h"
#include "grenade_frag.h"
#include "ndebugoverlay.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "mapbase/ai_grenade.h"
#include "mapbase/GlobalStrings.h"
#include "globalstate.h"
#include "sceneentity.h"

ConVar	sk_notyou_health("sk_notyou_health", "0");
#define NPC_NOTYOU_MODEL "models/police.mdl" //TEMPTEMPTEMPTEMP

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Animation events
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Interactions.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Custom Activities
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//	Squad slots
//-----------------------------------------------------------------------------
enum SquadSlot_t
{
	SQUAD_SLOT_NOTYOU_RUSH = LAST_SHARED_SQUADSLOT,
	SQUAD_SLOT_RUN_SHOOT,
};

class CNPC_NotYou : public CAI_BaseNPC
{
	DECLARE_CLASS(CNPC_NotYou, CAI_BaseNPC);
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;

public:
	void	Precache(void);
	void	Spawn(void);
	Class_T Classify(void);
	bool			ShouldMoveAndShoot();
	float			m_flStopMoveShootTime;
private:
	enum
	{
		SCHED_NOTYOU_SCHEDULE = BaseClass::NEXT_SCHEDULE,
		SCHED_NOTYOU_HIDE_AND_RELOAD,
		NEXT_SCHEDULE,
	};

	enum
	{
		TASK_NOTYOU_TASK = BaseClass::NEXT_TASK,
	};

	enum
	{
		COND_NEORTOU_CONDITION = BaseClass::NEXT_CONDITION,
	};
};

LINK_ENTITY_TO_CLASS(npc_notyou, CNPC_NotYou);

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC(CNPC_NotYou)

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_NotYou::Precache(void)
{
	PrecacheModel(NPC_NOTYOU_MODEL);

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_NotYou::Spawn(void)
{
	Precache();

	SetModel(NPC_NOTYOU_MODEL);
	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MOVETYPE_STEP);
	SetBloodColor(BLOOD_COLOR_RED);
	SetHealth(sk_notyou_health.GetFloat());
	m_flFieldOfView = 0.0; // indicates the width of this NPC's forward view cone ( as a dotproduct result )
	m_NPCState = NPC_STATE_NONE;
	m_flStopMoveShootTime = FLT_MAX; // Move and shoot defaults on.

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_TURN_HEAD | bits_CAP_MOVE_GROUND);
	CapabilitiesAdd(bits_CAP_MOVE_SHOOT);

	NPCInit();
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
// Output : 
//-----------------------------------------------------------------------------
Class_T	CNPC_NotYou::Classify(void)
{
	return	CLASS_NOTYOU;
}

bool CNPC_NotYou::ShouldMoveAndShoot()
{
	return BaseClass::ShouldMoveAndShoot();
}

//-------------------------------------------------------------------------------------------------
//
// Schedules
//
//-------------------------------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC(npc_notyou, CNPC_NotYou)

	DECLARE_SQUADSLOT(SQUAD_SLOT_NOTYOU_RUSH)

	DEFINE_SCHEDULE
	(
		SCHED_NOTYOU_HIDE_AND_RELOAD,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_RELOAD"
		"		TASK_FACE_ENEMY				0"
		"		TASK_FIND_COVER_FROM_ENEMY	0"
		"		TASK_RUN_PATH				0"
		"		TASK_WAIT_FOR_MOVEMENT		0"
		"		TASK_REMEMBER				MEMORY:INCOVER"
		"		TASK_RELOAD					0"
		""
		"	Interrupts"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_MOVE_AWAY"
	)


AI_END_CUSTOM_NPC()