// DEPRECATED
// Mix of Combine Sniper bullets and Hunter Flechettes
// High damage piercing shot
// DEPRECATED

#include "cbase.h"
#include "physobj.h"
#include "particle_parse.h"
#include "datacache/imdlcache.h"
#include "IEffects.h"
#include "te_effect_dispatch.h"
#include "soundent.h"
#include "pierceshot.h"
#ifdef MAPBASE
#include "mapbase/GlobalStrings.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//This model won't be rendered but we still want it for hitboxing
static const char* PIERCESHOT_MODEL = "models/weapons/hunter_flechette.mdl";

#define PIERCESHOT_AIR_VELOCITY	5500

ConVar piercer_speed("piercer_speed", "10000");
ConVar sk_piercer_dmg("sk_pierceshot_dmg", "60.0");

LINK_ENTITY_TO_CLASS(tyr_pierceshot, CPierceShot);

BEGIN_DATADESC(CPierceShot)

END_DATADESC()

CPierceShot *CPierceShot::FirePierce(const Vector& vecOrigin, const QAngle& angAngles, CBaseEntity* pentOwner)
{
	// Create a new entity with CFlechette private data
	CPierceShot* pPiercer = (CPierceShot*)CreateEntityByName("tyr_pierceshot");
	UTIL_SetOrigin(pPiercer, vecOrigin);
	pPiercer->SetAbsAngles(angAngles);
	pPiercer->Spawn();
	pPiercer->Activate();
	pPiercer->SetOwnerEntity(pentOwner);
	pPiercer->SetRenderMode(kRenderNone);
	pPiercer->AddEffects(EF_NOSHADOW);

	return pPiercer;
}

CPierceShot::CPierceShot()
{
	UseClientSideAnimation();
}

CPierceShot::~CPierceShot()
{
}

void CPierceShot::Spawn()
{
	Precache();

	SetModel(PIERCESHOT_MODEL);
	SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM);
	UTIL_SetSize(this, -Vector(1, 1, 1), Vector(1, 1, 1));
	SetSolid(SOLID_BBOX);
	SetGravity(0.01f);
	SetCollisionGroup(COLLISION_GROUP_PROJECTILE);

	// Make sure we're updated if we're underwater
	UpdateWaterState();

	SetTouch(&CPierceShot::PierceHit);
}

void CPierceShot::Precache()
{
	PrecacheModel(PIERCESHOT_MODEL);

	PrecacheScriptSound("PierceShot.NearMiss");
	PrecacheScriptSound("PierceShot.HitBody");
	PrecacheScriptSound("PierceShot.HitWorld");
}

bool CPierceShot::CreateVPhysics()
{
	// Create the object in the physics system
	VPhysicsInitNormal(SOLID_BBOX, FSOLID_NOT_STANDABLE, false);

	return true;
}

void CPierceShot::PierceHit(CBaseEntity* pOther)
{
	if (pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER))
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
		if ((pOther->m_takedamage == DAMAGE_NO) || (pOther->m_takedamage == DAMAGE_EVENTS_ONLY))
			return;
	}

	trace_t	tr;
	tr = BaseClass::GetTouchTrace();

	if (pOther->m_takedamage != DAMAGE_NO)
	{
		Vector	vecNormalizedVel = GetAbsVelocity();

		ClearMultiDamage();
		VectorNormalize(vecNormalizedVel);

		float flDamage = sk_piercer_dmg.GetFloat();
		CBreakable* pBreak = dynamic_cast <CBreakable*>(pOther);
		if (pBreak && (pBreak->GetMaterialType() == matGlass))
		{
			flDamage = MAX(pOther->GetHealth(), flDamage);
		}

		CTakeDamageInfo	dmgInfo(this, GetOwnerEntity(), flDamage, DMG_BULLET | DMG_NEVERGIB);
		CalculateMeleeDamageForce(&dmgInfo, vecNormalizedVel, tr.endpos, 0.7f);
		dmgInfo.SetDamagePosition(tr.endpos);
		pOther->DispatchTraceAttack(dmgInfo, vecNormalizedVel, &tr);

		ApplyMultiDamage();

		// Keep going through breakable glass.
		if (pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS)
			return;

		// Pierce Shots pass through NPCs and phys objects
		if (pOther->IsNPC() || pOther->GetMoveType() == MOVETYPE_VPHYSICS || (pOther->GetMoveType() == MOVETYPE_PUSH))
		{
			m_vecDir = GetAbsVelocity();
			VectorNormalize(m_vecDir);
			Vector vecCursor = tr.endpos;
			vecCursor += m_vecDir * 32;

			if (UTIL_PointContents(vecCursor) != CONTENTS_SOLID)
			{
				// Passed through the entity
				SetAbsOrigin(vecCursor);
				return;
			}
			else
			{
				// Elsewise, it stops
				SetTouch(NULL);
				SetThink(NULL);

				UTIL_Remove(this);
			}
		}

		SetAbsVelocity(Vector(0, 0, 0));

		// play impact sound!!
		EmitSound("PierceShot.HitBody");

		StopParticleEffects(this);

		Vector vForward;
		AngleVectors(GetAbsAngles(), &vForward);
		VectorNormalize(vForward);

		UTIL_Remove(this);
	}
	// Put a mark unless we've hit the sky
	else if ((tr.surface.flags & SURF_SKY) == false)
	{
		UTIL_ImpactTrace(&tr, DMG_BULLET);
		SetTouch(NULL);
		SetThink(NULL);

		UTIL_Remove(this);
	}
	else
	{
		SetTouch(NULL);
		SetThink(NULL);

		UTIL_Remove(this);
	}
}

void CPierceShot::Shoot(Vector& vecVelocity, bool bBrightFX)
{

	m_vecShootPosition = GetAbsOrigin();

	SetAbsVelocity(vecVelocity);

	SetThink(&CPierceShot::DopplerThink);
	SetNextThink(gpGlobals->curtime);
}

void CPierceShot::DopplerThink()
{
	CBasePlayer* pPlayer = AI_GetSinglePlayer();
	if (!pPlayer)
		return;

	Vector vecVelocity = GetAbsVelocity();
	VectorNormalize(vecVelocity);

	float flMyDot = DotProduct(vecVelocity, GetAbsOrigin());
	float flPlayerDot = DotProduct(vecVelocity, pPlayer->GetAbsOrigin());

	if (flPlayerDot <= flMyDot)
	{
		EmitSound("NPC_Hunter.FlechetteNearMiss");

		// We've played the near miss sound and we're not seeking. Stop thinking.
		SetThink(NULL);
	}
	else
	{
		SetNextThink(gpGlobals->curtime);
	}
}

void CPierceShot::OnParentCollisionInteraction(parentCollisionInteraction_t eType, int index, gamevcollisionevent_t* pEvent)
{
}
void CPierceShot::OnParentPhysGunDrop(CBasePlayer* pPhysGunUser, PhysGunDrop_t Reason)
{
}