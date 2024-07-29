//
// Implements a railgun mixed with a grappling hook
// <3 Obsidian Conflict & Maestra Fénix
//

#include "cbase.h"
#include "npcevent.h"
#include "in_buttons.h"
#include "weapon_railgun.h"
#include "game.h"                       
#include "player.h"               
#include "ai_basenpc.h"
#include "te_effect_dispatch.h"
#include "basehlcombatweapon.h"
#include "basecombatcharacter.h"
#include "gamerules.h"
#include "IEffects.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "beam_shared.h"
#include "explode.h"
#include "hl2_player.h"

#include "ammodef.h"		/* This is needed for the tracing done later */
#include "gamestats.h" //
#include "soundent.h" //
 
#include "vphysics/constraints.h"
#include "physics_saverestore.h"
#include "rumble_shared.h"
 
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
 
#define HOOK_MODEL			"models/props_junk/rock001a.mdl"

#define BOLT_AIR_VELOCITY	3500
#define BOLT_WATER_VELOCITY	1500
 
LINK_ENTITY_TO_CLASS( grapple_hook, CGrappleHook );
 
BEGIN_DATADESC( CGrappleHook )
	// Function Pointers
	DEFINE_THINKFUNC( FlyThink ),
	DEFINE_THINKFUNC( HookedThink ),
	DEFINE_FUNCTION( HookTouch ),

	DEFINE_FIELD( m_hPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hOwner, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hBolt, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bPlayerWasStanding, FIELD_BOOLEAN ),
 
END_DATADESC()
 
CGrappleHook *CGrappleHook::HookCreate( const Vector &vecOrigin, const QAngle &angAngles, CBaseEntity *pentOwner )
{
	// Create a new entity with CGrappleHook private data
	CGrappleHook *pHook = (CGrappleHook *)CreateEntityByName( "grapple_hook" );
	UTIL_SetOrigin( pHook, vecOrigin );
	pHook->SetAbsAngles( angAngles );
	pHook->Spawn();
 
	CWeaponRailgun *pOwner = (CWeaponRailgun *)pentOwner;
	pHook->m_hOwner = pOwner;
	pHook->SetOwnerEntity( pOwner->GetOwner() );
	pHook->m_hPlayer = (CBasePlayer *)pOwner->GetOwner();
 
	return pHook;
}
 
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGrappleHook::~CGrappleHook( void )
{ 
	if ( m_hBolt )
	{
		UTIL_Remove( m_hBolt );
		m_hBolt = NULL;
	}
 
	// Revert Jay's gai flag
	if ( m_hPlayer )
		m_hPlayer->SetPhysicsFlag( PFLAG_VPHYSICS_MOTIONCONTROLLER, false );
}
 
//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGrappleHook::CreateVPhysics( void )
{
	// Create the object in the physics system
	VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false );
 
	return true;
}
 
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
unsigned int CGrappleHook::PhysicsSolidMaskForEntity() const
{
	return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}
 
//-----------------------------------------------------------------------------
// Purpose: Spawn
//-----------------------------------------------------------------------------
void CGrappleHook::Spawn( void )
{
	Precache( );
 
	SetModel( HOOK_MODEL );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	UTIL_SetSize( this, -Vector(1,1,1), Vector(1,1,1) );
	SetSolid( SOLID_BBOX );
	SetGravity( 0.05f );
 
	// The rock is invisible, the crossbow bolt is the visual representation
	AddEffects( EF_NODRAW );
 
	// Make sure we're updated if we're underwater
	UpdateWaterState();
 
	SetTouch( &CGrappleHook::HookTouch );
 
	SetThink( &CGrappleHook::FlyThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
 
	m_pSpring		= NULL;
	m_fSpringLength = 0.0f;
	m_bPlayerWasStanding = false;
}
 
 
void CGrappleHook::Precache( void )
{
	PrecacheModel( HOOK_MODEL );
}
 
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CGrappleHook::HookTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS) )
		return;
 
	if ( (pOther != m_hOwner) && (pOther->m_takedamage != DAMAGE_NO) )
	{
		m_hOwner->NotifyHookDied();
 
		SetTouch( NULL );
		SetThink( NULL );
 
		UTIL_Remove( this );
	}
	else
	{
		trace_t	tr;
		tr = BaseClass::GetTouchTrace();
 
		// See if we struck the world
		if ( pOther->GetMoveType() == MOVETYPE_NONE && !( tr.surface.flags & SURF_SKY ) )
		{
			EmitSound( "Weapon_AR2.Reload_Push" );
 
			// if what we hit is static architecture, can stay around for a while.
			Vector vecDir = GetAbsVelocity();
 
			//FIXME: We actually want to stick (with hierarchy) to what we've hit
			SetMoveType( MOVETYPE_NONE );
 
			Vector vForward;
 
			AngleVectors( GetAbsAngles(), &vForward );
			VectorNormalize ( vForward );
 
			CEffectData	data;
 
			data.m_vOrigin = tr.endpos;
			data.m_vNormal = vForward;
			data.m_nEntIndex = 0;
 
		//	DispatchEffect( "Impact", data );
 
		//	AddEffects( EF_NODRAW );
			SetTouch( NULL );

			VPhysicsDestroyObject();
			VPhysicsInitNormal( SOLID_VPHYSICS, FSOLID_NOT_STANDABLE, false );
			AddSolidFlags( FSOLID_NOT_SOLID );
		//	SetMoveType( MOVETYPE_NONE );
 
			if ( !m_hPlayer )
			{
				Assert( 0 );
				return;
			}
 
			// Set Jay's gai flag
			m_hPlayer->SetPhysicsFlag( PFLAG_VPHYSICS_MOTIONCONTROLLER, true );
 
			//IPhysicsObject *pPhysObject = m_hPlayer->VPhysicsGetObject();
			IPhysicsObject *pRootPhysObject = VPhysicsGetObject();
			Assert( pRootPhysObject );
			Assert( pPhysObject );
 
			pRootPhysObject->EnableMotion( false );
 
			// Root has huge mass, tip has little
			pRootPhysObject->SetMass( VPHYSICS_MAX_MASS );
		//	pPhysObject->SetMass( 100 );
		//	float damping = 3;
		//	pPhysObject->SetDamping( &damping, &damping );
 
			Vector origin = m_hPlayer->GetAbsOrigin();
			Vector rootOrigin = GetAbsOrigin();
			m_fSpringLength = (origin - rootOrigin).Length();
 
			m_bPlayerWasStanding = ( ( m_hPlayer->GetFlags() & FL_DUCKING ) == 0 );
 
			SetThink( &CGrappleHook::HookedThink );
			SetNextThink( gpGlobals->curtime + 0.1f );
		}
		else
		{
			// Put a mark unless we've hit the sky
			if ( ( tr.surface.flags & SURF_SKY ) == false )
			{
				UTIL_ImpactTrace( &tr, DMG_BULLET );
			}
 
			SetTouch( NULL );
			SetThink( NULL );
 
			m_hOwner->NotifyHookDied();
			UTIL_Remove( this );
		}
	}
}
 
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrappleHook::HookedThink( void )
{
	//set next globalthink
	SetNextThink( gpGlobals->curtime + 0.05f ); //0.1f

	//All of this push the player far from the hook
	Vector tempVec1 = m_hPlayer->GetAbsOrigin() - GetAbsOrigin();
	VectorNormalize(tempVec1);

	int temp_multiplier = -1;

	m_hPlayer->SetGravity(0.0f);
	m_hPlayer->SetGroundEntity(NULL);

	if (m_hOwner->m_bHook){
		temp_multiplier = 1;
		m_hPlayer->SetAbsVelocity(tempVec1*temp_multiplier*750);//400
	}
	else
	{
		m_hPlayer->SetAbsVelocity(tempVec1*temp_multiplier*750);//400
	}
}
 
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrappleHook::FlyThink( void )
{
	QAngle angNewAngles;
 
	VectorAngles( GetAbsVelocity(), angNewAngles );
	SetAbsAngles( angNewAngles );
 
	SetNextThink( gpGlobals->curtime + 0.1f );
}
 
LINK_ENTITY_TO_CLASS( weapon_railgun, CWeaponRailgun );
 
PRECACHE_WEAPON_REGISTER( weapon_railgun );

IMPLEMENT_SERVERCLASS_ST(CWeaponRailgun, DT_WeaponRailgun)
END_SEND_TABLE()
 
BEGIN_DATADESC(CWeaponRailgun)
END_DATADESC()
 
acttable_t	CWeaponRailgun::m_acttable[] = 
{
#if AR2_ACTIVITY_FIX == 1
	{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_AR2,			true },
	{ ACT_RELOAD,					ACT_RELOAD_AR2,				true },
	{ ACT_IDLE,						ACT_IDLE_AR2,					true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_AR2,			false },

	{ ACT_WALK,						ACT_WALK_AR2,					true },

	// Readiness activities (not aiming)
		{ ACT_IDLE_RELAXED,				ACT_IDLE_AR2_RELAXED,			false },//never aims
		{ ACT_IDLE_STIMULATED,			ACT_IDLE_AR2_STIMULATED,		false },
		{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_AR2,			false },//always aims

		{ ACT_WALK_RELAXED,				ACT_WALK_AR2_RELAXED,			false },//never aims
		{ ACT_WALK_STIMULATED,			ACT_WALK_AR2_STIMULATED,		false },
		{ ACT_WALK_AGITATED,			ACT_WALK_AIM_AR2,				false },//always aims

		{ ACT_RUN_RELAXED,				ACT_RUN_AR2_RELAXED,			false },//never aims
		{ ACT_RUN_STIMULATED,			ACT_RUN_AR2_STIMULATED,		false },
		{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

		// Readiness activities (aiming)
		{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_AR2_RELAXED,			false },//never aims	
		{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_AR2_STIMULATED,	false },
		{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_AR2,			false },//always aims

		{ ACT_WALK_AIM_RELAXED,			ACT_WALK_AR2_RELAXED,			false },//never aims
		{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_AR2_STIMULATED,	false },
		{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_AR2,				false },//always aims

		{ ACT_RUN_AIM_RELAXED,			ACT_RUN_AR2_RELAXED,			false },//never aims
		{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_AR2_STIMULATED,	false },
		{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
		//End readiness activities

				{ ACT_WALK_AIM,					ACT_WALK_AIM_AR2,				true },
				{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
				{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
				{ ACT_RUN,						ACT_RUN_AR2,					true },
				{ ACT_RUN_AIM,					ACT_RUN_AIM_AR2,				true },
				{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
				{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
				{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_AR2,	false },
				{ ACT_COVER_LOW,				ACT_COVER_AR2_LOW,				true },
				{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_AR2_LOW,			false },
				{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_AR2_LOW,		false },
				{ ACT_RELOAD_LOW,				ACT_RELOAD_AR2_LOW,			false },
				{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_AR2,		true },
				//	{ ACT_RANGE_ATTACK2, ACT_RANGE_ATTACK_AR2_GRENADE, true },
				#else
					{ ACT_RANGE_ATTACK1,			ACT_RANGE_ATTACK_AR2,			true },
					{ ACT_RELOAD,					ACT_RELOAD_SMG1,				true },		// FIXME: hook to AR2 unique
					{ ACT_IDLE,						ACT_IDLE_SMG1,					true },		// FIXME: hook to AR2 unique
					{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_SMG1,			true },		// FIXME: hook to AR2 unique

					{ ACT_WALK,						ACT_WALK_RIFLE,					true },

					// Readiness activities (not aiming)
						{ ACT_IDLE_RELAXED,				ACT_IDLE_SMG1_RELAXED,			false },//never aims
						{ ACT_IDLE_STIMULATED,			ACT_IDLE_SMG1_STIMULATED,		false },
						{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_SMG1,			false },//always aims

						{ ACT_WALK_RELAXED,				ACT_WALK_RIFLE_RELAXED,			false },//never aims
						{ ACT_WALK_STIMULATED,			ACT_WALK_RIFLE_STIMULATED,		false },
						{ ACT_WALK_AGITATED,			ACT_WALK_AIM_RIFLE,				false },//always aims

						{ ACT_RUN_RELAXED,				ACT_RUN_RIFLE_RELAXED,			false },//never aims
						{ ACT_RUN_STIMULATED,			ACT_RUN_RIFLE_STIMULATED,		false },
						{ ACT_RUN_AGITATED,				ACT_RUN_AIM_RIFLE,				false },//always aims

						// Readiness activities (aiming)
							{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_SMG1_RELAXED,			false },//never aims	
							{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_RIFLE_STIMULATED,	false },
							{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_SMG1,			false },//always aims

							{ ACT_WALK_AIM_RELAXED,			ACT_WALK_RIFLE_RELAXED,			false },//never aims
							{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_RIFLE_STIMULATED,	false },
							{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_RIFLE,				false },//always aims

							{ ACT_RUN_AIM_RELAXED,			ACT_RUN_RIFLE_RELAXED,			false },//never aims
							{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_RIFLE_STIMULATED,	false },
							{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_RIFLE,				false },//always aims
							//End readiness activities

								{ ACT_WALK_AIM,					ACT_WALK_AIM_RIFLE,				true },
								{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,			true },
								{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,		true },
								{ ACT_RUN,						ACT_RUN_RIFLE,					true },
								{ ACT_RUN_AIM,					ACT_RUN_AIM_RIFLE,				true },
								{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,			true },
								{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,		true },
								{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_AR2,	false },
								{ ACT_COVER_LOW,				ACT_COVER_SMG1_LOW,				false },		// FIXME: hook to AR2 unique
								{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_AR2_LOW,			false },
								{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_SMG1_LOW,		true },		// FIXME: hook to AR2 unique
								{ ACT_RELOAD_LOW,				ACT_RELOAD_SMG1_LOW,			false },
								{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_SMG1,		true },
								//	{ ACT_RANGE_ATTACK2, ACT_RANGE_ATTACK_AR2_GRENADE, true },
								#endif

								#if EXPANDED_HL2_WEAPON_ACTIVITIES
									{ ACT_ARM,						ACT_ARM_RIFLE,					false },
									{ ACT_DISARM,					ACT_DISARM_RIFLE,				false },
								#endif

								#if EXPANDED_HL2_COVER_ACTIVITIES
									{ ACT_RANGE_AIM_MED,			ACT_RANGE_AIM_AR2_MED,			false },
									{ ACT_RANGE_ATTACK1_MED,		ACT_RANGE_ATTACK_AR2_MED,		false },

									{ ACT_COVER_WALL_R,				ACT_COVER_WALL_R_RIFLE,			false },
									{ ACT_COVER_WALL_L,				ACT_COVER_WALL_L_RIFLE,			false },
									{ ACT_COVER_WALL_LOW_R,			ACT_COVER_WALL_LOW_R_RIFLE,		false },
									{ ACT_COVER_WALL_LOW_L,			ACT_COVER_WALL_LOW_L_RIFLE,		false },
								#endif

								#ifdef MAPBASE
									// HL2:DM activities (for third-person animations in SP)
									{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_AR2,                    false },
									{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_AR2,                    false },
									{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_AR2,            false },
									{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_AR2,            false },
									{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_AR2,    false },
									{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_AR2,        false },
									{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_AR2,                    false },
								#if EXPANDED_HL2DM_ACTIVITIES
									{ ACT_HL2MP_WALK,					ACT_HL2MP_WALK_AR2,						false },
									{ ACT_HL2MP_GESTURE_RANGE_ATTACK2,	ACT_HL2MP_GESTURE_RANGE_ATTACK2_AR2,	false },
								#endif
								#endif
};
 
IMPLEMENT_ACTTABLE(CWeaponRailgun);
 
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponRailgun::CWeaponRailgun( void )
{
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= true;
	m_bInZoom			= false;
	m_bMustReload		= false;

	
	m_pLightGlow= NULL;

	pBeam	= NULL;
}
  
//-----------------------------------------------------------------------------
// Purpose: Precache
//-----------------------------------------------------------------------------
void CWeaponRailgun::Precache( void )
{
	UTIL_PrecacheOther( "grapple_hook" );
 
//	PrecacheScriptSound( "Weapon_Crossbow.BoltHitBody" );
//	PrecacheScriptSound( "Weapon_Crossbow.BoltHitWorld" );
//	PrecacheScriptSound( "Weapon_Crossbow.BoltSkewer" );
 
	PrecacheModel( "sprites/physbeam.vmt" );
	PrecacheModel( "sprites/physcannon_bluecore2b.vmt" );
	PrecacheParticleSystem("piercer_tracer");
 
	BaseClass::Precache();
}
 
//-----------------------------------------------------------------------------
// Purpose: FIRE!!!!!
//-----------------------------------------------------------------------------
void CWeaponRailgun::PrimaryAttack(void)
{
	CBasePlayer* pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	if (m_iClip1 <= 0)
	{
		if (!m_bFireOnEmpty)
		{
			Reload();
		}
		else
		{
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + GetViewModelSequenceDuration();
	m_flNextSecondaryAttack = gpGlobals->curtime + GetViewModelSequenceDuration();

	WeaponSound(SINGLE);
	pOwner->DoMuzzleFlash();
	pOwner->SetMuzzleFlashTime(gpGlobals->curtime + 0.5);

	// Fire the bullets
	Vector	vecSrcOwner = pOwner->Weapon_ShootPosition();
	PierceShot(vecSrcOwner, true);
	pOwner->RumbleEffect(RUMBLE_SHOTGUN_DOUBLE, 0, RUMBLE_FLAG_RESTART);

	// View effects
	color32 white = { 255, 255, 255, 64 };
	UTIL_ScreenFade(pOwner, white, 0.1, 0, FFADE_IN);

	pOwner->ViewPunch(QAngle(random->RandomInt(-2, -6), random->RandomInt(1, 2), 0));

	m_iClip1--;
	m_iPrimaryAttacks++;
	gamestats->Event_WeaponFired(pOwner, false, GetClassname());
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponRailgun::SecondaryAttack( void )
{
	// Can't have an active hook out
	if ( m_hHook != NULL )
		return;
 
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( !pPlayer )
	{
		return;
	}

	if ( m_iClip1 <= 0 )
	{
		if ( !m_bFireOnEmpty )
		{
			Reload();
		}
		else
		{
			WeaponSound( EMPTY );
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	m_iClip1--;

	WeaponSound( WPN_DOUBLE );
	pPlayer->DoMuzzleFlash();

	SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK2 );

	m_flNextPrimaryAttack = gpGlobals->curtime + GetViewModelSequenceDuration();
	m_flNextSecondaryAttack = gpGlobals->curtime + GetViewModelSequenceDuration();

	Vector vecSrc		= pPlayer->Weapon_ShootPosition();
	Vector vecAiming	= pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );	

	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 600, 0.2, GetOwner() );

	if ( !m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 ); 
	}

	trace_t tr;
	Vector vecShootOrigin, vecShootDir, vecDir, vecEnd;

	//Gets the direction where the player is aiming
	AngleVectors (pPlayer->EyeAngles(), &vecDir);

	//Gets the position of the player
	vecShootOrigin = pPlayer->Weapon_ShootPosition();

	//Gets the position where the hook will hit
	vecEnd	= vecShootOrigin + (vecDir * MAX_TRACE_LENGTH);	
	
	//Traces a line between the two vectors
	UTIL_TraceLine( vecShootOrigin, vecEnd, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr);

	//Draws the beam
	DrawBeam( vecShootOrigin, tr.endpos, 15.5 );

	//Creates an energy impact effect if we don't hit the sky or other places
	if ( (tr.surface.flags & SURF_SKY) == false )
	{
		CPVSFilter filter( tr.endpos );
		te->GaussExplosion( filter, 0.0f, tr.endpos, tr.plane.normal, 0 );

		//Makes a sprite at the end of the beam
		m_pLightGlow = CSprite::SpriteCreate( "sprites/physcannon_bluecore2b.vmt", GetAbsOrigin(), TRUE);

		//Sets FX render and color
		m_pLightGlow->SetTransparency( 9, 255, 255, 255, 200, kRenderFxNoDissipation );
		
		//Sets the position
		m_pLightGlow->SetAbsOrigin(tr.endpos);
		
		//Bright
		m_pLightGlow->SetBrightness( 255 );
		
		//Scale
		m_pLightGlow->SetScale( 0.65 );
	}

	FireHook();
 
	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration( ACT_VM_SECONDARYATTACK ) );
	m_iSecondaryAttacks++;
	gamestats->Event_WeaponFired(pPlayer, false, GetClassname());
}

void CWeaponRailgun::PierceShot(Vector& vecSrc, bool FirstShot)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer == NULL)
		return;

	Vector	vecAiming = pPlayer->GetAutoaimVector(AUTOAIM_SCALE_DEFAULT);
	Vector	vecLooking = pPlayer->Weapon_ShootPosition();
	int iAttachment = GetTracerAttachment();

	if (FirstShot == true)
	{
		pPlayer->FireBullets(1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0, -1, -1, 120, NULL, true, false);

		Vector	vecAimingEnd = vecLooking + (vecAiming * MAX_TRACE_LENGTH);

		trace_t tr;
		UTIL_TraceLine(vecLooking, vecAimingEnd, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr);

		UTIL_ParticleTracer("piercer_tracer", vecLooking, tr.endpos, entindex(), iAttachment, true);

		if (tr.m_pEnt)
		{
			if (tr.m_pEnt->IsNPC() || tr.m_pEnt->VPhysicsGetObject() || tr.m_pEnt->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS || (tr.surface.flags & SURF_SKY) == false)
			{
				Vector vecCursor = tr.endpos;
				vecCursor += vecAiming * 4;

				if (UTIL_PointContents(vecCursor) != CONTENTS_SOLID)
				{
					// Passed through the entity
					PierceShot(vecCursor, false);
				}
				else
				{
					return;
				}
			}
			else
			{
				return;

			}
		}
		else
		{
			return;
		}
	}
	else
	{
		Vector	vecAimingEnd = vecLooking + (vecAiming * MAX_TRACE_LENGTH);
		trace_t tr2;
		UTIL_ParticleTracer("piercer_tracer", vecSrc, tr2.endpos, entindex(), iAttachment, true);
		UTIL_TraceLine(vecSrc, vecAimingEnd, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr2);
		pPlayer->FireBullets(1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 0, -1, -1, 120, NULL, true, false);

		if (tr2.m_pEnt)
		{
			if (tr2.m_pEnt->IsNPC() || tr2.m_pEnt->VPhysicsGetObject() || tr2.m_pEnt->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS)
			{
				Vector vecCursor = tr2.endpos;
				vecCursor += vecAiming * 32;

				if (UTIL_PointContents(vecCursor) != CONTENTS_SOLID)
				{
					// Passed through the entity
					PierceShot(vecCursor, false);
				}
				else
				{
					return;
				}
			}
			else
			{
				return;
			}
		}
		else
		{
			return;
		}
	}
}
 
//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponRailgun::Reload( void )
{
	//Can't reload if we're grappling
	if (m_hHook != NULL)
		return false;
	
	bool fRet = DefaultReload(GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD);
	if (fRet)
	{
		WeaponSound(RELOAD);
		m_bMustReload = false;
	}
	return fRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponRailgun::ItemBusyFrame(void)
{
	BaseClass::ItemBusyFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponRailgun::ItemPostFrame( void )
{
	//Enforces being able to use PrimaryAttack and Secondary Attack
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
 
	if ( ( pOwner->m_nButtons & IN_ATTACK ) )
	{
		if ( m_flNextPrimaryAttack < gpGlobals->curtime )
		{
			PrimaryAttack();
		}
	}

	if ( ( pOwner->m_afButtonPressed & IN_ATTACK2 ) )
	{
		if ( m_flNextPrimaryAttack < gpGlobals->curtime )
		{
			SecondaryAttack();
		}
	}
	if ((pOwner->m_nButtons & IN_RELOAD))
	{
		if (!m_hHook)
			Reload();
	}

	if ( m_hHook )
	{
		if ( !(pOwner->m_nButtons & IN_ATTACK2) || (pOwner->m_nButtons & IN_SPEED))
		{
			m_hHook->SetTouch( NULL );
			m_hHook->SetThink( NULL );
 
			UTIL_Remove( m_hHook );
			m_hHook = NULL;
 
			NotifyHookDied();
 
			m_bMustReload = true;
		}
	}
	BaseClass::ItemPostFrame();
}
 
//-----------------------------------------------------------------------------
// Purpose: Fires the hook
//-----------------------------------------------------------------------------
void CWeaponRailgun::FireHook( void )
{
	if ( m_bMustReload )
		return;
 
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
 
	if ( pOwner == NULL )
		return;
 
	Vector vecAiming	= pOwner->GetAutoaimVector( 0 );	
	Vector vecSrc		= pOwner->Weapon_ShootPosition();
 
	QAngle angAiming;
	VectorAngles( vecAiming, angAiming );
 
	CGrappleHook *pHook = CGrappleHook::HookCreate( vecSrc, angAiming, this );
 
	if ( pOwner->GetWaterLevel() == 3 )
	{
		pHook->SetAbsVelocity( vecAiming * BOLT_WATER_VELOCITY );
	}
	else
	{
		pHook->SetAbsVelocity( vecAiming * BOLT_AIR_VELOCITY );
	}
 
	m_hHook = pHook;
 
	pOwner->ViewPunch( QAngle( -2, 0, 0 ) );
 
	//WeaponSound( SINGLE );
	//WeaponSound( SPECIAL2 );
 
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
 
	m_flNextPrimaryAttack = m_flNextSecondaryAttack	= gpGlobals->curtime + 0.75;
}
 
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSwitchingTo - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponRailgun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( m_hHook )
	{
		m_hHook->SetTouch( NULL );
		m_hHook->SetThink( NULL );
 
		UTIL_Remove( m_hHook );
		m_hHook = NULL;
 
		NotifyHookDied();
 
		m_bMustReload = true;
	}
 
	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponRailgun::Drop( const Vector &vecVelocity )
{
	if ( m_hHook )
	{
		m_hHook->SetTouch( NULL );
		m_hHook->SetThink( NULL );
 
		UTIL_Remove( m_hHook );
		m_hHook = NULL;
 
		NotifyHookDied();
 
		m_bMustReload = true;
	}
 
	BaseClass::Drop( vecVelocity );
}
 
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponRailgun::CanHolster( void )
{
	//Can't have an active hook out
	if ( m_hHook != NULL )
		return false;
 
	return BaseClass::CanHolster();
}
 
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponRailgun::NotifyHookDied( void )
{
	m_hHook = NULL;
 
	if ( pBeam )
	{
		UTIL_Remove( pBeam ); //Kill beam
		pBeam = NULL;

		UTIL_Remove( m_pLightGlow ); //Kill sprite
		m_pLightGlow = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws a beam
// Input  : &startPos - where the beam should begin
//          &endPos - where the beam should end
//          width - what the diameter of the beam should be (units?)
//-----------------------------------------------------------------------------
void CWeaponRailgun::DrawBeam( const Vector &startPos, const Vector &endPos, float width )
{
	//Tracer down the middle (NOT NEEDED, IT WILL FIRE A TRACER)
	//UTIL_Tracer( startPos, endPos, 0, TRACER_DONT_USE_ATTACHMENT, 6500, false, "GaussTracer" );
 
	trace_t tr;
	//Draw the main beam shaft
	pBeam = CBeam::BeamCreate( "sprites/laserbeam.vmt", 2.5 );

	// It starts at startPos
	pBeam->SetStartPos( startPos );
 
	// This sets up some things that the beam uses to figure out where
	// it should start and end
	pBeam->PointEntInit( endPos, this );
 
	// This makes it so that the beam appears to come from the muzzle of the pistol
	pBeam->SetEndAttachment( LookupAttachment("Muzzle") );
	pBeam->SetWidth( width );
//	pBeam->SetEndWidth( 0.05f );
 
	// Higher brightness means less transparent
	pBeam->SetBrightness( 255 );
	//pBeam->SetColor( 255, 185+random->RandomInt( -16, 16 ), 40 );
	pBeam->RelinkBeam();

	//Sets scrollrate of the beam sprite 
	float scrollOffset = gpGlobals->curtime + 5.5;
	pBeam->SetScrollRate(scrollOffset);
 
	// The beam should only exist for a very short time
	//pBeam->LiveForTime( 0.1f );

	UpdateWaterState();
 
	SetTouch( &CGrappleHook::HookTouch );
 
	SetThink( &CGrappleHook::FlyThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &tr - used to figure out where to do the effect
//          nDamageType - ???
//-----------------------------------------------------------------------------
void CWeaponRailgun::DoImpactEffect( trace_t &tr, int nDamageType )
{
	if ( (tr.surface.flags & SURF_SKY) == false )
	{
		CPVSFilter filter( tr.endpos );
		te->GaussExplosion( filter, 0.0f, tr.endpos, tr.plane.normal, 0 );
	}
}