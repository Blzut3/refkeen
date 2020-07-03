// WL_DRAW.C

#include "wl_def.h"
//#include <DOS.H>
#pragma hdrstop

//#define DEBUGWALLS
//#define DEBUGTICS

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

// *** S3DNA RESTORATION ***
// This seems to be required for S3DNA
#ifdef GAMEVER_NOAH3D
#define DOORWALL(x)	((PMSpriteStart-(x))-8)
#else
// the door is the last picture before the sprites
#define DOORWALL	(PMSpriteStart-8)
#endif

#define ACTORSIZE	0x4000

/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/


// *** SHAREWARE V1.0 APOGEE RESTORATION ***
// Not sure how were these values picked, but here they are
#if (GAMEVER_WOLFREV <= GV_WR_WL1AP10)
#ifdef DEBUGWALLS
unsigned screenloc[3]= {0,0,0};
#else
unsigned screenloc[3]= {3328,16128,28928};
#endif
unsigned freelatch = 41728;
#else
#ifdef DEBUGWALLS
unsigned screenloc[3]= {0,0,0};
#else
unsigned screenloc[3]= {PAGE1START,PAGE2START,PAGE3START};
#endif
unsigned freelatch = FREESTART;
#endif

// *** SHAREWARE V1.0 APOGEE RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL1AP10)
int 	screenpage;
#endif

long 	lasttimecount;
long 	frameon;

unsigned	wallheight[MAXVIEWWIDTH];

fixed	tileglobal	= TILEGLOBAL;
fixed	mindist		= MINDIST;


//
// math tables
//
int			pixelangle[MAXVIEWWIDTH];
long		far finetangent[FINEANGLES/4];
fixed 		far sintable[ANGLES+ANGLES/4],far *costable = sintable+(ANGLES/4);

//
// refresh variables
//
fixed	viewx,viewy;			// the focal point
int		viewangle;
fixed	viewsin,viewcos;



fixed	FixedByFrac (fixed a, fixed b);
void	TransformActor (objtype *ob);
void	BuildTables (void);
void	ClearScreen (void);
int		CalcRotate (objtype *ob);
void	DrawScaleds (void);
void	CalcTics (void);
void	FixOfs (void);
void	ThreeDRefresh (void);



//
// wall optimization variables
//
int		lastside;		// true for vertical
long	lastintercept;
int		lasttilehit;

// *** ALPHA RESTORATION *** - A couple of unused variables
#if (GAMEVER_WOLFREV <= GV_WR_WL920312)
int	someUnusedDrawVar1, someUnusedDrawVar2;
#endif

//
// ray tracing variables
//
int			focaltx,focalty,viewtx,viewty;

int			midangle,angle;
unsigned	xpartial,ypartial;
unsigned	xpartialup,xpartialdown,ypartialup,ypartialdown;
unsigned	xinttile,yinttile;

unsigned	tilehit;
unsigned	pixx;

int		xtile,ytile;
int		xtilestep,ytilestep;
long	xintercept,yintercept;
long	xstep,ystep;

int		horizwall[MAXWALLTILES],vertwall[MAXWALLTILES];


/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/


void AsmRefresh (void);			// in WL_DR_A.ASM

/*
============================================================================

			   3 - D  DEFINITIONS

============================================================================
*/


//==========================================================================


/*
========================
=
= FixedByFrac
=
= multiply a 16/16 bit, 2's complement fixed point number by a 16 bit
= fraction, passed as a signed magnitude 32 bit number
=
========================
*/

#pragma warn -rvl			// I stick the return value in with ASMs

fixed FixedByFrac (fixed a, fixed b)
{
//
// setup
//
asm	mov	si,[WORD PTR b+2]	// sign of result = sign of fraction

asm	mov	ax,[WORD PTR a]
asm	mov	cx,[WORD PTR a+2]

asm	or	cx,cx
asm	jns	aok:				// negative?
asm	neg	cx
asm	neg	ax
asm	sbb	cx,0
asm	xor	si,0x8000			// toggle sign of result
aok:

//
// multiply  cx:ax by bx
//
asm	mov	bx,[WORD PTR b]
asm	mul	bx					// fraction*fraction
asm	mov	di,dx				// di is low word of result
asm	mov	ax,cx				//
asm	mul	bx					// units*fraction
asm add	ax,di
asm	adc	dx,0

//
// put result dx:ax in 2's complement
//
asm	test	si,0x8000		// is the result negative?
asm	jz	ansok:
asm	neg	dx
asm	neg	ax
asm	sbb	dx,0

ansok:;

}

#pragma warn +rvl

//==========================================================================

/*
========================
=
= TransformActor
=
= Takes paramaters:
=   gx,gy		: globalx/globaly of point
=
= globals:
=   viewx,viewy		: point of view
=   viewcos,viewsin	: sin/cos of viewangle
=   scale		: conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
========================
*/


//
// transform actor
//
void TransformActor (objtype *ob)
{
	int ratio;
	fixed gx,gy,gxt,gyt,nx,ny;
	long	temp;

//
// translate point to view centered coordinates
//
	gx = ob->x-viewx;
	gy = ob->y-viewy;

//
// calculate newx
//
	gxt = FixedByFrac(gx,viewcos);
	gyt = FixedByFrac(gy,viewsin);
	nx = gxt-gyt-ACTORSIZE;		// fudge the shape forward a bit, because
								// the midpoint could put parts of the shape
								// into an adjacent wall

//
// calculate newy
//
	gxt = FixedByFrac(gx,viewsin);
	gyt = FixedByFrac(gy,viewcos);
	ny = gyt+gxt;

//
// calculate perspective ratio
//
	ob->transx = nx;
	ob->transy = ny;

	if (nx<mindist)			// too close, don't overflow the divide
	{
	  ob->viewheight = 0;
	  return;
	}

	ob->viewx = centerx + ny*scale/nx;	// DEBUG: use assembly divide

//
// calculate height (heightnumerator/(nx>>8))
//
	asm	mov	ax,[WORD PTR heightnumerator]
	asm	mov	dx,[WORD PTR heightnumerator+2]
	asm	idiv	[WORD PTR nx+1]			// nx>>8
	asm	mov	[WORD PTR temp],ax
	asm	mov	[WORD PTR temp+2],dx

	ob->viewheight = temp;
}

//==========================================================================

/*
========================
=
= TransformTile
=
= Takes paramaters:
=   tx,ty		: tile the object is centered in
=
= globals:
=   viewx,viewy		: point of view
=   viewcos,viewsin	: sin/cos of viewangle
=   scale		: conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
= Returns true if the tile is withing getting distance
=
========================
*/

boolean TransformTile (int tx, int ty, int *dispx, int *dispheight)
{
	int ratio;
	fixed gx,gy,gxt,gyt,nx,ny;
	long	temp;

//
// translate point to view centered coordinates
//
	gx = ((long)tx<<TILESHIFT)+0x8000-viewx;
	gy = ((long)ty<<TILESHIFT)+0x8000-viewy;

//
// calculate newx
//
	gxt = FixedByFrac(gx,viewcos);
	gyt = FixedByFrac(gy,viewsin);
	nx = gxt-gyt-0x2000;		// 0x2000 is size of object

//
// calculate newy
//
	gxt = FixedByFrac(gx,viewsin);
	gyt = FixedByFrac(gy,viewcos);
	ny = gyt+gxt;


//
// calculate perspective ratio
//
	if (nx<mindist)			// too close, don't overflow the divide
	{
		*dispheight = 0;
		return false;
	}

	*dispx = centerx + ny*scale/nx;	// DEBUG: use assembly divide

//
// calculate height (heightnumerator/(nx>>8))
//
	asm	mov	ax,[WORD PTR heightnumerator]
	asm	mov	dx,[WORD PTR heightnumerator+2]
	asm	idiv	[WORD PTR nx+1]			// nx>>8
	asm	mov	[WORD PTR temp],ax
	asm	mov	[WORD PTR temp+2],dx

	*dispheight = temp;

//
// see if it should be grabbed
//
	if (nx<TILEGLOBAL && ny>-TILEGLOBAL/2 && ny<TILEGLOBAL/2)
		return true;
	else
		return false;
}

//==========================================================================

/*
====================
=
= CalcHeight
=
= Calculates the height of xintercept,yintercept from viewx,viewy
=
====================
*/

#pragma warn -rvl			// I stick the return value in with ASMs

int	CalcHeight (void)
{
	int	transheight;
	int ratio;
	fixed gxt,gyt,nx,ny;
	long	gx,gy;

	gx = xintercept-viewx;
	gxt = FixedByFrac(gx,viewcos);

	gy = yintercept-viewy;
	gyt = FixedByFrac(gy,viewsin);

	nx = gxt-gyt;

  //
  // calculate perspective ratio (heightnumerator/(nx>>8))
  //
	if (nx<mindist)
		nx=mindist;			// don't let divide overflow

	asm	mov	ax,[WORD PTR heightnumerator]
	asm	mov	dx,[WORD PTR heightnumerator+2]
	asm	idiv	[WORD PTR nx+1]			// nx>>8
}


//==========================================================================

/*
===================
=
= ScalePost
=
===================
*/

long		postsource;
unsigned	postx;
unsigned	postwidth;

void	near ScalePost (void)		// VGA version
{
	asm	mov	ax,SCREENSEG
	asm	mov	es,ax

	asm	mov	bx,[postx]
	asm	shl	bx,1
	asm	mov	bp,WORD PTR [wallheight+bx]		// fractional height (low 3 bits frac)
	asm	and	bp,0xfff8				// bp = heightscaler*4
	asm	shr	bp,1
	asm	cmp	bp,[maxscaleshl2]
	asm	jle	heightok
	asm	mov	bp,[maxscaleshl2]
heightok:
	asm	add	bp,OFFSET fullscalefarcall
	//
	// scale a byte wide strip of wall
	//
	asm	mov	bx,[postx]
	asm	mov	di,bx
	asm	shr	di,2						// X in bytes
	asm	add	di,[bufferofs]

	asm	and	bx,3
	asm	shl	bx,3						// bx = pixel*8+pixwidth
	asm	add	bx,[postwidth]

	asm	mov	al,BYTE PTR [mapmasks1-1+bx]	// -1 because no widths of 0
	asm	mov	dx,SC_INDEX+1
	asm	out	dx,al						// set bit mask register
	asm	lds	si,DWORD PTR [postsource]
	asm	call DWORD PTR [bp]				// scale the line of pixels

	asm	mov	al,BYTE PTR [ss:mapmasks2-1+bx]   // -1 because no widths of 0
	asm	or	al,al
	asm	jz	nomore

	//
	// draw a second byte for vertical strips that cross two bytes
	//
	asm	inc	di
	asm	out	dx,al						// set bit mask register
	asm	call DWORD PTR [bp]				// scale the line of pixels

	asm	mov	al,BYTE PTR [ss:mapmasks3-1+bx]	// -1 because no widths of 0
	asm	or	al,al
	asm	jz	nomore
	//
	// draw a third byte for vertical strips that cross three bytes
	//
	asm	inc	di
	asm	out	dx,al						// set bit mask register
	asm	call DWORD PTR [bp]				// scale the line of pixels


nomore:
	asm	mov	ax,ss
	asm	mov	ds,ax
}

void  FarScalePost (void)				// just so other files can call
{
	ScalePost ();
}


/*
====================
=
= HitVertWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

void HitVertWall (void)
{
	int			wallpic;
	unsigned	texture;

	texture = (yintercept>>4)&0xfc0;
	if (xtilestep == -1)
	{
		texture = 0xfc0-texture;
		xintercept += TILEGLOBAL;
	}
	wallheight[pixx] = CalcHeight();

	if (lastside==1 && lastintercept == xtile && lasttilehit == tilehit)
	{
		// in the same wall type as last time, so check for optimized draw
		if (texture == (unsigned)postsource)
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		else
		{
			ScalePost ();
			(unsigned)postsource = texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
	// new wall
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();

		lastside = true;
		lastintercept = xtile;

		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		if (tilehit & 0x40)
		{								// check for adjacent doors
			ytile = yintercept>>TILESHIFT;
			if ( tilemap[xtile-xtilestep][ytile]&0x80 )
				// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
				wallpic = DOORWALL(1);
#else
				wallpic = DOORWALL+3;
#endif
			else
				wallpic = vertwall[tilehit & ~0x40];
		}
		else
			wallpic = vertwall[tilehit];

		*( ((unsigned *)&postsource)+1) = (unsigned)PM_GetPage(wallpic);
		(unsigned)postsource = texture;

	}
	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	tilemap[xtile][ytile] |= 0x20;
#endif
}


/*
====================
=
= HitHorizWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

void HitHorizWall (void)
{
	int			wallpic;
	unsigned	texture;

	texture = (xintercept>>4)&0xfc0;
	if (ytilestep == -1)
		yintercept += TILEGLOBAL;
	else
		texture = 0xfc0-texture;
	wallheight[pixx] = CalcHeight();

	if (lastside==0 && lastintercept == ytile && lasttilehit == tilehit)
	{
		// in the same wall type as last time, so check for optimized draw
		if (texture == (unsigned)postsource)
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		else
		{
			ScalePost ();
			(unsigned)postsource = texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
	// new wall
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();

		lastside = 0;
		lastintercept = ytile;

		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		if (tilehit & 0x40)
		{								// check for adjacent doors
			xtile = xintercept>>TILESHIFT;
			if ( tilemap[xtile][ytile-ytilestep]&0x80 )
				// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
				wallpic = DOORWALL(1);
#else
				wallpic = DOORWALL+2;
#endif
			else
				wallpic = horizwall[tilehit & ~0x40];
		}
		else
			wallpic = horizwall[tilehit];

		*( ((unsigned *)&postsource)+1) = (unsigned)PM_GetPage(wallpic);
		(unsigned)postsource = texture;
	}

	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	tilemap[xtile][ytile] |= 0x20;
#endif
}

//==========================================================================

/*
====================
=
= HitHorizDoor
=
====================
*/

void HitHorizDoor (void)
{
	unsigned	texture,doorpage,doornum;

	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	doornum = tilehit&0x1f;
	doorobjlist[doornum].seen = true;
#else
	doornum = tilehit&0x7f;
#endif
	texture = ( (xintercept-doorposition[doornum]) >> 4) &0xfc0;

	wallheight[pixx] = CalcHeight();

	if (lasttilehit == tilehit)
	{
	// in the same door as last time, so check for optimized draw
		if (texture == (unsigned)postsource)
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		else
		{
			ScalePost ();
			(unsigned)postsource = texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();			// draw last post
	// first pixel in this door
		lastside = 2;
		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		switch (doorobjlist[doornum].lock)
		{
		// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
		case dr_normal:
			doorpage = DOORWALL(5);
			break;
		case dr_lock1:
			doorpage = DOORWALL(4);
			break;
		case dr_lock2:
			doorpage = DOORWALL(3);
			break;
		case dr_elevator:
			doorpage = DOORWALL(2);
			break;
#else
		case dr_normal:
			doorpage = DOORWALL;
			break;
		case dr_lock1:
		case dr_lock2:
		case dr_lock3:
		case dr_lock4:
			doorpage = DOORWALL+6;
			break;
		case dr_elevator:
			doorpage = DOORWALL+4;
			break;
#endif
		}

		*( ((unsigned *)&postsource)+1) = (unsigned)PM_GetPage(doorpage);
		(unsigned)postsource = texture;
	}
	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	tilemap[xtile][ytile] |= 0x20;
#endif
}

//==========================================================================

/*
====================
=
= HitVertDoor
=
====================
*/

void HitVertDoor (void)
{
	unsigned	texture,doorpage,doornum;

	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	doornum = tilehit&0x1f;
	doorobjlist[doornum].seen = true;
#else
	doornum = tilehit&0x7f;
#endif
	texture = ( (yintercept-doorposition[doornum]) >> 4) &0xfc0;

	wallheight[pixx] = CalcHeight();

	if (lasttilehit == tilehit)
	{
	// in the same door as last time, so check for optimized draw
		if (texture == (unsigned)postsource)
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		else
		{
			ScalePost ();
			(unsigned)postsource = texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();			// draw last post
	// first pixel in this door
		lastside = 2;
		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		switch (doorobjlist[doornum].lock)
		{
		// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
		case dr_normal:
			doorpage = DOORWALL(5);
			break;
		case dr_lock1:
			doorpage = DOORWALL(4);
			break;
		case dr_lock2:
			doorpage = DOORWALL(3);
			break;
		case dr_elevator:
			doorpage = DOORWALL(2);
			break;
#else
		case dr_normal:
			doorpage = DOORWALL;
			break;
		case dr_lock1:
		case dr_lock2:
		case dr_lock3:
		case dr_lock4:
			doorpage = DOORWALL+6;
			break;
		case dr_elevator:
			doorpage = DOORWALL+4;
			break;
#endif
		}

		// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
		*( ((unsigned *)&postsource)+1) = (unsigned)PM_GetPage(doorpage);
#else
		*( ((unsigned *)&postsource)+1) = (unsigned)PM_GetPage(doorpage+1);
#endif
		(unsigned)postsource = texture;
	}
	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	tilemap[xtile][ytile] |= 0x20;
#endif
}

// *** ALPHA RESTORATION ***
#if (GAMEVER_WOLFREV > GV_WR_WL920312)
//==========================================================================


/*
====================
=
= HitHorizPWall
=
= A pushable wall in action has been hit
=
====================
*/

void HitHorizPWall (void)
{
	int			wallpic;
	unsigned	texture,offset;

	texture = (xintercept>>4)&0xfc0;
	offset = pwallpos<<10;
	if (ytilestep == -1)
		yintercept += TILEGLOBAL-offset;
	else
	{
		texture = 0xfc0-texture;
		yintercept += offset;
	}

	wallheight[pixx] = CalcHeight();

	if (lasttilehit == tilehit)
	{
		// in the same wall type as last time, so check for optimized draw
		if (texture == (unsigned)postsource)
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		else
		{
			ScalePost ();
			(unsigned)postsource = texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
	// new wall
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();

		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		wallpic = horizwall[tilehit&63];

		*( ((unsigned *)&postsource)+1) = (unsigned)PM_GetPage(wallpic);
		(unsigned)postsource = texture;
	}

	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	tilemap[xtile][ytile] |= 0x20;
#endif
}


/*
====================
=
= HitVertPWall
=
= A pushable wall in action has been hit
=
====================
*/

void HitVertPWall (void)
{
	int			wallpic;
	unsigned	texture,offset;

	texture = (yintercept>>4)&0xfc0;
	offset = pwallpos<<10;
	if (xtilestep == -1)
	{
		xintercept += TILEGLOBAL-offset;
		texture = 0xfc0-texture;
	}
	else
		xintercept += offset;

	wallheight[pixx] = CalcHeight();

	if (lasttilehit == tilehit)
	{
		// in the same wall type as last time, so check for optimized draw
		if (texture == (unsigned)postsource)
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		else
		{
			ScalePost ();
			(unsigned)postsource = texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
	// new wall
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();

		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		wallpic = vertwall[tilehit&63];

		*( ((unsigned *)&postsource)+1) = (unsigned)PM_GetPage(wallpic);
		(unsigned)postsource = texture;
	}

	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	tilemap[xtile][ytile] |= 0x20;
#endif
}
#endif // GAMEVER_WOLFREV > GV_WR_WL920312

//==========================================================================

//==========================================================================

// *** SHAREWARE V1.0 APOGEE + ALPHA RESTORATION ***
// Re-enable unused EGA code, *and* restore egaFloor+egaCeiling (not in alpha)
#if (GAMEVER_WOLFREV <= GV_WR_WL1AP10)
#if (GAMEVER_WOLFREV > GV_WR_WL920312)
unsigned egaFloor[] = {0,0,0,0,0,0,0,0,0,5};
unsigned egaCeiling[] = {0x0808,0x0808,0x0808,0x0808,0x0808,0x0808,0x0808,0x0808,0x0808,0x0d0d};
#endif

//#if 0
/*
=====================
=
= ClearScreen
=
=====================
*/

void ClearScreen (void)
{
// *** ALPHA RESTORATION ***
#if (GAMEVER_WOLFREV > GV_WR_WL920312)
 unsigned floor=egaFloor[gamestate.episode*10+mapon],
	  ceiling=egaCeiling[gamestate.episode*10+mapon];
#endif

  //
  // clear the screen
  //
asm	mov	dx,GC_INDEX
asm	mov	ax,GC_MODE + 256*2		// read mode 0, write mode 2
asm	out	dx,ax
asm	mov	ax,GC_BITMASK + 255*256
asm	out	dx,ax

asm	mov	dx,40
asm	mov	ax,[viewwidth]
asm	shr	ax,3
asm	sub	dx,ax					// dx = 40-viewwidth/8

asm	mov	bx,[viewwidth]
asm	shr	bx,4					// bl = viewwidth/16
asm	mov	bh,BYTE PTR [viewheight]
asm	shr	bh,1					// half height

#if (GAMEVER_WOLFREV <= GV_WR_WL920312)
asm	xor	ax,ax
#else
asm	mov	ax,[ceiling]
#endif
asm	mov	es,[screenseg]
asm	mov	di,[bufferofs]

toploop:
asm	mov	cl,bl
asm	rep	stosw
asm	add	di,dx
asm	dec	bh
asm	jnz	toploop

asm	mov	bh,BYTE PTR [viewheight]
asm	shr	bh,1					// half height
#if (GAMEVER_WOLFREV <= GV_WR_WL920312)
asm	mov	ax,0x0808
#else
asm	mov	ax,[floor]
#endif

bottomloop:
asm	mov	cl,bl
asm	rep	stosw
asm	add	di,dx
asm	dec	bh
asm	jnz	bottomloop


asm	mov	dx,GC_INDEX
asm	mov	ax,GC_MODE + 256*10		// read mode 1, write mode 2
asm	out	dx,ax
asm	mov	al,GC_BITMASK
asm	out	dx,al

}
#endif
//==========================================================================

// *** ALPHA RESTORATION ***
#if (GAMEVER_WOLFREV > GV_WR_WL920312)
unsigned vgaCeiling[]=
{
// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
 0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,
 0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,
 0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,
 0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2,0xd2d2
#elif (!defined SPEAR)
//#ifndef SPEAR
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0xbfbf,
 0x4e4e,0x4e4e,0x4e4e,0x1d1d,0x8d8d,0x4e4e,0x1d1d,0x2d2d,0x1d1d,0x8d8d,
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x2d2d,0xdddd,0x1d1d,0x1d1d,//0x9898,
// *** SHAREWARE V1.0+1.1 APOGEE RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL1AP10)
 0x8d8d,
#else
 0x9898,
#if (GAMEVER_WOLFREV > GV_WR_WL1AP11)
 0x1d1d,0x9d9d,0x2d2d,0xdddd,0xdddd,0x9d9d,0x2d2d,0x4d4d,0x1d1d,0xdddd,
 0x7d7d,0x1d1d,0x2d2d,0x2d2d,0xdddd,0xd7d7,0x1d1d,0x1d1d,0x1d1d,0x2d2d,
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0xdddd,0xdddd,0x7d7d,0xdddd,0xdddd,0xdddd
#else
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0xd7d7,0x1d1d,0x1d1d,0x1d1d,0x1d1d,
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,
#endif
#endif // GAMEVER_WOLFREV <= GV_WR_WL1AP10
#else
 0x6f6f,0x4f4f,0x1d1d,0xdede,0xdfdf,0x2e2e,0x7f7f,0x9e9e,0xaeae,0x7f7f,
 0x1d1d,0xdede,0xdfdf,0xdede,0xdfdf,0xdede,0xe1e1,0xdcdc,0x2e2e,0x1d1d,0xdcdc
#endif
};
#endif // GAMEVER_WOLFREV > GV_WR_WL920312

/*
=====================
=
= VGAClearScreen
=
=====================
*/

void VGAClearScreen (void)
{
 // *** S3DNA + ALPHA RESTORATION ***
#ifdef GAMEVER_NOAH3D
 unsigned ceiling=vgaCeiling[gamestate.mapon];
#elif (GAMEVER_WOLFREV > GV_WR_WL920312)
 unsigned ceiling=vgaCeiling[gamestate.episode*10+mapon];
#endif

  //
  // clear the screen
  //
// *** ALPHA RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL920312)
asm	mov	dx,SC_INDEX+1
asm	mov	al,15
asm	out	dx,al
#else
asm	mov	dx,SC_INDEX
asm	mov	ax,SC_MAPMASK+15*256	// write through all planes
asm	out	dx,ax
#endif

asm	mov	dx,80
asm	mov	ax,[viewwidth]
asm	shr	ax,2
asm	sub	dx,ax					// dx = 40-viewwidth/2

asm	mov	bx,[viewwidth]
asm	shr	bx,3					// bl = viewwidth/8
asm	mov	bh,BYTE PTR [viewheight]
asm	shr	bh,1					// half height

asm	mov	es,[screenseg]
asm	mov	di,[bufferofs]
// *** ALPHA RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL920312)
asm	mov	ax,0x1d1d
#else
asm	mov	ax,[ceiling]
#endif

toploop:
asm	mov	cl,bl
asm	rep	stosw
asm	add	di,dx
asm	dec	bh
asm	jnz	toploop

asm	mov	bh,BYTE PTR [viewheight]
asm	shr	bh,1					// half height
	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
asm	mov	ax,0xd9d9
#else
asm	mov	ax,0x1919
#endif

bottomloop:
asm	mov	cl,bl
asm	rep	stosw
asm	add	di,dx
asm	dec	bh
asm	jnz	bottomloop
}

//==========================================================================

/*
=====================
=
= CalcRotate
=
=====================
*/

int	CalcRotate (objtype *ob)
{
	int	angle,viewangle;
	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	if (ob->obclass == flameobj || ob->obclass == missileobj)
		return 0;
#endif

	// this isn't exactly correct, as it should vary by a trig value,
	// but it is close enough with only eight rotations

	viewangle = player->angle + (centerx - ob->viewx)/8;

	// *** PRE-V1.4 APOGEE + S3DNA RESTORATION ***
	// Including special cases for Wolf3D v1.0 and S3DNA
#if (GAMEVER_WOLFREV > GV_WR_WL1AP10) && (!defined GAMEVER_NOAH3D)
#if (GAMEVER_WOLFREV <= GV_WR_WL6AP11)
	if (ob->obclass == rocketobj)
#else
	if (ob->obclass == rocketobj || ob->obclass == hrocketobj)
#endif
		angle =  (viewangle-180)- ob->angle;
	else
#endif
		angle =  (viewangle-180)- dirangle[ob->dir];

	angle+=ANGLES/16;
	while (angle>=ANGLES)
		angle-=ANGLES;
	while (angle<0)
		angle+=ANGLES;

	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	if (ob->state->rotate == 2)
		return 0;
#else
	if (ob->state->rotate == 2)             // 2 rotation pain frame
		return 4*(angle/(ANGLES/2));        // seperated by 3 (art layout...)
#endif

	return angle/(ANGLES/8);
}


/*
=====================
=
= DrawScaleds
=
= Draws all objects that are visable
=
=====================
*/

#define MAXVISABLE	50

typedef struct
{
	int	viewx,
		viewheight,
		// *** S3DNA RESTORATION ***
		shapenum
#ifdef GAMEVER_NOAH3D
		,
		snoring
#endif
		;
} visobj_t;

// *** SHAREWARE V1.0+1.1 APOGEE RESTORATION *** - Move back into function body
#if (GAMEVER_WOLFREV > GV_WR_WL1AP11)
visobj_t	vislist[MAXVISABLE],*visptr,*visstep,*farthest;
#endif

void DrawScaleds (void)
{
	int 		i,j,least,numvisable,height;
	memptr		shape;
	byte		*tilespot,*visspot;
	int			shapenum;
	unsigned	spotloc;

	statobj_t	*statptr;
	objtype		*obj;

// *** SHAREWARE V1.0+1.1 APOGEE RESTORATION *** - Moved back into function body from outside
#if (GAMEVER_WOLFREV <= GV_WR_WL1AP11)
	visobj_t	vislist[MAXVISABLE],*visptr,*visstep,*farthest;

	if (nospr)
		return;
#endif

	visptr = &vislist[0];

//
// place static objects
//
	for (statptr = &statobjlist[0] ; statptr !=laststatobj ; statptr++)
	{
		if ((visptr->shapenum = statptr->shapenum) == -1)
			continue;						// object has been deleted

		if (!*statptr->visspot)
			continue;						// not visable

		if (TransformTile (statptr->tilex,statptr->tiley
			,&visptr->viewx,&visptr->viewheight) && statptr->flags & FL_BONUS)
		{
			GetBonus (statptr);
			continue;
		}

		// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
		if (statptr->shapenum == -2)
			continue;

#endif
		if (!visptr->viewheight)
			continue;						// to close to the object
		// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
		visptr->snoring = 0;
#endif

		// *** SHAREWARE V1.0+1.1 APOGEE RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL1AP11)
		if (visptr < &vislist[MAXVISABLE])
#else
		if (visptr < &vislist[MAXVISABLE-1])	// don't let it overflow
#endif
			visptr++;
	}

//
// place active objects
//
	for (obj = player->next;obj;obj=obj->next)
	{
		if (!(visptr->shapenum = obj->state->shapenum))
			continue;						// no shape

		spotloc = (obj->tilex<<6)+obj->tiley;	// optimize: keep in struct?
		visspot = &spotvis[0][0]+spotloc;
		tilespot = &tilemap[0][0]+spotloc;

		//
		// could be in any of the nine surrounding tiles
		//
		if (*visspot
		|| ( *(visspot-1) && !*(tilespot-1) )
		|| ( *(visspot+1) && !*(tilespot+1) )
		|| ( *(visspot-65) && !*(tilespot-65) )
		|| ( *(visspot-64) && !*(tilespot-64) )
		|| ( *(visspot-63) && !*(tilespot-63) )
		|| ( *(visspot+65) && !*(tilespot+65) )
		|| ( *(visspot+64) && !*(tilespot+64) )
		|| ( *(visspot+63) && !*(tilespot+63) ) )
		{
			obj->active = true;
			TransformActor (obj);
			if (!obj->viewheight)
				continue;						// too close or far away

			visptr->viewx = obj->viewx;
			visptr->viewheight = obj->viewheight;
			// *** S3DNA RESTORATION ***
#ifndef GAMEVER_NOAH3D
			if (visptr->shapenum == -1)
				visptr->shapenum = obj->temp1;	// special shape
#endif

			if (obj->state->rotate)
				visptr->shapenum += CalcRotate (obj);

			// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
			if ((obj->hitpoints > 0) ||
			    (obj->obclass == missileobj) || (obj->obclass == flameobj) || (obj->obclass == needleobj) || (obj->obclass == rocketobj))
				visptr->snoring = 0;
			else
			{
				// 2.6 number. Upper is frame, Lower is tics*2
				visptr->snoring = obj->snore>>6;
				obj->snore += 3*tics;
			}
#endif

		// *** SHAREWARE V1.0+1.1 APOGEE RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL1AP11)
			if (visptr < &vislist[MAXVISABLE])
#else
			if (visptr < &vislist[MAXVISABLE-1])	// don't let it overflow
#endif
				visptr++;
			obj->flags |= FL_VISABLE;
		}
		else
			obj->flags &= ~FL_VISABLE;
	}

//
// draw from back to front
//
	numvisable = visptr-&vislist[0];

	if (!numvisable)
		return;									// no visable objects

	for (i = 0; i<numvisable; i++)
	{
		least = 32000;
		for (visstep=&vislist[0] ; visstep<visptr ; visstep++)
		{
			height = visstep->viewheight;
			if (height < least)
			{
				least = height;
				farthest = visstep;
			}
		}
		//
		// draw farthest
		//
		ScaleShape(farthest->viewx,farthest->shapenum,farthest->viewheight);

		// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
		if (farthest->snoring)
			ScaleShape(farthest->viewx,farthest->snoring+SPR_SNOOZE_1-1,farthest->viewheight);
#endif
		farthest->viewheight = 32000;
	}

}

//==========================================================================

/*
==============
=
= DrawPlayerWeapon
=
= Draw the player's hands
=
==============
*/

int	weaponscale[NUMWEAPONS] = {SPR_KNIFEREADY,SPR_PISTOLREADY
	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	,SPR_MACHINEGUNREADY,SPR_CHAINREADY,SPR_CANTAREADY,SPR_WATERREADY};
#else
	,SPR_MACHINEGUNREADY,SPR_CHAINREADY};
#endif

void DrawPlayerWeapon (void)
{
	int	shapenum;

	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	if (player->state == &s_deathcam)
	{
		if (endtics >= 192)
			playstate = ex_victorious;
		SimpleScaleShape(viewwidth/2,SPR_YOUWIN,endtics);
		endtics += tics;
	}
	else if (player->state == &s_gameover)
	{
		SimpleScaleShape(viewwidth/2,SPR_GAMEOVER,endtics);
		endtics += tics;
	}
	// *** ALPHA RESTORATION ***
#elif (!defined SPEAR) && (GAMEVER_WOLFREV > GV_WR_WL920312)
//#ifndef SPEAR
	if (gamestate.victoryflag)
	{
// *** SHAREWARE V1.0 APOGEE RESTORATION ***
#if (GAMEVER_WOLFREV > GV_WR_WL1AP10)
		if (player->state == &s_deathcam && (TimeCount&32) )
			SimpleScaleShape(viewwidth/2,SPR_DEATHCAM,viewheight+1);
#endif
		return;
	}
#endif

	if (gamestate.weapon != -1)
	{
		shapenum = weaponscale[gamestate.weapon]+gamestate.weaponframe;
		SimpleScaleShape(viewwidth/2,shapenum,viewheight+1);
	}

	if (demorecord || demoplayback)
		SimpleScaleShape(viewwidth/2,SPR_DEMO,viewheight+1);
}


//==========================================================================


/*
=====================
=
= CalcTics
=
=====================
*/

void CalcTics (void)
{
	long	newtime,oldtimecount;

//
// calculate tics since last refresh for adaptive timing
//
	if (lasttimecount > TimeCount)
		TimeCount = lasttimecount;		// if the game was paused a LONG time

	// *** PRE-V1.4 APOGEE RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL6AP11)
	if (DemoMode != demo_Off)
	{
		oldtimecount = lasttimecount;
		while (oldtimecount + 2*DEMOTICS > TimeCount) ;

		lasttimecount = oldtimecount + DEMOTICS;
		TimeCount = lasttimecount + DEMOTICS;
		tics = DEMOTICS;
		return;
	}
#endif

	do
	{
		newtime = TimeCount;
		tics = newtime-lasttimecount;
	} while (!tics);			// make sure at least one tic passes

	lasttimecount = newtime;

#ifdef FILEPROFILE
		strcpy (scratch,"\tTics:");
		itoa (tics,str,10);
		strcat (scratch,str);
		strcat (scratch,"\n");
		write (profilehandle,scratch,strlen(scratch));
#endif

	if (tics>MAXTICS)
	{
		TimeCount -= (tics-MAXTICS);
		tics = MAXTICS;
	}
}


//==========================================================================


/*
========================
=
= FixOfs
=
========================
*/

void	FixOfs (void)
{
	// *** SHAREWARE V1.0 APOGEE RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL1AP10)
	if (++screenpage == 3)
		screenpage = 0;
	bufferofs = screenloc[screenpage];
#endif
	VW_ScreenToScreen (displayofs,bufferofs,viewwidth/8,viewheight);
}


//==========================================================================


/*
====================
=
= WallRefresh
=
====================
*/

void WallRefresh (void)
{
//
// set up variables for this view
//
	viewangle = player->angle;
	midangle = viewangle*(FINEANGLES/ANGLES);
	viewsin = sintable[viewangle];
	viewcos = costable[viewangle];
	viewx = player->x - FixedByFrac(focallength,viewcos);
	viewy = player->y + FixedByFrac(focallength,viewsin);

	focaltx = viewx>>TILESHIFT;
	focalty = viewy>>TILESHIFT;

	viewtx = player->x >> TILESHIFT;
	viewty = player->y >> TILESHIFT;

	xpartialdown = viewx&(TILEGLOBAL-1);
	xpartialup = TILEGLOBAL-xpartialdown;
	ypartialdown = viewy&(TILEGLOBAL-1);
	ypartialup = TILEGLOBAL-ypartialdown;

	lastside = -1;			// the first pixel is on a new wall
	AsmRefresh ();
	ScalePost ();			// no more optimization on last post
}

// *** SHAREWARE V1.0 APOGEE RESTORATION *** - An unused function from v1.0
#if (GAMEVER_WOLFREV <= GV_WR_WL1AP10)
//==========================================================================

int someUnusedDrawArray[] = {
	0x1b, 0x08, 0x1b, 0x1c, 0x00, 0x00, 0x00, 0x00,
	0x1c, 0x08, 0x1c, 0x1b, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void	SomeUnusedDrawFunc (void)
{
	int *arrPtr;
	for (arrPtr=someUnusedDrawArray; arrPtr[0]; arrPtr+=8)
	{
		arrPtr[7] -= tics;
		if (arrPtr[7] >= 0)
			continue;
		arrPtr[7] += arrPtr[1];
		++(arrPtr[6]);
		if (arrPtr[arrPtr[6]+2] == 0)
			arrPtr[6] = 0;
		horizwall[arrPtr[0]] = (arrPtr[arrPtr[6]+2]-1)<<1;
		vertwall[arrPtr[0]] = (arrPtr[arrPtr[6]+2]<<1)-1;
	}
}

#endif // GAMEVER_WOLFREV <= GV_WR_WL1AP10
//==========================================================================

/*
========================
=
= ThreeDRefresh
=
========================
*/

void	ThreeDRefresh (void)
{
	int tracedir;

// this wouldn't need to be done except for my debugger/video wierdness
	outportb (SC_INDEX,SC_MAPMASK);

//
// clear out the traced array
//
asm	mov	ax,ds
asm	mov	es,ax
asm	mov	di,OFFSET spotvis
asm	xor	ax,ax
asm	mov	cx,2048							// 64*64 / 2
asm	rep stosw

	// *** SHAREWARE V1.0 APOGEE RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL1AP10)
	if (++screenpage == 3)
		screenpage = 0;
	bufferofs = screenloc[screenpage]+screenofs;
#else
	bufferofs += screenofs;
#endif

//
// follow the walls from there to the right, drawwing as we go
//
	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	if (nofloors)
#endif
		VGAClearScreen ();

	WallRefresh ();
	// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
	if (!nofloors)
		DrawPlanes ();
#endif

//
// draw all the scaled images
//
	DrawScaleds();			// draw scaled stuff
	DrawPlayerWeapon ();	// draw player's hands

//
// show screen and time last cycle
//
	if (fizzlein)
		// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
		if (!(--fizzlein))
#endif
	{
		// *** S3DNA RESTORATION ***
#ifdef GAMEVER_NOAH3D
		VW_FadeIn ();
#else
		FizzleFade(bufferofs,displayofs+screenofs,viewwidth,viewheight,20,false);
		fizzlein = false;
#endif

	// *** PRE-V1.4 APOGEE RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL6AP11)
		lasttimecount = TimeCount;
#else
		lasttimecount = TimeCount = 0;		// don't make a big tic count
#endif

	}

	// *** SHAREWARE V1.0 APOGEE RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL1AP10)
	displayofs = bufferofs-screenofs;
#else
	bufferofs -= screenofs;
	displayofs = bufferofs;
#endif

	asm	cli
	asm	mov	cx,[displayofs]
	asm	mov	dx,3d4h		// CRTC address register
	asm	mov	al,0ch		// start address high register
	asm	out	dx,al
	asm	inc	dx
	asm	mov	al,ch
	asm	out	dx,al   	// set the high byte
	// *** SHAREWARE V1.0 APOGEE RESTORATION ***
#if (GAMEVER_WOLFREV <= GV_WR_WL1AP10)
	asm	dec	dx
	asm	mov	al,0dh
	asm	out	dx,al
	asm	inc	dx
	asm	mov	al,cl
	asm	out	dx,al
	asm	sti
#else
	asm	sti

	bufferofs += SCREENSIZE;
	if (bufferofs > PAGE3START)
		bufferofs = PAGE1START;
#endif

	frameon++;
	PM_NextFrame();
}


//===========================================================================

