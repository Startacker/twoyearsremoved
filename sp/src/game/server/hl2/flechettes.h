//
// Clones Hunter Flechettes into their own file for ease of access
// Effectively a mix of the SMG grenade and flechettes, more direct projectile, stick on hit, THEN explode
//

#ifndef TYR_FLECHETTE_H
#define TYR_FLECHETTE_H

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

class CFlechette : public CPhysicsProp, public IParentPropInteraction
{
public:
	DECLARE_CLASS(CFlechette, CPhysicsProp);
	CFlechette();
	~CFlechette();

public:

	void Spawn();
	void Activate();
	void Precache();
	void Shoot(Vector& vecVelocity, bool bBright);
	void SetSeekTarget(CBaseEntity* pTargetEntity);
	void Explode();
	static void Shoot_Flechette();

	bool CreateVPhysics();

	unsigned int PhysicsSolidMaskForEntity() const;
	static CFlechette* FlechetteCreate(const Vector& vecOrigin, const QAngle& angAngles, CBaseEntity* pentOwner = NULL);

	// IParentPropInteraction
	void OnParentCollisionInteraction(parentCollisionInteraction_t eType, int index, gamevcollisionevent_t* pEvent);
	void OnParentPhysGunDrop(CBasePlayer* pPhysGunUser, PhysGunDrop_t Reason);

protected:

	void SetupGlobalModelData();

	void StickTo(CBaseEntity* pOther, trace_t& tr);

	void BubbleThink();
	void DangerSoundThink();
	void ExplodeThink();
	void DopplerThink();
	void SeekThink();

	bool m_bThrownBack;
	bool CreateSprites(bool bBright);

	void FlechetteTouch(CBaseEntity* pOther);

	Vector m_vecShootPosition;
	Vector	m_vecDir;
	EHANDLE m_hSeekTarget;

	DECLARE_DATADESC();
	//DECLARE_SERVERCLASS();
};
#endif