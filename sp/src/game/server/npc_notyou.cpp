//
// Player-mirroring miniboss.
// Capable of dashing, experiences impatience against hiding players
//

#include "cbase.h"
#include "ai_default.h"
#include "ai_task.h"
#include "ai_schedule.h"
#include "ai_node.h"
#include "ai_hull.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_senses.h"
#include "ai_navigator.h"
#include "ai_motor.h"
#include "ai_behavior_lead.h"
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
#include "mapbase/GlobalStrings.h"
#include "globalstate.h"
#include "sceneentity.h"

ConVar	sk_notyou_health("sk_notyou_health", "0");
#define NPC_NOTYOU_MODEL "models/combine_soldier.mdl" //TEMPTEMPTEMPTEMP

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

LINK_ENTITY_TO_CLASS(npc_notyou, CNPC_NotYou);

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC(CNPC_NotYou)
	DEFINE_FIELD(m_nShots, FIELD_INTEGER),
	DEFINE_FIELD(m_flShotDelay, FIELD_FLOAT),
	DEFINE_FIELD(m_flStopMoveShootTime, FIELD_TIME),
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
	CapabilitiesAdd(bits_CAP_AIM_GUN);
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
	m_flStopMoveShootTime = FLT_MAX;

	return BaseClass::ShouldMoveAndShoot();
}

void CNPC_NotYou::PrescheduleThink()
{
	BaseClass::PrescheduleThink();

	if (gpGlobals->curtime >= m_flStopMoveShootTime)
	{
		// Time to stop move and shoot and start facing the way I'm running.
		// This makes the combine look attentive when disengaging, but prevents
		// them from always running around facing you.
		//
		// Only do this if it won't be immediately shut off again.
		if (GetNavigator()->GetPathTimeToGoal() > 1.0f)
		{
			m_MoveAndShootOverlay.SuspendMoveAndShoot(5.0f);
			m_flStopMoveShootTime = FLT_MAX;
		}
	}

	if (m_flGroundSpeed > 0 && GetState() == NPC_STATE_COMBAT && m_MoveAndShootOverlay.IsSuspended())
	{
		// Return to move and shoot when near my goal so that I 'tuck into' the location facing my enemy.
		if (GetNavigator()->GetPathTimeToGoal() <= 1.0f)
		{
			m_MoveAndShootOverlay.SuspendMoveAndShoot(0);
		}
	}
}

bool CNPC_NotYou::UpdateEnemyMemory(CBaseEntity* pEnemy, const Vector& position, CBaseEntity* pInformer)
{

	return BaseClass::UpdateEnemyMemory(pEnemy, position, pInformer);
}

//Stealing this from combine_s for now
WeaponProficiency_t CNPC_NotYou::CalcWeaponProficiency(CBaseCombatWeapon* pWeapon)
{
#ifdef MAPBASE
	if (pWeapon->ClassMatches(gm_isz_class_AR2))
#else
	if (FClassnameIs(pWeapon, "weapon_ar2"))
#endif
	{
		if (hl2_episodic.GetBool())
		{
			return WEAPON_PROFICIENCY_VERY_GOOD;
		}
		else
		{
			return WEAPON_PROFICIENCY_GOOD;
		}
	}
#ifdef MAPBASE
	else if (pWeapon->ClassMatches(gm_isz_class_Shotgun))
#else
	else if (FClassnameIs(pWeapon, "weapon_shotgun"))
#endif
	{
		return WEAPON_PROFICIENCY_PERFECT;
	}
#ifdef MAPBASE
	else if (pWeapon->ClassMatches(gm_isz_class_SMG1))
#else
	else if (FClassnameIs(pWeapon, "weapon_smg1"))
#endif
	{
		return WEAPON_PROFICIENCY_GOOD;
	}
#ifdef MAPBASE
	else if (pWeapon->ClassMatches(gm_isz_class_Pistol))
	{
		// Mods which need a lower soldier pistol accuracy can either change this value or use proficiency override in Hammer.
		return WEAPON_PROFICIENCY_VERY_GOOD;
	}
#endif

	return BaseClass::CalcWeaponProficiency(pWeapon);
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

