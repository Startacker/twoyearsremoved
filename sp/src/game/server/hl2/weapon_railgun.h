//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the grapple hook weapon.
//			
//			Primary attack: fires a beam that hooks on a surface.
//			Secondary attack: switches between pull and rapple modes
//
//
//=============================================================================//

#ifndef WEAPON_GRAPPLE_H
#define WEAPON_GRAPPLE_H
 
#ifdef _WIN32
#pragma once
#endif

#include "basehlcombatweapon.h"                
#include "sprite.h"                            
#include "props.h"                             
 
    #include "rope.h"
    #include "props.h"
 
#include "rope_shared.h"

#include "te_effect_dispatch.h"
#include "beam_shared.h"
 
 
class CWeaponRailgun;
 
//-----------------------------------------------------------------------------
// Grapple Hook
//-----------------------------------------------------------------------------
class CGrappleHook : public CBaseCombatCharacter
{
    DECLARE_CLASS( CGrappleHook, CBaseCombatCharacter );
 
public:
    CGrappleHook() { };
    ~CGrappleHook();
 
    Class_T Classify( void ) { return CLASS_NONE; }
 
public:
    void Spawn( void );
    void Precache( void );
    void FlyThink( void );
    void HookedThink( void );
    void HookTouch( CBaseEntity *pOther );
    bool CreateVPhysics( void );
    unsigned int PhysicsSolidMaskForEntity() const;
    static CGrappleHook *HookCreate( const Vector &vecOrigin, const QAngle &angAngles, CBaseEntity *pentOwner = NULL );
 
protected:
 
    DECLARE_DATADESC();
 
private:
  
    CHandle<CWeaponRailgun>     m_hOwner;
    CHandle<CBasePlayer>        m_hPlayer;
    CHandle<CDynamicProp>       m_hBolt;
    IPhysicsSpring              *m_pSpring;
    float                       m_fSpringLength;
    bool                        m_bPlayerWasStanding;

	
};
 
#endif
 
//-----------------------------------------------------------------------------
// CWeaponRailgun
//-----------------------------------------------------------------------------
      
class CWeaponRailgun : public CBaseHLCombatWeapon            
{                                             
    DECLARE_DATADESC();

      
public:
    DECLARE_CLASS(CWeaponRailgun, CBaseHLCombatWeapon);

    CWeaponRailgun( void );
 
    DECLARE_SERVERCLASS();

    virtual void    Precache( void );
    virtual void    PrimaryAttack( void );
    virtual void    SecondaryAttack( void );
    void	PierceShot(Vector& vecSr, bool FirstShot);
    bool            CanHolster( void );
    virtual bool    Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
    void            Drop( const Vector &vecVelocity );
    virtual bool    Reload( void );
    virtual void    ItemBusyFrame(void);
    virtual void    ItemPostFrame( void );
 
    void            NotifyHookDied( void );
 
    CBaseEntity     *GetHook( void ) { return m_hHook; }

	void   	DrawBeam( const Vector &startPos, const Vector &endPos, float width );
	void	DoImpactEffect( trace_t &tr, int nDamageType );

	bool                        m_bHook;
 
    
    DECLARE_ACTTABLE();

private:

    void    FireHook( void );

    
 
private:

	CHandle<CBeam>        pBeam;
	CHandle<CSprite>	m_pLightGlow;
 
    CNetworkVar( bool,    m_bInZoom );
    CNetworkVar( bool,    m_bMustReload );
 
    CNetworkHandle( CBaseEntity, m_hHook );

	CNetworkVar( int, m_nBulletType );
};