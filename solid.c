/*   
 * Copyright (C) 2003 Robert Kooima
 *
 * NEVERBALL is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */

#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include "glext.h"
#include "vec3.h"
#include "geom.h"
#include "solid.h"
#include "config.h"

#define SMALL 1.0e-10
#define LARGE 1.0e+10

/*---------------------------------------------------------------------------*/

static double erp(double t)
{
    return 3.0 * t * t - 2.0 * t * t * t;
}

static double derp(double t)
{
    return 6.0 * t     - 6.0 * t * t;
}

static void sol_body_v(double v[3],
                       const struct s_file *fp,
                       const struct s_body *bp)
{
    if (bp->pi >= 0 && fp->pv[bp->pi].f)
    {
        const struct s_path *pp = fp->pv + bp->pi;
        const struct s_path *pq = fp->pv + pp->pi;

        v_sub(v, pq->p, pp->p);
        v_scl(v, v, 1.0 / pp->t);

        v_scl(v, v, derp(bp->t / pp->t));
    }
    else
    {
        v[0] = 0.0;
        v[1] = 0.0;
        v[2] = 0.0;
    }
}

static void sol_body_p(double p[3],
                       const struct s_file *fp,
                       const struct s_body *bp)
{
    double v[3];

    if (bp->pi >= 0)
    {
        const struct s_path *pp = fp->pv + bp->pi;
        const struct s_path *pq = fp->pv + pp->pi;

        v_sub(v, pq->p, pp->p);

        v_mad(p, pp->p, v, erp(bp->t / pp->t));
    }
    else
    {
        p[0] = 0.0;
        p[1] = 0.0;
        p[2] = 0.0;
    }
}

/*---------------------------------------------------------------------------*/
/*
 * The  following code  renders a  body in  a  ludicrously inefficient
 * manner.  It iterates the materials and scans the data structure for
 * geometry using each.  This  has the effect of absolutely minimizing
 * material  changes,  texture  bindings,  and  Begin/End  pairs,  but
 * maximizing trips through the data.
 *
 * However, this  is only done once  for each level.   The results are
 * stored in display lists.  Thus, it is well worth it.
 */

static void sol_draw_mtrl(const struct s_file *fp, int i)
{
    const struct s_mtrl *mp = fp->mv + i;

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,   mp->a);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,   mp->d);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  mp->s);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION,  mp->e);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mp->h);

    glBindTexture(GL_TEXTURE_2D, mp->o);
}

static void sol_draw_geom(const struct s_file *fp,
                          const struct s_geom *gp, int mi)
{
    if (gp->mi == mi)
    {
        const double *ui = fp->tv[gp->ti].u;
        const double *uj = fp->tv[gp->tj].u;
        const double *uk = fp->tv[gp->tk].u;

        const double *vi = fp->vv[gp->vi].p;
        const double *vj = fp->vv[gp->vj].p;
        const double *vk = fp->vv[gp->vk].p;

        glNormal3dv(fp->sv[gp->si].n);

#ifdef GL_ARB_multitexture
        if (glMultiTexCoord2dARB)
        {
            glMultiTexCoord2dARB(GL_TEXTURE0_ARB, ui[0], ui[1]);
            glMultiTexCoord2dARB(GL_TEXTURE1_ARB, vi[0], vi[2]);
            glVertex3dv(vi);

            glMultiTexCoord2dARB(GL_TEXTURE0_ARB, uj[0], uj[1]);
            glMultiTexCoord2dARB(GL_TEXTURE1_ARB, vj[0], vj[2]);
            glVertex3dv(vj);

            glMultiTexCoord2dARB(GL_TEXTURE0_ARB, uk[0], uk[1]);
            glMultiTexCoord2dARB(GL_TEXTURE1_ARB, vk[0], vk[2]);
            glVertex3dv(vk);
        }
#else
        glTexCoord2d(ui[0], ui[1]);
        glVertex3dv(vi);

        glTexCoord2d(uj[0], uj[1]);
        glVertex3dv(vj);

        glTexCoord2d(uk[0], uk[1]);
        glVertex3dv(vk);
#endif
    }
}

static void sol_draw_lump(const struct s_file *fp,
                          const struct s_lump *lp, int mi)
{
    int i;

    for (i = 0; i < lp->gc; i++)
    {
        int gi = fp->iv[lp->g0 + i];
        
        sol_draw_geom(fp, fp->gv + gi, mi);
     }
}

static void sol_draw_body(const struct s_file *fp,
                          const struct s_body *bp, int t)
{
    int mi, li;

    /* Iterate all materials of the correct opacity. */

    for (mi = 0; mi < fp->mc; mi++)
        if (t == (fp->mv[mi].d[3] < 0.999) ? 1 : 0)
        {
            /* Set the material state. */

            sol_draw_mtrl(fp, mi);

            /* Render all geometry of that material. */

            glBegin(GL_TRIANGLES);
            {
                for (li = 0; li < bp->lc; li++)
                    sol_draw_lump(fp, fp->lv + bp->l0 + li, mi);
            }
            glEnd();
        }
}

/*---------------------------------------------------------------------------*/

static void sol_draw_list(const struct s_file *fp,
                          const struct s_body *bp, int t)
{
    GLuint l = (t ? bp->tl : bp->ol);
    double p[3];

    sol_body_p(p, fp, bp);

    glPushMatrix();
    {
        /* Translate a moving body. */

        glTranslated(p[0], p[1], p[2]);

        /* Translate the shadow on a moving body. */

#ifdef GL_ARB_multitexture
        if (glActiveTextureARB)
        {
            glActiveTextureARB(GL_TEXTURE1_ARB);
            glMatrixMode(GL_TEXTURE);
            {
                glPushMatrix();
                glTranslated(p[0], p[2], 0.0);
            }
            glMatrixMode(GL_MODELVIEW);
            glActiveTextureARB(GL_TEXTURE0_ARB);
        }
#endif
        
        /* Draw the body. */

        if (glIsList(l))
            glCallList(l);

        /* Pop the shadow translation. */

#ifdef GL_ARB_multitexture
        if (glActiveTextureARB)
        {
            glActiveTextureARB(GL_TEXTURE1_ARB);
            glMatrixMode(GL_TEXTURE);
            {
                glPopMatrix();
            }
            glMatrixMode(GL_MODELVIEW);
            glActiveTextureARB(GL_TEXTURE0_ARB);
        }
#endif
    }
    glPopMatrix();
}


void sol_draw(const struct s_file *fp)
{
    int i;

    glPushAttrib(GL_LIGHTING_BIT);
    glPushAttrib(GL_DEPTH_BUFFER_BIT);
    {
        /* Render all obaque geometry into the color and depth buffers. */

        for (i = 0; i < fp->bc; i++)
            sol_draw_list(fp, fp->bv + i, 0);

        /* Render all translucent geometry into only the color buffer. */

        glDepthMask(GL_FALSE);

        for (i = 0; i < fp->bc; i++)
            sol_draw_list(fp, fp->bv + i, 1);
    }
    glPopAttrib();
    glPopAttrib();
}

/*---------------------------------------------------------------------------*/

static void sol_load_objects(struct s_file *fp)
{
    int i;

    for (i = 0; i < fp->bc; i++)
    {
        fp->bv[i].ol = glGenLists(1);
        fp->bv[i].tl = glGenLists(1);

        /* Draw all opaque geometry. */

        glNewList(fp->bv[i].ol, GL_COMPILE);
        {
            sol_draw_body(fp, fp->bv + i, 0);
        }
        glEndList();

        /* Draw all translucent geometry. */

        glNewList(fp->bv[i].tl, GL_COMPILE);
        {
            sol_draw_body(fp, fp->bv + i, 1);
        }
        glEndList();
    }
}

static void sol_load_textures(struct s_file *fp, int n)
{
    SDL_Surface *s;
    SDL_Surface *d;
    char tga[64];
    char jpg[64];

    int i;

    for (i = 0; i < fp->mc; i++)
    {
        strncpy(tga, fp->mv[i].f, PATHMAX);
        strcat(tga, ".tga");
        strncpy(jpg, fp->mv[i].f, PATHMAX);
        strcat(jpg, ".jpg");

        /* Prefer a lossless copy of the texture over a lossy compression. */

        if ((s = IMG_Load(tga)) || (s = IMG_Load(jpg)))
        {
            GLenum f0 = (s->format->BitsPerPixel == 32) ? GL_RGBA : GL_RGB;
            GLenum f1 = (s->format->BitsPerPixel == 32) ? GL_BGRA : GL_RGB;

            glGenTextures(1, &fp->mv[i].o);
            glBindTexture(GL_TEXTURE_2D, fp->mv[i].o);

            if (n > 1)
            {
                /* Create a new buffer and copy the scaled image to it. */

                d = SDL_CreateRGBSurface(SDL_SWSURFACE, s->w / n, s->h / n,
                                         s->format->BitsPerPixel,
                                         RMASK, GMASK, BMASK, AMASK);
                if (d)
                {
                    SDL_LockSurface(s);
                    SDL_LockSurface(d);
                    {
                        gluScaleImage(f1,
                                      s->w, s->h, GL_UNSIGNED_BYTE, s->pixels,
                                      d->w, d->h, GL_UNSIGNED_BYTE, d->pixels);
                    }
                    SDL_UnlockSurface(d);
                    SDL_UnlockSurface(s);

                    /* Load the scaled image. */

                    glTexImage2D(GL_TEXTURE_2D, 0, f0, d->w, d->h,
                                 0, f1, GL_UNSIGNED_BYTE, d->pixels);

                    SDL_FreeSurface(d);
                }
            }
            else
            {
                /* Load the unscaled image. */

                glTexImage2D(GL_TEXTURE_2D, 0, f0, s->w, s->h,
                             0, f1, GL_UNSIGNED_BYTE, s->pixels);
            }

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            SDL_FreeSurface(s);
        }
    }
}

int sol_load(struct s_file *fp, const char *filename, int scale)
{
    FILE *fin;

    if ((fin = fopen(filename, FMODE_RB)))
    {
        int n[18];

        memset(fp, 0, sizeof (struct s_file));

        fread(n, sizeof (int), 18, fin);

        fp->mc = n[0];
        fp->vc = n[1];
        fp->ec = n[2];
        fp->sc = n[3];
        fp->tc = n[4];
        fp->gc = n[5];
        fp->lc = n[6];
        fp->nc = n[7];
        fp->pc = n[8];
        fp->bc = n[9];
        fp->cc = n[10];
        fp->zc = n[11];
        fp->jc = n[12];
        fp->xc = n[13];
        fp->uc = n[14];
        fp->wc = n[15];
        fp->ac = n[16];
        fp->ic = n[17];

        fp->mv = (struct s_mtrl *) calloc(n[0],  sizeof (struct s_mtrl));
        fp->vv = (struct s_vert *) calloc(n[1],  sizeof (struct s_vert));
        fp->ev = (struct s_edge *) calloc(n[2],  sizeof (struct s_edge));
        fp->sv = (struct s_side *) calloc(n[3],  sizeof (struct s_side));
        fp->tv = (struct s_texc *) calloc(n[4],  sizeof (struct s_texc));
        fp->gv = (struct s_geom *) calloc(n[5],  sizeof (struct s_geom));
        fp->lv = (struct s_lump *) calloc(n[6],  sizeof (struct s_lump));
        fp->nv = (struct s_node *) calloc(n[7],  sizeof (struct s_node));
        fp->pv = (struct s_path *) calloc(n[8],  sizeof (struct s_path));
        fp->bv = (struct s_body *) calloc(n[9],  sizeof (struct s_body));
        fp->cv = (struct s_coin *) calloc(n[10], sizeof (struct s_coin));
        fp->zv = (struct s_goal *) calloc(n[11], sizeof (struct s_goal));
        fp->jv = (struct s_jump *) calloc(n[12], sizeof (struct s_jump));
        fp->xv = (struct s_swch *) calloc(n[13], sizeof (struct s_swch));
        fp->uv = (struct s_ball *) calloc(n[14], sizeof (struct s_ball));
        fp->wv = (struct s_view *) calloc(n[15], sizeof (struct s_view));
        fp->av = (char          *) calloc(n[16], sizeof (char));
        fp->iv = (int           *) calloc(n[17], sizeof (int));

        fread(fp->mv, sizeof (struct s_mtrl), n[0],  fin);
        fread(fp->vv, sizeof (struct s_vert), n[1],  fin);
        fread(fp->ev, sizeof (struct s_edge), n[2],  fin);
        fread(fp->sv, sizeof (struct s_side), n[3],  fin);
        fread(fp->tv, sizeof (struct s_texc), n[4],  fin);
        fread(fp->gv, sizeof (struct s_geom), n[5],  fin);
        fread(fp->lv, sizeof (struct s_lump), n[6],  fin);
        fread(fp->nv, sizeof (struct s_node), n[7],  fin);
        fread(fp->pv, sizeof (struct s_path), n[8],  fin);
        fread(fp->bv, sizeof (struct s_body), n[9],  fin);
        fread(fp->cv, sizeof (struct s_coin), n[10], fin);
        fread(fp->zv, sizeof (struct s_goal), n[11], fin);
        fread(fp->jv, sizeof (struct s_jump), n[12], fin);
        fread(fp->xv, sizeof (struct s_swch), n[13], fin);
        fread(fp->uv, sizeof (struct s_ball), n[14], fin);
        fread(fp->wv, sizeof (struct s_view), n[15], fin);
        fread(fp->av, sizeof (char),          n[16], fin);
        fread(fp->iv, sizeof (int),           n[17], fin);

        fclose(fin);

        sol_load_textures(fp, scale);
        sol_load_objects(fp);

        return 1;
    }
    return 0;
}

int sol_stor(struct s_file *fp, const char *filename)
{
    FILE *fout;

    if ((fout = fopen(filename, FMODE_WB)))
    {
        int n[18];

        n[0]  = fp->mc;
        n[1]  = fp->vc;
        n[2]  = fp->ec;
        n[3]  = fp->sc;
        n[4]  = fp->tc;
        n[5]  = fp->gc;
        n[6]  = fp->lc;
        n[7]  = fp->nc;
        n[8]  = fp->pc;
        n[9]  = fp->bc;
        n[10] = fp->cc;
        n[11] = fp->zc;
        n[12] = fp->jc;
        n[13] = fp->xc;
        n[14] = fp->uc;
        n[15] = fp->wc;
        n[16] = fp->ac;
        n[17] = fp->ic;

        fwrite(n, sizeof (int), 18, fout);

        fwrite(fp->mv, sizeof (struct s_mtrl), n[0],  fout);
        fwrite(fp->vv, sizeof (struct s_vert), n[1],  fout);
        fwrite(fp->ev, sizeof (struct s_edge), n[2],  fout);
        fwrite(fp->sv, sizeof (struct s_side), n[3],  fout);
        fwrite(fp->tv, sizeof (struct s_texc), n[4],  fout);
        fwrite(fp->gv, sizeof (struct s_geom), n[5],  fout);
        fwrite(fp->lv, sizeof (struct s_lump), n[6],  fout);
        fwrite(fp->nv, sizeof (struct s_node), n[7],  fout);
        fwrite(fp->pv, sizeof (struct s_path), n[8],  fout);
        fwrite(fp->bv, sizeof (struct s_body), n[9],  fout);
        fwrite(fp->cv, sizeof (struct s_coin), n[10], fout);
        fwrite(fp->zv, sizeof (struct s_goal), n[11], fout);
        fwrite(fp->jv, sizeof (struct s_jump), n[12], fout);
        fwrite(fp->xv, sizeof (struct s_swch), n[13], fout);
        fwrite(fp->uv, sizeof (struct s_ball), n[14], fout);
        fwrite(fp->wv, sizeof (struct s_view), n[15], fout);
        fwrite(fp->av, sizeof (char),          n[16], fout);
        fwrite(fp->iv, sizeof (int),           n[17], fout);

        fclose(fout);

        return 1;
    }
    return 0;
}

void sol_free(struct s_file *fp)
{
    int i;

    for (i = 0; i < fp->mc; i++)
    {
        if (glIsTexture(fp->mv[i].o))
            glDeleteTextures(1, &fp->mv[i].o);
    }

    for (i = 0; i < fp->bc; i++)
    {
        if (glIsList(fp->bv[i].ol))
            glDeleteLists(fp->bv[i].ol, 1);
        if (glIsList(fp->bv[i].tl))
            glDeleteLists(fp->bv[i].tl, 1);
    }

    if (fp->mv) free(fp->mv);
    if (fp->vv) free(fp->vv);
    if (fp->ev) free(fp->ev);
    if (fp->sv) free(fp->sv);
    if (fp->tv) free(fp->tv);
    if (fp->gv) free(fp->gv);
    if (fp->lv) free(fp->lv);
    if (fp->nv) free(fp->nv);
    if (fp->pv) free(fp->pv);
    if (fp->bv) free(fp->bv);
    if (fp->cv) free(fp->cv);
    if (fp->zv) free(fp->zv);
    if (fp->jv) free(fp->jv);
    if (fp->xv) free(fp->xv);
    if (fp->uv) free(fp->uv);
    if (fp->wv) free(fp->wv);
    if (fp->av) free(fp->av);
    if (fp->iv) free(fp->iv);

    memset(fp, 0, sizeof (struct s_file));
}

/*---------------------------------------------------------------------------*/
/* Solves (p + v * t) . (p + v * t) == r * r for smallest t.                 */

static double v_sol(const double p[3], const double v[3], double r)
{
    double a = v_dot(v, v);
    double b = v_dot(v, p) * 2.0;
    double c = v_dot(p, p) - r * r;
    double d = b * b - 4.0 * a * c;

    if (a == 0.0) return LARGE;
    if (d <  0.0) return LARGE;

    if (d == 0.0)
        return -b * 0.5 / a;
    else
    {
        double t0 = 0.5 * (-b - sqrt(d)) / a;
        double t1 = 0.5 * (-b + sqrt(d)) / a;
        double t  = (t0 < t1) ? t0 : t1;

        return (t < 0.0) ? LARGE : t;
    }
}

/*---------------------------------------------------------------------------*/

/*
 * Compute the  earliest time  and position of  the intersection  of a
 * sphere and a vertex.
 *
 * The sphere has radius R and moves along vector V from point P.  The
 * vertex moves  along vector  W from point  Q in a  coordinate system
 * based at O.
 */
static double v_vert(double Q[3],
                     const double o[3],
                     const double q[3],
                     const double w[3],
                     const double p[3],
                     const double v[3], double r)
{
    double O[3], P[3], V[3];
    double t = LARGE;

    v_add(O, o, q);
    v_sub(P, p, O);
    v_sub(V, v, w);

    if (v_dot(P, V) < 0.0)
    {
        t = v_sol(P, V, r);

        if (t < LARGE)
            v_mad(Q, O, w, t);
    }
    return t;
}

/*
 * Compute the  earliest time  and position of  the intersection  of a
 * sphere and an edge.
 *
 * The sphere has radius R and moves along vector V from point P.  The
 * edge moves along vector W from point Q in a coordinate system based
 * at O.  The edge extends along the length of vector U.
 */
static double v_edge(double Q[3],
                     const double o[3],
                     const double q[3],
                     const double u[3],
                     const double w[3],
                     const double p[3],
                     const double v[3], double r)
{
    double d[3], e[3];
    double P[3], V[3];
    double du, eu, uu, s, t;

    v_sub(d, p, o);
    v_sub(d, d, q);
    v_sub(e, v, w);

    du = v_dot(d, u);
    eu = v_dot(e, u);
    uu = v_dot(u, u);

    v_mad(P, d, u, -du / uu);
    v_mad(V, e, u, -eu / uu);

    t = v_sol(P, V, r);
    s = (du + eu * t) / uu;

    if (0.0 < t && t < LARGE && 0.0 < s && s < 1.0)
    {
        v_mad(d, o, w, t);
        v_mad(e, q, u, s);
        v_add(Q, e, d);
    }
    else
        t = LARGE;

    return t;
}

/*
 * Compute  the earlist  time and  position of  the intersection  of a
 * sphere and a plane.
 *
 * The sphere has radius R and moves along vector V from point P.  The
 * plane  oves  along  vector  W.   The  plane has  normal  N  and  is
 * positioned at distance D from the origin O along that normal.
 */
static double v_side(double Q[3],
                     const double o[3],
                     const double w[3],
                     const double n[3], double d,
                     const double p[3],
                     const double v[3], double r)
{
    double on = v_dot(o, n);
    double pn = v_dot(p, n);
    double vn = v_dot(v, n);
    double wn = v_dot(w, n);
    double t  = LARGE;

    if (vn - wn > 0.0)
        return LARGE;
    else
    {
        double u = (r + d + on - pn) / (vn - wn);
        double a = (    d + on - pn) / (vn - wn);

        if (0.0 <= u)
        {
            t = u;

            v_mad(Q, p, v, +t);
            v_mad(Q, Q, n, -r);
        }
        else if (0.0 <= a)
        {
            t = 0;

            v_mad(Q, p, v, +t);
            v_mad(Q, Q, n, -r);
        }
    }
    return t;
}

/*---------------------------------------------------------------------------*/

/*
 * Compute the new  linear and angular velocities of  a bouncing ball.
 * Q  gives the  position  of the  point  of impact  and  W gives  the
 * velocity of the object being impacted.
 */
static double sol_bounce(struct s_ball *up,
                         const double q[3],
                         const double w[3], double dt)
{
    const double kb = 1.10;
    const double ke = 0.70;
    const double km = 0.20;

    double n[3], r[3], d[3], u[3], vn, wn, xn, yn;
    double *p = up->p;
    double *v = up->v;

    /* Find the normal of the impact. */

    v_sub(r, p, q);
    v_sub(d, v, w);
    v_nrm(n, r);

    /* Find the new angular velocity. */

    v_crs(up->w, d, r);
    v_scl(up->w, up->w, -1.0 / (up->r * up->r));

    /* Find the new linear velocity. */

    vn = v_dot(v, n);
    wn = v_dot(w, n);
    xn = (vn < 0.0) ? -vn * ke : vn;
    yn = (wn > 0.0) ?  wn * kb : wn;

    v_mad(u, w, n, -wn);
    v_mad(v, v, n, -vn);
    v_mad(v, v, u, +km * dt);
    v_mad(v, v, n, xn + yn); 

    v_mad(p, q, n, up->r);

    /* Return the "energy" of the impact, to determine the sound amplitude. */

    return fabs(v_dot(n, d));
}

/*---------------------------------------------------------------------------*/

/*
 * Compute the states of all switches after DT seconds have passed.
 */
static void sol_swch_step(struct s_file *fp, double dt)
{
    int xi;

    for (xi = 0; xi < fp->xc; xi++)
    {
        struct s_swch *xp = fp->xv + xi;

        if (xp->t > 0)
        {
            xp->t -= dt;

            if (xp->t <= 0)
            {
                int pi = xp->pi;
                int pj = xp->pi;

                do  /* Tortoise and hare cycle traverser. */
                {
                    fp->pv[pi].f = xp->f0;
                    fp->pv[pj].f = xp->f0;

                    pi = fp->pv[pi].pi;
                    pj = fp->pv[pj].pi;
                    pj = fp->pv[pj].pi;
                }
                while (pi != pj);

                xp->f = xp->f0;
            }
        }
    }
}

/*
 * Compute the positions of all bodies after DT seconds have passed.
 */
static void sol_body_step(struct s_file *fp, double dt)
{
    int i;

    for (i = 0; i < fp->bc; i++)
    {
        struct s_body *bp = fp->bv + i;
        struct s_path *pp = fp->pv + bp->pi;

        if (bp->pi >= 0 && pp->f)
        {
            bp->t += dt;

            if (bp->t > pp->t)
            {
                bp->t -= pp->t;
                bp->pi = pp->pi;
            }
        }
    }
}

/*
 * Compute the positions of all balls after DT seconds have passed.
 */
static void sol_ball_step(struct s_file *fp, double dt)
{
    int i;

    for (i = 0; i < fp->uc; i++)
    {
        struct s_ball *up = fp->uv + i;

        v_mad(up->p, up->p, up->v, dt);

        if (v_len(up->w) > 0.0)
        {
            double M[16];
            double w[3];
            double e[3][3];

            v_nrm(w, up->w);
            m_rot(M, w, v_len(up->w) * dt);

            m_vxfm(e[0], M, up->e[0]);
            m_vxfm(e[1], M, up->e[1]);
            m_vxfm(e[2], M, up->e[2]);

            v_crs(up->e[2], e[0], e[1]);
            v_crs(up->e[1], e[2], e[0]);
            v_crs(up->e[0], e[1], e[2]);

            v_nrm(up->e[0], up->e[0]);
            v_nrm(up->e[1], up->e[1]);
            v_nrm(up->e[2], up->e[2]);
        }
    }
}

/*---------------------------------------------------------------------------*/

static double sol_test_vert(double T[3],
                            const struct s_ball *up,
                            const struct s_vert *vp,
                            const double o[3],
                            const double w[3])
{
    return v_vert(T, o, vp->p, w, up->p, up->v, up->r);
}

static double sol_test_edge(double T[3],
                            const struct s_ball *up,
                            const struct s_file *fp,
                            const struct s_edge *ep,
                            const double o[3],
                            const double w[3])
{
    double q[3];
    double u[3];

    v_cpy(q, fp->vv[ep->vi].p);
    v_sub(u, fp->vv[ep->vj].p,
             fp->vv[ep->vi].p);

    return v_edge(T, o, q, u, w, up->p, up->v, up->r);
}

static double sol_test_side(double T[3],
                            const struct s_ball *up,
                            const struct s_file *fp,
                            const struct s_lump *lp,
                            const struct s_side *sp,
                            const double o[3],
                            const double w[3])
{
    double t = v_side(T, o, w, sp->n, sp->d, up->p, up->v, up->r);
    int i;

    if (t < LARGE)
        for (i = 0; i < lp->sc; i++)
        {
            const struct s_side *sq = fp->sv + fp->iv[lp->s0 + i];

            if (sp != sq &&
                v_dot(T, sq->n) -
                v_dot(o, sq->n) -
                v_dot(w, sq->n) * t > sq->d)
                return LARGE;
        }
    return t;
}

/*---------------------------------------------------------------------------*/

static double sol_test_fore(const struct s_ball *up,
                            const struct s_side *sp,
                            const double o[3])
{
    double q[3];

    /* If the ball is not behind the plane, the test passes. */

    v_sub(q, up->p, o);

    if (v_dot(q, sp->n) - sp->d + up->r >= 0)
        return 1;

    /* If the ball is behind but moving toward the plane, test passes. */

    if (v_dot(up->v, sp->n) > 0)
        return 1;

    /* Else, test fails. */

    return 0;
}

static double sol_test_back(const struct s_ball *up,
                            const struct s_side *sp,
                            const double o[3])
{
    double q[3];

    /* If the ball is not in front of the plane, the test passes. */

    v_sub(q, up->p, o);

    if (v_dot(q, sp->n) - sp->d - up->r <= 0)
        return 1;

    /* If the ball is in front but moving toward the plane, test passes. */

    if (v_dot(up->v, sp->n) < 0)
        return 1;

    /* Else, test fails. */

    return 0;
}

/*---------------------------------------------------------------------------*/

static double sol_test_lump(double T[3],
                            const struct s_ball *up,
                            const struct s_file *fp,
                            const struct s_lump *lp,
                            const double o[3],
                            const double w[3])
{
    double U[3], u, t = LARGE;
    int i;

    /* Short circuit a non-solid lump. */

    if (lp->fl & 1) return t;

    /* Test all verts */

    if (up->r > 0.0)
        for (i = 0; i < lp->vc; i++)
        {
            const struct s_vert *vp = fp->vv + fp->iv[lp->v0 + i];

            if ((u = sol_test_vert(U, up, vp, o, w)) < t)
            {
                v_cpy(T, U);
                t = u;
            }
        }
 
   /* Test all edges */

    if (up->r > 0.0)
        for (i = 0; i < lp->ec; i++)
        {
            const struct s_edge *ep = fp->ev + fp->iv[lp->e0 + i];

            if ((u = sol_test_edge(U, up, fp, ep, o, w)) < t)
            {
                v_cpy(T, U);
                t = u;
            }
        }

    /* Test all sides */

    for (i = 0; i < lp->sc; i++)
    {
        const struct s_side *sp = fp->sv + fp->iv[lp->s0 + i];

        if ((u = sol_test_side(U, up, fp, lp, sp, o, w)) < t)
        {
            v_cpy(T, U);
            t = u;
        }
    }
    return t;
}

static double sol_test_node(double T[3],
                            const struct s_ball *up,
                            const struct s_file *fp,
                            const struct s_node *np,
                            const double o[3],
                            const double w[3])
{
    double U[3], u, t = LARGE;
    int i;

    /* Test all lumps */

    for (i = 0; i < np->lc; i++)
    {
        const struct s_lump *lp = fp->lv + np->l0 + i;

        if ((u = sol_test_lump(U, up, fp, lp, o, w)) < t)
        {
            v_cpy(T, U);
            t = u;
        }
    }

    /* Test in front of this node */

    if (np->ni >= 0 && sol_test_fore(up, fp->sv + np->si, o))
    {
        const struct s_node *nq = fp->nv + np->ni;

        if ((u = sol_test_node(U, up, fp, nq, o, w)) < t)
        {
            v_cpy(T, U);
            t = u;
        }
    }

    /* Test behind this node */

    if (np->nj >= 0 && sol_test_back(up, fp->sv + np->si, o))
    {
        const struct s_node *nq = fp->nv + np->nj;

        if ((u = sol_test_node(U, up, fp, nq, o, w)) < t)
        {
            v_cpy(T, U);
            t = u;
        }
    }

    return t;
}

static double sol_test_body(double T[3], double V[3],
                            const struct s_ball *up,
                            const struct s_file *fp,
                            const struct s_body *bp)
{
    double U[3], O[3], W[3], u, t = LARGE;

    sol_body_p(O, fp, bp);
    sol_body_v(W, fp, bp);

    const struct s_node *np = fp->nv + bp->ni;

    if ((u = sol_test_node(U, up, fp, np, O, W)) < t)
    {
        v_cpy(T, U);
        v_cpy(V, W);
        t = u;
    }
    return t;
}

static double sol_test_file(double T[3], double V[3],
                            const struct s_ball *up,
                            const struct s_file *fp)
{
    double U[3], W[3], u, t = LARGE;
    int i;

    for (i = 0; i < fp->bc; i++)
    {
        const struct s_body *bp = fp->bv + i;

        if ((u = sol_test_body(U, W, up, fp, bp)) < t)
        {
            v_cpy(T, U);
            v_cpy(V, W);
            t = u;
        }
    }
    return t;
}

/*---------------------------------------------------------------------------*/

double sol_step(struct s_file *fp, const double *g, double dt)
{
    double T[3], V[3], d, nt, b = 0.0, tt = dt;
    
    struct s_ball *up = fp->uv;

    v_mad(up->v, up->v, g, tt);

    while (tt > 0 && tt >= (nt = sol_test_file(T, V, up, fp)))
        if (fabs(nt) >= 0)
        {
            sol_swch_step(fp, nt);
            sol_body_step(fp, nt);
            sol_ball_step(fp, nt);

            tt -= nt;

            if (b < (d = sol_bounce(up, T, V, nt)))
                b = d;
        }

    sol_swch_step(fp, tt);
    sol_body_step(fp, tt);
    sol_ball_step(fp, tt);

    return b;
}

/*---------------------------------------------------------------------------*/

int sol_coin_test(struct s_file *fp, double *p, double coin_r)
{
    const double *ball_p = fp->uv->p;
    const double  ball_r = fp->uv->r;
    int ci, n;

    for (ci = 0; ci < fp->cc; ci++)
    {
        double r[3];

        r[0] = ball_p[0] - fp->cv[ci].p[0];
        r[1] = ball_p[1] - fp->cv[ci].p[1];
        r[2] = ball_p[2] - fp->cv[ci].p[2];

        if (fp->cv[ci].n > 0 && v_len(r) < ball_r + coin_r)
        {
            p[0] = fp->cv[ci].p[0];
            p[1] = fp->cv[ci].p[1];
            p[2] = fp->cv[ci].p[2];

            n = fp->cv[ci].n;
            fp->cv[ci].n = 0;

            return n;
        }
    }
    return 0;
}

int sol_goal_test(struct s_file *fp, double *p)
{
    const double *ball_p = fp->uv->p;
    const double  ball_r = fp->uv->r;
    int zi;

    for (zi = 0; zi < fp->zc; zi++)
    {
        double r[3];

        r[0] = ball_p[0] - fp->zv[zi].p[0];
        r[1] = ball_p[2] - fp->zv[zi].p[2];
        r[2] = 0;

        if (v_len(r) < fp->zv[zi].r - ball_r &&
            ball_p[1] > fp->zv[zi].p[1] &&
            ball_p[1] < fp->zv[zi].p[1] + GOAL_HEIGHT / 2)
        {
            p[0] = fp->zv[zi].p[0];
            p[1] = fp->zv[zi].p[1];
            p[2] = fp->zv[zi].p[2];

            return 1;
        }
    }
    return 0;
}

int sol_jump_test(struct s_file *fp, double *p)
{
    const double *ball_p = fp->uv->p;
    const double  ball_r = fp->uv->r;
    int ji;

    for (ji = 0; ji < fp->jc; ji++)
    {
        double r[3];

        r[0] = ball_p[0] - fp->jv[ji].p[0];
        r[1] = ball_p[2] - fp->jv[ji].p[2];
        r[2] = 0;

        if (v_len(r) < fp->jv[ji].r - ball_r &&
            ball_p[1] > fp->jv[ji].p[1] &&
            ball_p[1] < fp->jv[ji].p[1] + JUMP_HEIGHT / 2)
        {
            p[0] = fp->jv[ji].q[0] + (ball_p[0] - fp->jv[ji].p[0]);
            p[1] = fp->jv[ji].q[1] + (ball_p[1] - fp->jv[ji].p[1]);
            p[2] = fp->jv[ji].q[2] + (ball_p[2] - fp->jv[ji].p[2]);

            return 1;
        }
    }
    return 0;
}

int sol_swch_test(struct s_file *fp, int flag)
{
    const double *ball_p = fp->uv->p;
    const double  ball_r = fp->uv->r;
    int xi;

    for (xi = 0; xi < fp->xc; xi++)
    {
        struct s_swch *xp = fp->xv + xi;

        if (xp->t0 == 0 || xp->f == xp->f0)
        {
            double r[3];

            r[0] = ball_p[0] - xp->p[0];
            r[1] = ball_p[2] - xp->p[2];
            r[2] = 0;

            if (v_len(r)  < xp->r - ball_r &&
                ball_p[1] > xp->p[1] &&
                ball_p[1] < xp->p[1] + SWCH_HEIGHT / 2)
            {
                if (flag)
                {
                    int pi = xp->pi;
                    int pj = xp->pi;

                    /* Toggle the state, update the path. */

                    xp->f = xp->f ? 0 : 1;

                    do  /* Tortoise and hare cycle traverser. */
                    {
                        fp->pv[pi].f = xp->f;
                        fp->pv[pj].f = xp->f;

                        pi = fp->pv[pi].pi;
                        pj = fp->pv[pj].pi;
                        pj = fp->pv[pj].pi;
                    }
                    while (pi != pj);

                    /* It toggled to non-default state, start the timer. */

                    if (xp->f != xp->f0)
                        xp->t  = xp->t0;
                }
                return 0;
            }
        }
    }
    return 1;
}

/*---------------------------------------------------------------------------*/

int double_put(FILE *fout, double *d)
{
    return (fwrite(d, sizeof (double), 1, fout) == 1) ? 1 : 0;
}

int double_get(FILE *fin, double *d)
{
    if (fread(d, sizeof (double), 1, fin) == 1)
        return 1;
    else
        return 0;
}

/*---------------------------------------------------------------------------*/

int vector_put(FILE *fout, double v[3])
{
    return (double_put(fout, v + 0) &&
            double_put(fout, v + 1) &&
            double_put(fout, v + 2));
}

int vector_get(FILE *fin, double v[3])
{
    return (double_get(fin, v + 0) &&
            double_get(fin, v + 1) &&
            double_get(fin, v + 2));
}

int sol_put(FILE *fout, struct s_file *fp)
{
    return (vector_put(fout, fp->uv[0].p)    &&
            vector_put(fout, fp->uv[0].e[0]) &&
            vector_put(fout, fp->uv[0].e[1]));
}

int sol_get(FILE *fin, struct s_file *fp)
{
    if (vector_get(fin, fp->uv[0].p)    &&
        vector_get(fin, fp->uv[0].e[0]) &&
        vector_get(fin, fp->uv[0].e[1]))
    {
        v_crs(fp->uv[0].e[2], fp->uv[0].e[0], fp->uv[0].e[1]);
        return 1;
    }
    return 0;
}

/*---------------------------------------------------------------------------*/

