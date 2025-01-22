#ifndef NPC_NOTYOU
#define NPC_NOTYOU
#ifdef _WIN32
#pragma once
#endif

#include "ai_basenpc.h"
#include "ai_basehumanoid.h"
#include "ai_behavior.h"
#include "ai_behavior_assault.h"
#include "ai_behavior_standoff.h"
#include "ai_behavior_follow.h"
#include "ai_behavior_functank.h"
#include "ai_behavior_rappel.h"
#include "ai_behavior_actbusy.h"
#include "ai_sentence.h"
#include "ai_baseactor.h"
#ifdef MAPBASE
#include "mapbase/ai_grenade.h"
#include "ai_behavior_police.h"
#endif

class CNPC_NotYou : public CAI_BaseNPC
{
	DECLARE_CLASS(CNPC_NotYou, CAI_BaseNPC);
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;

public:
	void	Precache(void);
	void	Spawn(void);
	void	PrescheduleThink(void);
	Class_T Classify(void);
	WeaponProficiency_t CalcWeaponProficiency(CBaseCombatWeapon* pWeapon);
	bool			ShouldMoveAndShoot();
	float			m_flStopMoveShootTime;
	bool			UpdateEnemyMemory(CBaseEntity* pEnemy, const Vector& position, CBaseEntity* pInformer = NULL);

	int				m_nShots;
	float			m_flShotDelay;

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

class CBaseEntity;

#endif