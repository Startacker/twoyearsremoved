//
// Clones Hunter Flechettes into their own file for ease of access
// Effectively a mix of the SMG grenade and flechettes, more direct projectile, stick on hit, THEN explode
//

#include "cbase.h"
#include "physobj.h"
#include "particle_parse.h"
#include "datacache/imdlcache.h"
#include "IEffects.h"
#include "te_effect_dispatch.h"
#include "soundent.h"
#include "flechettes.h"
#ifdef MAPBASE
#include "mapbase/GlobalStrings.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char* FLECHETTE_MODEL = "models/weapons/hunter_flechette.mdl";

static const char* s_szTYRFlechetteBubbles = "FlechetteBubbles";
static const char* s_szTYRFlechetteSeekThink = "FlechetteSeekThink";
static const char* s_szTYRFlechetteDangerSoundThink = "FlechetteDangerSoundThink";
static const char* s_szTYRFlechetteSpriteTrail = "sprites/bluelaser1.vmt";
static int s_TYRFlechetteImpact = -2;
static int s_TYRFlechetteFuseAttach = -1;

#define FLECHETTE_TYR_AIR_VELOCITY	2500
#define FLECHETTE_WARN_TIME		1.0f

ConVar flechette_speed("flechette_speed", "2000");
ConVar sk_flechette_dmg("sk_flechette_dmg", "4.0");
ConVar sk_flechette_explode_dmg("sk_flechette_explode_dmg", "75.0");
ConVar sk_flechette_explode_radius("sk_flechette_explode_radius", "128.0");
ConVar sk_flechette_stuck_explode_radius("sk_flechette_stuck_explode_radius", "196.0");
ConVar flechette_explode_delay("flechette_explode_delay", "1.0");
ConVar flechette_delay("flechette_delay", "0.1");
ConVar flechette_cheap_explosions("flechette_cheap_explosions", "1");



LINK_ENTITY_TO_CLASS(tyr_flechette, CFlechette);

BEGIN_DATADESC(CFlechette)

DEFINE_THINKFUNC(BubbleThink),
DEFINE_THINKFUNC(DangerSoundThink),
DEFINE_THINKFUNC(ExplodeThink),
DEFINE_THINKFUNC(DopplerThink),
DEFINE_THINKFUNC(SeekThink),

DEFINE_ENTITYFUNC(FlechetteTouch),

DEFINE_FIELD(m_vecDir, FIELD_VECTOR),
DEFINE_FIELD(m_vecShootPosition, FIELD_POSITION_VECTOR),
DEFINE_FIELD(m_hSeekTarget, FIELD_EHANDLE),
DEFINE_FIELD(m_stuckEnemy, FIELD_BOOLEAN),

END_DATADESC()

CFlechette *CFlechette::FlechetteCreate(const Vector& vecOrigin, const QAngle& angAngles, CBaseEntity* pentOwner)
{
	// Create a new entity with CFlechette private data
	CFlechette *pFlechette = (CFlechette *)CreateEntityByName("tyr_flechette");
	UTIL_SetOrigin(pFlechette, vecOrigin);
	pFlechette->SetAbsAngles(angAngles);
	pFlechette->Spawn();
	pFlechette->Activate();
	pFlechette->SetOwnerEntity(pentOwner);
	pFlechette->AddEffects(EF_NOSHADOW);

	return pFlechette;
}

void CFlechette::Shoot_Flechette()
{
	MDLCACHE_CRITICAL_SECTION();

	bool allowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache(true);

	CBasePlayer* pPlayer = UTIL_GetCommandClient();

	QAngle angEye = pPlayer->EyeAngles();
	CFlechette* entity = CFlechette::FlechetteCreate(pPlayer->EyePosition(), angEye, pPlayer);
	if (entity)
	{
		entity->Precache();
		DispatchSpawn(entity);

		// Shoot the flechette.		
		Vector forward;
		pPlayer->EyeVectors(&forward);
		forward *= 2000.0f;
		entity->Shoot(forward, false);
	}

	CBaseEntity::SetAllowPrecache(allowPrecache);
}

CFlechette::CFlechette()
{
	UseClientSideAnimation();
}

CFlechette::~CFlechette()
{
}

void CFlechette::SetSeekTarget(CBaseEntity* pTargetEntity)
{
	if (pTargetEntity)
	{
		m_hSeekTarget = pTargetEntity;
		SetContextThink(&CFlechette::SeekThink, gpGlobals->curtime, s_szTYRFlechetteSeekThink);
	}
}

bool CFlechette::CreateVPhysics()
{
	// Create the object in the physics system
	VPhysicsInitNormal(SOLID_BBOX, FSOLID_NOT_STANDABLE, false);

	return true;
}

unsigned int CFlechette::PhysicsSolidMaskForEntity() const
{
	return (BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX) & ~CONTENTS_GRATE;
}

void CFlechette::OnParentCollisionInteraction(parentCollisionInteraction_t eType, int index, gamevcollisionevent_t* pEvent)
{
	if (eType == COLLISIONINTER_PARENT_FIRST_IMPACT)
	{
		m_bThrownBack = true;
		Explode();
	}
}

bool CFlechette::CreateSprites(bool bBright)
{
	if (bBright)
	{
		DispatchParticleEffect("hunter_flechette_trail_striderbuster", PATTACH_ABSORIGIN_FOLLOW, this);
	}
	else
	{
		DispatchParticleEffect("hunter_flechette_trail", PATTACH_ABSORIGIN_FOLLOW, this);
	}

	return true;
}

void CFlechette::Spawn()
{
	Precache();

	SetModel(FLECHETTE_MODEL);
	SetMoveType(MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM);
	UTIL_SetSize(this, -Vector(1, 1, 1), Vector(1, 1, 1));
	SetSolid(SOLID_BBOX);
	SetGravity(0.05f);
	SetCollisionGroup(COLLISION_GROUP_PROJECTILE);

	// Make sure we're updated if we're underwater
	UpdateWaterState();

	SetTouch(&CFlechette::FlechetteTouch);

	// Make us glow until we've hit the wall
	m_nSkin = 1;
}

void CFlechette::Activate()
{
	BaseClass::Activate();
	SetupGlobalModelData();
}

void CFlechette::SetupGlobalModelData()
{
	if (s_TYRFlechetteImpact == -2)
	{
		s_TYRFlechetteImpact = LookupSequence("impact");
		s_TYRFlechetteFuseAttach = LookupAttachment("attach_fuse");
	}
}

void CFlechette::Precache()
{
	PrecacheModel(FLECHETTE_MODEL);
	PrecacheModel("sprites/light_glow02_noz.vmt");

	PrecacheScriptSound("NPC_Hunter.FlechetteNearmiss");
	PrecacheScriptSound("NPC_Hunter.FlechetteHitBody");
	PrecacheScriptSound("NPC_Hunter.FlechetteHitWorld");
	PrecacheScriptSound("NPC_Hunter.FlechettePreExplode");
	PrecacheScriptSound("NPC_Hunter.FlechetteExplode");

	PrecacheParticleSystem("hunter_flechette_trail_striderbuster");
	PrecacheParticleSystem("hunter_flechette_trail");
	PrecacheParticleSystem("hunter_projectile_explosion_1");
	PrecacheParticleSystem("hunter_projectile_explosion_3");
}

void CFlechette::StickTo(CBaseEntity* pOther, trace_t& tr)
{
	EmitSound("NPC_Hunter.FlechetteHitWorld");

	SetMoveType(MOVETYPE_NONE);

	if (!pOther->IsWorld())
	{
		SetParent(pOther);
		SetSolid(SOLID_NONE);
		SetSolidFlags(FSOLID_NOT_SOLID);
	}

	// Do an impact effect.
	//Vector vecDir = GetAbsVelocity();
	//float speed = VectorNormalize( vecDir );

	//Vector vForward;
	//AngleVectors( GetAbsAngles(), &vForward );
	//VectorNormalize ( vForward );

	//CEffectData	data;
	//data.m_vOrigin = tr.endpos;
	//data.m_vNormal = vForward;
	//data.m_nEntIndex = 0;
	//DispatchEffect( "BoltImpact", data );

	Vector vecVelocity = GetAbsVelocity();

	SetTouch(NULL);

	// We're no longer flying. Stop checking for water volumes.
	SetContextThink(NULL, 0, s_szTYRFlechetteBubbles);

	// Stop seeking.
	m_hSeekTarget = NULL;
	SetContextThink(NULL, 0, s_szTYRFlechetteSeekThink);

	// Get ready to explode.
		DangerSoundThink();

	// Play our impact animation.
	ResetSequence(s_TYRFlechetteImpact);

	static int s_nImpactCount = 0;
	s_nImpactCount++;
	if (s_nImpactCount & 0x01)
	{
		UTIL_ImpactTrace(&tr, DMG_BULLET);

		// Shoot some sparks
		if (UTIL_PointContents(GetAbsOrigin()) != CONTENTS_WATER)
		{
			g_pEffects->Sparks(GetAbsOrigin());
		}
	}
}

void CFlechette::SeekThink()
{
	if (m_hSeekTarget)
	{
		Vector vecBodyTarget = m_hSeekTarget->BodyTarget(GetAbsOrigin());

		Vector vecClosest;
		CalcClosestPointOnLineSegment(GetAbsOrigin(), m_vecShootPosition, vecBodyTarget, vecClosest, NULL);

		Vector vecDelta = vecBodyTarget - m_vecShootPosition;
		VectorNormalize(vecDelta);

		QAngle angShoot;
		VectorAngles(vecDelta, angShoot);

		float flSpeed = flechette_speed.GetFloat();
		if (!flSpeed)
		{
			flSpeed = 2500.0f;
		}

		Vector vecVelocity = vecDelta * flSpeed;
		Teleport(&vecClosest, &angShoot, &vecVelocity);

		SetNextThink(gpGlobals->curtime, s_szTYRFlechetteSeekThink);
	}
}

void CFlechette::FlechetteTouch(CBaseEntity* pOther)
{
	if (pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS | FSOLID_TRIGGER))
	{
		// Some NPCs are triggers that can take damage (like antlion grubs). We should hit them.
		if ((pOther->m_takedamage == DAMAGE_NO) || (pOther->m_takedamage == DAMAGE_EVENTS_ONLY))
			return;
	}

	if (FClassnameIs(pOther, "flechette"))
		return;

	trace_t	tr;
	tr = BaseClass::GetTouchTrace();

	// Flechettes pass through player allies
	if (pOther->Classify() == CLASS_PLAYER_ALLY)
	{
		m_vecDir = GetAbsVelocity();
		VectorNormalize(m_vecDir);
		Vector vecCursor = tr.endpos;
		vecCursor += m_vecDir * 32;

		if (UTIL_PointContents(vecCursor) != CONTENTS_SOLID)
		{
			// We passed through the ally!
			SetAbsOrigin(vecCursor);
			return;
		}
		else
		{
			//If a flechette is still within a solid object, we get rid of it
			//This is because its likely not great to have them sticking inside of solid geo
			SetTouch(NULL);
			SetThink(NULL);
			SetContextThink(NULL, 0, s_szTYRFlechetteBubbles);

			UTIL_Remove(this);
		}
	}

	if (pOther->m_takedamage != DAMAGE_NO)
	{
		Vector	vecNormalizedVel = GetAbsVelocity();

		ClearMultiDamage();
		VectorNormalize(vecNormalizedVel);

		float flDamage = sk_flechette_dmg.GetFloat();
		CBreakable* pBreak = dynamic_cast <CBreakable*>(pOther);
		if (pBreak && (pBreak->GetMaterialType() == matGlass))
		{
			flDamage = MAX(pOther->GetHealth(), flDamage);
		}

		CTakeDamageInfo	dmgInfo(this, GetOwnerEntity(), flDamage, DMG_DISSOLVE | DMG_NEVERGIB);
		CalculateMeleeDamageForce(&dmgInfo, vecNormalizedVel, tr.endpos, 0.7f);
		dmgInfo.SetDamagePosition(tr.endpos);
		pOther->DispatchTraceAttack(dmgInfo, vecNormalizedVel, &tr);

		ApplyMultiDamage();

		// Keep going through breakable glass.
		if (pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS)
			return;

		SetAbsVelocity(Vector(0, 0, 0));

		// play body "thwack" sound
		EmitSound("NPC_Hunter.FlechetteHitBody");

		StopParticleEffects(this);

		Vector vForward;
		AngleVectors(GetAbsAngles(), &vForward);
		VectorNormalize(vForward);

		trace_t	tr2;
		UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + vForward * 128, MASK_BLOCKLOS, pOther, COLLISION_GROUP_NONE, &tr2);

		if (tr2.fraction != 1.0f)
		{
			//NDebugOverlay::Box( tr2.endpos, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 255, 0, 0, 10 );
			//NDebugOverlay::Box( GetAbsOrigin(), Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 0, 255, 0, 10 );

			if (tr2.m_pEnt == NULL || (tr2.m_pEnt && tr2.m_pEnt->GetMoveType() == MOVETYPE_NONE))
			{
				CEffectData	data;

				data.m_vOrigin = tr2.endpos;
				data.m_vNormal = vForward;
				data.m_nEntIndex = tr2.fraction != 1.0f;

				//DispatchEffect( "BoltImpact", data );
			}
		}
		if (pOther->IsNPC())
		{
			// We hit an enemy, stick to them!
			StickTo(pOther, tr);
			// Set up our larger explosion!
			m_stuckEnemy = true;
		}
		else if (((pOther->GetMoveType() == MOVETYPE_VPHYSICS) || (pOther->GetMoveType() == MOVETYPE_PUSH)) && ((pOther->GetHealth() > 0) || (pOther->m_takedamage == DAMAGE_EVENTS_ONLY)))
		{
			CPhysicsProp* pProp = dynamic_cast<CPhysicsProp*>(pOther);
			if (pProp)
			{
				pProp->SetInteraction(PROPINTER_PHYSGUN_NOTIFY_CHILDREN);
			}

			// We hit a physics object that survived the impact. Stick to it.
			StickTo(pOther, tr);
		}
		
		else
		{
			SetTouch(NULL);
			SetThink(NULL);
			SetContextThink(NULL, 0, s_szTYRFlechetteBubbles);

			UTIL_Remove(this);
		}
	}
	else
	{
		// See if we struck the world
		if (pOther->GetMoveType() == MOVETYPE_NONE && !(tr.surface.flags & SURF_SKY))
		{
			// We hit a physics object that survived the impact. Stick to it.
			StickTo(pOther, tr);
		}
		else if (pOther->GetMoveType() == MOVETYPE_PUSH && FClassnameIs(pOther, "func_breakable"))
		{
			// We hit a func_breakable, stick to it.
			// The MOVETYPE_PUSH is a micro-optimization to cut down on the classname checks.
			StickTo(pOther, tr);
		}
		else
		{
			// Put a mark unless we've hit the sky
			if ((tr.surface.flags & SURF_SKY) == false)
			{
				UTIL_ImpactTrace(&tr, DMG_BULLET);
			}

			UTIL_Remove(this);
		}
	}
}

void CFlechette::OnParentPhysGunDrop(CBasePlayer* pPhysGunUser, PhysGunDrop_t Reason)
{
	m_bThrownBack = true;
}

void CFlechette::DopplerThink()
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

void CFlechette::BubbleThink()
{
	SetNextThink(gpGlobals->curtime + 0.1f, s_szTYRFlechetteBubbles);

	if (GetWaterLevel() == 0)
		return;

	UTIL_BubbleTrail(GetAbsOrigin() - GetAbsVelocity() * 0.1f, GetAbsOrigin(), 5);
}

void CFlechette::Shoot(Vector& vecVelocity, bool bBrightFX)
{
	CreateSprites(bBrightFX);

	m_vecShootPosition = GetAbsOrigin();

	SetAbsVelocity(vecVelocity);

	SetThink(&CFlechette::DopplerThink);
	SetNextThink(gpGlobals->curtime);

	SetContextThink(&CFlechette::BubbleThink, gpGlobals->curtime + 0.1, s_szTYRFlechetteBubbles);
}

void CFlechette::DangerSoundThink()
{
	EmitSound("NPC_Hunter.FlechettePreExplode");

	CSoundEnt::InsertSound(SOUND_DANGER | SOUND_CONTEXT_EXCLUDE_COMBINE, GetAbsOrigin(), 150.0f, 0.5, this);
	SetThink(&CFlechette::ExplodeThink);
	SetNextThink(gpGlobals->curtime + FLECHETTE_WARN_TIME);
}

void CFlechette::ExplodeThink()
{
	Explode();
}

void CFlechette::Explode()
{
	SetSolid(SOLID_NONE);

	// Don't catch self in own explosion!
	m_takedamage = DAMAGE_NO;

	EmitSound("NPC_Hunter.FlechetteExplode");

	// Move the explosion effect to the tip to reduce intersection with the world.
	Vector vecFuse;
	GetAttachment(s_TYRFlechetteFuseAttach, vecFuse);
	
	float explosionRadius;
	if (m_stuckEnemy == true)
	{
		explosionRadius = sk_flechette_stuck_explode_radius.GetFloat();
		DispatchParticleEffect("hunter_projectile_explosion_3", vecFuse, GetAbsAngles(), NULL);
	}
	else
	{
		explosionRadius = sk_flechette_explode_radius.GetFloat();
		DispatchParticleEffect("hunter_projectile_explosion_1", vecFuse, GetAbsAngles(), NULL);
	}

	int nDamageType = DMG_DISSOLVE;

	// Perf optimization - only every other explosion makes a physics force. This is
	// hardly noticeable since flechettes usually explode in clumps.
	static int s_nExplosionCount = 0;
	s_nExplosionCount++;
	if ((s_nExplosionCount & 0x01) && flechette_cheap_explosions.GetBool())
	{
		nDamageType |= DMG_PREVENT_PHYSICS_FORCE;
	}

	RadiusDamage(CTakeDamageInfo(this, GetOwnerEntity(), sk_flechette_explode_dmg.GetFloat(), nDamageType), GetAbsOrigin(), explosionRadius, CLASS_NONE, NULL);

	AddEffects(EF_NODRAW);

	SetThink(&CBaseEntity::SUB_Remove);
	SetNextThink(gpGlobals->curtime + 0.1f);
}