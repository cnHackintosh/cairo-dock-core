/*
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef __CAIRO_DOCK_PARTICLE_SYSTEM__
#define  __CAIRO_DOCK_PARTICLE_SYSTEM__

#include <glib.h>

G_BEGIN_DECLS

/**
*@file cairo-dock-particle-system.h A Particle System is a set of particles that evolve according to a given model. Each particle will see its parameters change with time : direction, speed, oscillation, color, size, etc.
* Particle Systems fully take advantage of OpenGL and are able to render many thousands of particles at a high frequency refresh.
* 
*/

/// A particle of a particle system.
typedef struct _CairoParticle {
	/// horizontal position, in fraction of the particle system's width, and relatively to the center of the particle system. So it is comprised between -1 and 1.
	GLfloat x;
	/// vertical position, in fraction of the particle system's height, and relatively to the bottom of the particle system. So it is comprised between 0 and 1.
	GLfloat y;
	/// depth of the particle, negative to be "behind". 0 means it is at the same depth as icons.
	GLfloat z;
	/// horizontal speed
	GLfloat vx;
	/// vertical speed
	GLfloat vy;
	/// size
	GLfloat fWidth, fHeight;
	/// color r,g,b,a
	GLfloat color[4];
	/// phase of the oscillations.
	GLfloat fOscillation;
	/// oscillation variation speed.
	GLfloat fOmega;
	/// current size factor
	GLfloat fSizeFactor;
	/// size variation speed.
	GLfloat fResizeSpeed;
	/// current life time, decreased by 1 at each step.
	gint iLife;
	/// total life time.
	gint iInitialLife;
	} CairoParticle;

/// A particle system.
typedef struct _CairoParticleSystem {
	CairoParticle *pParticles;
	gint iNbParticles;
	GLuint iTexture;
	GLfloat *pVertices;
	GLfloat *pCoords;
	GLfloat *pColors;
	GLfloat fWidth, fHeight;
	double dt;
	gboolean bDirectionUp;
	gboolean bAddLuminance;
	gboolean bAddLight;
	} CairoParticleSystem;

/// Function that re-initializes a particle when its life is over.
typedef void (CairoDockRewindParticleFunc) (CairoParticle *pParticle, double dt);

/** Render all the particles of a particle system with a given depth.
*@param pParticleSystem the particle system.
*@param iDepth depth of the particles that will be rendered. If set to -1, only particles with a negative z will be rendered, if set to 1, only particles with a positive z will be rendered, if set to 0, all the particles will be rendered.
*/
void cairo_dock_render_particles_full (CairoParticleSystem *pParticleSystem, int iDepth);
/** Render all the particles of a particle system.
*@param pParticleSystem the particle system.
*/
#define cairo_dock_render_particles(pParticleSystem) cairo_dock_render_particles_full (pParticleSystem, 0)

/**  Create a particle system.
*@param iNbParticles number of particles of the system.
*@param iTexture texture to map on each particle.
*@param fWidth width of the system.
*@param fHeight height of the system.
*@return a newly allocated particle system.
*/
CairoParticleSystem *cairo_dock_create_particle_system (int iNbParticles, GLuint iTexture, double fWidth, double fHeight);

/** Destroy a particle system, freeing all the ressources it was using.
*@param pParticleSystem the particle system.
*/
void cairo_dock_free_particle_system (CairoParticleSystem *pParticleSystem);

/** Update a particle system to the next step with a generic particle behavior model. You can write your own model depending on your needs.
*@param pParticleSystem the particle system.
*@param pRewindParticle function called on a particle when its life is over.
*@return TRUE if some particles are still alive.
*/
gboolean cairo_dock_update_default_particle_system (CairoParticleSystem *pParticleSystem, CairoDockRewindParticleFunc pRewindParticle);

G_END_DECLS
#endif
