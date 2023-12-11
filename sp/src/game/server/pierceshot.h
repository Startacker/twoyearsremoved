// DEPRECATED
// Mix of Combine Sniper bullets and Hunter Flechettes
// High damage piercing shot
// DEPRECATED

#ifndef TYR_PIERCESHOT_H
#define TYR_PIERCESHOT_H

#if defined( _WIN32 )
#pragma once
#endif

#include "cbase.h"
#include "physobj.h"
#include "particle_parse.h"
#include "datacache/imdlcache.h"
#include "IEffects.h"
#include "te_effect_dispatch.h"
#include "soundent.h"
#ifdef MAPBASE
#include "mapbase/GlobalStrings.h"
#endif

class CPierceShot : public CPhysicsProp, public IParentPropInteraction
{
public:
	DECLARE_CLASS(CPierceShot, CPhysicsProp);
	CPierceShot();
	~CPierceShot();

public:

	void Spawn();
	void Precache();
	void Shoot(Vector& vecVelocity, bool bBright);
	static void FirePierce();

	bool CreateVPhysics();

	static CPierceShot* FirePierce(const Vector& vecOrigin, const QAngle& angAngles, CBaseEntity* pentOwner = NULL);

protected:

	void DopplerThink();
	void BulletThink();

	void PierceHit(CBaseEntity* pOther);

	Vector m_vecShootPosition;
	Vector	m_vecDir;
	EHANDLE m_hSeekTarget;

	void OnParentCollisionInteraction(parentCollisionInteraction_t eType, int index, gamevcollisionevent_t* pEvent);
	void OnParentPhysGunDrop(CBasePlayer* pPhysGunUser, PhysGunDrop_t Reason);

	DECLARE_DATADESC();
};

#endif