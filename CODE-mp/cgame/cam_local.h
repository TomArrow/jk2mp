#pragma once

#include "../game/q_shared.h"


typedef struct camera_s
{
	/*int			frameTime;

	int			fastforwardval; //targettime of fast-forwarding (0 = off)

	float		oldTimescale; //for timescale toggling

	//pathfinding & items
	camItems_t  items;
	camItems_t	olditems;
	//
	double		velocity; //camera velocity between waypoints
	double		position;  //camera position between waypoints (0...1)

	qboolean	gtimed;
	int			gtimed_starttime;

	//freeview
	vec3_t		movedelta;              //used for smoothing
	vec2_t		lookdelta;	             //used for smoothing

	int			keyFlags; //CKF_*

	camClient_t client[MAX_CLIENTS]; //info I use for clients
	//spectating a client
	int			specEnt;
	vec3_t		specOrg;
	vec3_t		specAng;*/
	vec3_t		specVel;
	/*
	//effects
	int			raintime;  //time between raindrops :D
	vec3_t		sunorigin; //origin of drawn sun*/

} camera_t;

void Cam_DrawClientNames(void);
void Cam_Draw2d(void);