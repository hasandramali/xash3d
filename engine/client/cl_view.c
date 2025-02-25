/*
cl_view.c - player rendering positioning
Copyright (C) 2009 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef XASH_DEDICATED

#include "common.h"
#include "client.h"
#include "const.h"
#include "entity_types.h"
#include "gl_local.h"
#include "vgui_draw.h"

#include "touch.h" // IN_TouchDraw( )
#include "joyinput.h" // Joy_DrawOnScreenKeyboard( )

/*
===============
V_CalcViewRect
calc frame rectangle (Quake1 style)
===============
*/
void V_CalcViewRect( void )
{
	int	size, sb_lines;
	if( scr_viewsize->integer >= 120 )
		sb_lines = 0;		// no status bar at all
	else if( scr_viewsize->integer >= 110 )
		sb_lines = 24;		// no inventory
	else sb_lines = 48;
	size = min( scr_viewsize->integer, 100 );
	cl.refdef.viewport[2] = scr_width->integer * size / 100;
	cl.refdef.viewport[3] = scr_height->integer * size / 100;
	if( cl.refdef.viewport[3] > scr_height->integer - sb_lines )
		cl.refdef.viewport[3] = scr_height->integer - sb_lines;
	if( cl.refdef.viewport[3] > scr_height->integer )
		cl.refdef.viewport[3] = scr_height->integer;
	cl.refdef.viewport[0] = ( scr_width->integer - cl.refdef.viewport[2] ) / 2;
	cl.refdef.viewport[1] = ( scr_height->integer - sb_lines - cl.refdef.viewport[3] ) / 2;
}

/*
===============
V_SetupRefDef

update refdef values each frame
===============
*/
void V_SetupRefDef( void )
{
	cl_entity_t	*clent;

	// compute viewport rectangle
	V_CalcViewRect( );

	clent = CL_GetLocalPlayer ();

	clgame.entities->curstate.scale = clgame.movevars.waveHeight;
	clgame.viewent.curstate.modelindex = cl.predicted.viewmodel;
	clgame.viewent.model = Mod_Handle( clgame.viewent.curstate.modelindex );
	clgame.viewent.curstate.number = cl.playernum + 1;
	clgame.viewent.curstate.entityType = ET_NORMAL;
	clgame.viewent.index = cl.playernum + 1;

	cl.refdef.movevars = &clgame.movevars;
	cl.refdef.health = cl.frame.client.health;
	cl.refdef.playernum = cl.playernum;
	cl.refdef.max_entities = clgame.maxEntities;
	cl.refdef.maxclients = cl.maxclients;
	cl.refdef.time = cl.time;
	cl.refdef.frametime = host.frametime;
	//cl.refdef.frametime = cl.time - cl.oldtime;
	cl.refdef.demoplayback = cls.demoplayback;
	cl.refdef.smoothing = 0;
	cl.refdef.viewsize = scr_viewsize->integer;
	cl.refdef.onlyClientDraw = 0;	// reset clientdraw
	cl.refdef.hardware = true;	// always true
	cl.refdef.spectator = (clent->curstate.spectator != 0);
	cl.scr_fov = bound( 10.0f, cl.scr_fov, 179.0f );
	cl.refdef.nextView = 0;

	SCR_AddDirtyPoint( 0, 0 );
	SCR_AddDirtyPoint( scr_width->integer - 1, scr_height->integer - 1 );

	// calc FOV
	cl.refdef.fov_x = cl.scr_fov; // this is a final fov value
	cl.refdef.fov_y = V_CalcFov( &cl.refdef.fov_x, cl.refdef.viewport[2], cl.refdef.viewport[3] );

	// adjust FOV for widescreen
	if( glState.wideScreen && r_adjust_fov->integer )
		V_AdjustFov( &cl.refdef.fov_x, &cl.refdef.fov_y, cl.refdef.viewport[2], cl.refdef.viewport[3], false );

	if( CL_IsPredicted( ) && !cl.refdef.demoplayback )
	{
		//VectorMA( cl.predicted.origin, cl.lerpBack, cl.predicted.error, cl.predicted.origin );
		VectorCopy( cl.predicted.origin, cl.refdef.simorg );
		VectorCopy( cl.predicted.velocity, cl.refdef.simvel );
		VectorCopy( cl.predicted.viewofs, cl.refdef.viewheight );
		VectorCopy( cl.predicted.punchangle, cl.refdef.punchangle );
		cl.refdef.onground   = cl.predicted.onground != -1;
		cl.refdef.waterlevel = cl.predicted.waterlevel;
	}
	else
	{
		VectorCopy( cl.frame.client.origin, cl.refdef.simorg );
		VectorCopy( cl.frame.client.view_ofs, cl.refdef.viewheight );
		VectorCopy( cl.frame.client.velocity, cl.refdef.simvel );
		VectorCopy( cl.frame.client.punchangle, cl.refdef.punchangle );
		cl.refdef.onground   = cl.frame.client.flags & FL_ONGROUND ? 1 : 0;
		cl.refdef.waterlevel = cl.frame.client.waterlevel;
	}
}

/*
===============
V_SetupOverviewState

Get initial overview values
===============
*/
void V_SetupOverviewState( void )
{
	ref_overview_t	*ov = &clgame.overView;
	float		mapAspect, screenAspect, aspect;

	ov->rotated = ( world.size[1] <= world.size[0] ) ? true : false;

	// calculate nearest aspect
	mapAspect = world.size[!ov->rotated] / world.size[ov->rotated];
	screenAspect = (float)glState.width / (float)glState.height;
	aspect = max( mapAspect, screenAspect );

	ov->zNear = world.maxs[2];
	ov->zFar = world.mins[2];
	ov->flZoom = ( 8192.0f / world.size[ov->rotated] ) / aspect;

	VectorAverage( world.mins, world.maxs, ov->origin );
}

/*
===============
V_WriteOverviewScript

Create overview scrip file
===============
*/
void V_WriteOverviewScript( void )
{
	ref_overview_t	*ov = &clgame.overView;
	string		filename;
	file_t		*f;

	Q_snprintf( filename, sizeof( filename ), "overviews/%s.txt", clgame.mapname );

	f = FS_Open( filename, "w", false );
	if( !f ) return;

	FS_Printf( f, "// overview description file for %s.bsp\n\n", clgame.mapname );
	FS_Print( f, "global\n{\n" );
	FS_Printf( f, "\tZOOM\t%.2f\n", ov->flZoom );
	FS_Printf( f, "\tORIGIN\t%.f\t%.f\t%.f\n", ov->origin[0], ov->origin[1], ov->zFar + 1 );
	FS_Printf( f, "\tROTATED\t%i\n", ov->rotated ? 1 : 0 );
	FS_Print( f, "}\n\nlayer\n{\n" );
	FS_Printf( f, "\tIMAGE\t\"overviews/%s.bmp\"\n", clgame.mapname );
	FS_Printf( f, "\tHEIGHT\t%.f\n", ov->zFar + 1 );	// ???
	FS_Print( f, "}\n" );

	FS_Close( f );
}

/*
===============
V_MergeOverviewRefdef

merge refdef with overview settings
===============
*/
void V_MergeOverviewRefdef( ref_params_t *fd )
{
	ref_overview_t	*ov = &clgame.overView;
	float		aspect;
	float		size_x, size_y;
	vec2_t		mins, maxs;

	if( !gl_overview->integer ) return;

	// NOTE: Xash3D may use 16:9 or 16:10 aspects
	aspect = (float)glState.width / (float)glState.height;

	size_x = fabs( 8192.0f / ov->flZoom );
	size_y = fabs( 8192.0f / (ov->flZoom * aspect ));

	// compute rectangle
	ov->xLeft = -(size_x / 2);
	ov->xRight = (size_x / 2);
	ov->xTop = -(size_y / 2);
	ov->xBottom = (size_y / 2);

	if( gl_overview->integer == 1 )
	{
		Con_NPrintf( 0, " Overview: Zoom %.2f, Map Origin (%.2f, %.2f, %.2f), Z Min %.2f, Z Max %.2f, Rotated %i\n",
		ov->flZoom, ov->origin[0], ov->origin[1], ov->origin[2], ov->zNear, ov->zFar, ov->rotated );
	}

	VectorCopy( ov->origin, fd->vieworg );
	fd->vieworg[2] = ov->zFar + ov->zNear;
	Vector2Copy( fd->vieworg, mins );
	Vector2Copy( fd->vieworg, maxs );

	mins[!ov->rotated] += ov->xLeft;
	maxs[!ov->rotated] += ov->xRight;
	mins[ov->rotated] += ov->xTop;
	maxs[ov->rotated] += ov->xBottom;

	fd->viewangles[0] = 90.0f;
	fd->viewangles[1] = 90.0f;
	fd->viewangles[2] = (ov->rotated) ? (ov->flZoom < 0.0f) ? 180.0f : 0.0f : (ov->flZoom < 0.0f) ? -90.0f : 90.0f;

	Mod_SetOrthoBounds( mins, maxs );
}

typedef struct
{
	qboolean valid;
	vec3_t value;
} lag_angles_t;

#define STEP_MILLIS 10
#define ANGLE_BACKUP_2 128

#define TO_STEP( x ) ( (int)( (x)*STEP_MILLIS ) )
#define FROM_STEP( x ) ( (float)( x ) * ( 1.0f / STEP_MILLIS ) )

static lag_angles_t lag_angles[ANGLE_BACKUP_2];
static int last_step;

static float Map( float value, float low1, float high1, float low2, float high2 )
{
	return low2 + ( value - low1 ) * ( high2 - low2 ) / ( high1 - low1 );
}

void AngleLerp( const vec3_t a, const vec3_t b, float t, vec3_t dst )
{
	/* crappy but it's fast */
	int i;
	float dt;

	for ( i = 0; i < 3; i++ )
	{
		dt = b[i] - a[i];

		if ( dt < -180 )
			dt += 360;
		else if ( dt > 180 )
			dt -= 360;

		dst[i] = a[i] + t * dt;
	}
}

static void AddLagAngles( float time, vec3_t angles )
{
	int step = TO_STEP( time );

	if ( step < last_step )
	{
		// client restart
		memset( lag_angles, 0, sizeof( lag_angles ) );
	}
	else if ( step == last_step )
	{
		// too soon
		return;
	}

	last_step = step;

	VectorCopy( angles, lag_angles[step % ANGLE_BACKUP].value );
	lag_angles[step % ANGLE_BACKUP].valid = true;
}

static qboolean GetLagAngles( float time, vec3_t dest )
{
	int step = TO_STEP( time );

	lag_angles_t *angles = &lag_angles[step % ANGLE_BACKUP];
	if ( !angles->valid )
		return false;

	float step_time = FROM_STEP( TO_STEP( time ) );
	if ( step_time < time )
	{
		lag_angles_t *next_angles = &lag_angles[( step + 1 ) % ANGLE_BACKUP];
		if ( !next_angles->valid )
		{
			// can't lerp to these
			VectorCopy( angles->value, dest );
		}
		else
		{
			float next_time = FROM_STEP( step + 1 );
			float frac      = Map( time, step_time, next_time, 0, 1 );
			AngleLerp( angles->value, next_angles->value, frac, dest );
		}
	}
	else
	{
		// if round_time > time, the difference is
		// probably so small that it can't be noticed
		VectorCopy( angles->value, dest );
	}

	return true;
}

inline static void VectorMA_KopaFish( const vec3_t v, float s, vec3_t dst )
{
	dst[0] += v[0] * s;
	dst[1] += v[1] * s;
	dst[2] += v[2] * s;
}

inline static void VectorMA_2( const vec3_t v, float s, vec3_t dst )
{
	dst[0] += v[0] * s;
	dst[1] += v[1] * s;
	dst[2] -= v[2] * s;
}

inline static void VectorNegatee( vec3_t v )
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}
static void V_AddLag_CSS( vec3_t origin, vec3_t angles, vec3_t front, vec3_t side, vec3_t up )
{
	AddLagAngles( cl.refdef.time, angles );

	vec3_t prev_angles;
	if ( !GetLagAngles( cl.refdef.time - 0.1f, prev_angles ) )
		return;

	vec3_t delta_angles;
	VectorSubtract( prev_angles, angles, delta_angles );
	VectorNegatee( delta_angles );

	vec3_t delta_front;
	AngleVectors( delta_angles, delta_front, NULL, NULL );

	VectorMA_2( front, ( 1 - delta_front[0] ) * viewmodel_lag_scale->value, origin );
	VectorMA_2( side, ( delta_front[1] ) * viewmodel_lag_scale->value, origin );
	VectorMA_2( up, ( -delta_front[2] ) * viewmodel_lag_scale->value, origin );


}
static void V_AddLag_HL2( vec3_t origin, vec3_t front )
{
	vec3_t delta_front;
	static vec3_t last_front;

	delta_front[0] = front[0] - last_front[0];
	delta_front[1] = front[1] - last_front[1];
	delta_front[2] = front[2] - last_front[2];

	VectorMA_KopaFish( delta_front, viewmodel_lag_speed->value * cl.refdef.frametime, last_front );// Xashdaki VectorMA Hata veriyor Sadece Aldýgým yerdeki VectorMA'yý kullanýyorum
	VectorMA_2( delta_front, -1 * viewmodel_lag_scale->value, origin );
}

// custom thirdperson
void ThirdPerson( )
{
	cl_entity_t *m_iLocal = CL_GetLocalPlayer( );
	if ( IsAliveEntity( m_iLocal ))
	{
		vec3_t r, u, b, newViewOrg;

		VectorScale( cl.refdef.right, cl_thirdperson_right->value, r );
		VectorScale( cl.refdef.up, cl_thirdperson_up->value, u );
		VectorScale( cl.refdef.forward, -cl_thirdperson_forward->value, b );
		VectorAdd( cl.refdef.vieworg, r, newViewOrg );
		VectorAdd( newViewOrg, u, newViewOrg );
		VectorAdd( newViewOrg, b, newViewOrg );

		pmtrace_t trace;
		CL_SetTraceHull( 2 );
		CL_PlayerTrace( cl.refdef.vieworg, newViewOrg, PM_TRACELINE_PHYSENTSONLY, -1, &trace );
		if ( trace.fraction != 1.0f )
			VectorCopy( trace.endpos, cl.refdef.vieworg );
		else
			VectorCopy( newViewOrg, cl.refdef.vieworg );
	}
}

void FirstPerson( )
{
	cl_entity_t *m_iLocal = CL_GetLocalPlayer( );
	if ( IsAliveEntity( m_iLocal ))
	{
		vec3_t r, u, b, newViewOrg;
		VectorScale( cl.refdef.right, 0, r );
		VectorScale( cl.refdef.up, 0, u );
		VectorScale( cl.refdef.forward, 0, b );
		VectorAdd( cl.refdef.vieworg, r, newViewOrg );
		VectorAdd( newViewOrg, u, newViewOrg );
		VectorAdd( newViewOrg, b, newViewOrg );
		pmtrace_t trace;
		CL_SetTraceHull( 2 );
		CL_PlayerTrace( cl.refdef.vieworg, newViewOrg, PM_TRACELINE_PHYSENTSONLY, -1, &trace );
		if ( trace.fraction != 1.0f )
			VectorCopy( trace.endpos, cl.refdef.vieworg );
		else
			VectorCopy( newViewOrg, cl.refdef.vieworg );
	}
}

/*
===============
V_CalcRefDef

sets cl.refdef view values
===============
*/
void V_CalcRefDef( void )
{
	R_Set2DMode( false );
	tr.framecount++;	// g-cont. keep actual frame for all viewpasses

	do
	{
		clgame.dllFuncs.pfnCalcRefdef( &cl.refdef );
		if ( viewmodel_lag_style->value != 0 )
		{
			cl_entity_t *vm;
			vec3_t front, side, up;

			vm = &clgame.viewent;
			AngleVectors( vm->angles, front, side, up );

			/* fix the slight difference between view and vm origin */
			vm->origin[0] += 1.0f / 32;
			vm->origin[1] += 1.0f / 32;
			vm->origin[2] += 1.0f / 32;

			switch ( (int)viewmodel_lag_style->value )
			{
			case 1:
				V_AddLag_HL2( vm->origin, front );
				break;

			case 2:
				V_AddLag_CSS( vm->origin, vm->angles, front, side, up );
				break;
			}
		}

		if ( m_iThirdPerson )
		{
			ThirdPerson( );
			Cvar_Set( "cam_command", "1" );
		}
		else
		{
		FirstPerson( );
			Cvar_Set( "cam_command", "2" );
		}

		V_MergeOverviewRefdef( &cl.refdef );
		R_RenderFrame( &cl.refdef, true );
		cl.refdef.onlyClientDraw = false;
	} while( cl.refdef.nextView );

	// Xash3D extension. draw debug triangles on a server
	SV_DrawDebugTriangles ();

	SCR_AddDirtyPoint( cl.refdef.viewport[0], cl.refdef.viewport[1] );
	SCR_AddDirtyPoint( cl.refdef.viewport[0] + cl.refdef.viewport[2] - 1, cl.refdef.viewport[1] + cl.refdef.viewport[3] - 1 );
}

//============================================================================

/*
==================
V_RenderView

==================
*/
void V_RenderView( void )
{
	if( !cl.video_prepped || ( !ui_renderworld->value && UI_IsVisible() && !cl.background ))
		return; // still loading

	if( cl.frame.valid && ( cl.force_refdef || !cl.refdef.paused ))
	{
		cl.force_refdef = false;

		R_ClearScene ();
		CL_AddEntities ();
		V_SetupRefDef ();
	}

	V_CalcRefDef ();
}

/*
==================
V_PreRender

==================
*/
qboolean V_PreRender( void )
{
	// too early
	if( !glw_state.initialized )
		return false;

	if( host.state == HOST_NOFOCUS && !( host.force_draw_version && ( host.force_draw_version_time > host.realtime ) ) )
		return false;

	if( host.state == HOST_SLEEP )
		return false;

	// if the screen is disabled (loading plaque is up)
	if( cls.disable_screen )
	{
		if(( host.realtime - cls.disable_screen ) > cl_timeout->value )
		{
			MsgDev( D_NOTE, "V_PreRender: loading plaque timed out.\n" );
			cls.disable_screen = 0.0f;
		}
		return false;
	}
	
	R_BeginFrame( !cl.refdef.paused );

	return true;
}

/*
==================
V_PostRender

==================
*/
void V_PostRender( void )
{
	qboolean	draw_2d = false;

	R_Set2DMode( true );

	if( cls.state == ca_active )
	{
		SCR_TileClear();
		CL_DrawHUD( CL_ACTIVE );
		VGui_Paint();
	}

	switch( cls.scrshot_action )
	{
	case scrshot_inactive:
	case scrshot_normal:
	case scrshot_snapshot:
		draw_2d = true;
		break;
	default:
		break;
	}

	if( draw_2d )
	{
		Touch_Draw();
		SCR_RSpeeds();
		SCR_NetSpeeds();
		R_Strobe_DrawDebugInfo( );

		SCR_DrawPos();
		SV_DrawOrthoTriangles();
		CL_DrawDemoRecording();
		R_ShowTextures();
		CL_DrawHUD( CL_CHANGELEVEL );
		
		Con_DrawConsole();
		UI_UpdateMenu( host.realtime );
		SCR_DrawNetGraph();
		Con_DrawVersion();
#if 0
		Joy_DrawOnScreenKeyboard();
#endif
		Con_DrawDebug(); // must be last
		S_ExtraUpdate();
	}

	SCR_MakeScreenShot();
	R_EndFrame();
}
#endif // XASH_DEDICATED
