/***
 * libgjk
 * ---------------------------------
 * Copyright (c)2010 Daniel Fiser <danfis@danfis.cz>
 *
 *
 *  This file is part of libgjk.
 *
 *  Distributed under the OSI-approved BSD License (the "License");
 *  see accompanying file BDS-LICENSE for details or see
 *  <http://www.opensource.org/licenses/bsd-license.php>.
 *
 *  This software is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the License for more information.
 */

#include <stdio.h>
#include <float.h>
#include "polytope.h"
#include "alloc.h"

_gjk_inline void _gjkPtNearestUpdate(gjk_pt_t *pt, gjk_pt_el_t *el)
{
    if (gjkEq(pt->nearest_dist, el->dist)){
        if (el->type < pt->nearest_type){
            pt->nearest = el;
            pt->nearest_dist = el->dist;
            pt->nearest_type = el->type;
        }
    }else if (el->dist < pt->nearest_dist){
        pt->nearest = el;
        pt->nearest_dist = el->dist;
        pt->nearest_type = el->type;
    }
}

static void _gjkPtNearestRenew(gjk_pt_t *pt)
{
    gjk_pt_vertex_t *v;
    gjk_pt_edge_t *e;
    gjk_pt_face_t *f;

    pt->nearest_dist = GJK_REAL_MAX;
    pt->nearest_type = 3;
    pt->nearest = NULL;

    gjkListForEachEntry(&pt->vertices, v, list){
        _gjkPtNearestUpdate(pt, (gjk_pt_el_t *)v);
    }

    gjkListForEachEntry(&pt->edges, e, list){
        _gjkPtNearestUpdate(pt, (gjk_pt_el_t *)e);
    }

    gjkListForEachEntry(&pt->faces, f, list){
        _gjkPtNearestUpdate(pt, (gjk_pt_el_t *)f);
    }
}



void gjkPtInit(gjk_pt_t *pt)
{
    gjkListInit(&pt->vertices);
    gjkListInit(&pt->edges);
    gjkListInit(&pt->faces);

    pt->nearest = NULL;
    pt->nearest_dist = GJK_REAL_MAX;
    pt->nearest_type = 3;
}

void gjkPtDestroy(gjk_pt_t *pt)
{
    gjk_pt_face_t *f, *f2;
    gjk_pt_edge_t *e, *e2;
    gjk_pt_vertex_t *v, *v2;

    // first delete all faces
    gjkListForEachEntrySafe(&pt->faces, f, f2, list){
        gjkPtDelFace(pt, f);
    }

    // delete all edges
    gjkListForEachEntrySafe(&pt->edges, e, e2, list){
        gjkPtDelEdge(pt, e);
    }

    // delete all vertices
    gjkListForEachEntrySafe(&pt->vertices, v, v2, list){
        gjkPtDelVertex(pt, v);
    }
}


gjk_pt_vertex_t *gjkPtAddVertex(gjk_pt_t *pt, const gjk_support_t *v)
{
    gjk_pt_vertex_t *vert;

    vert = GJK_ALLOC(gjk_pt_vertex_t);
    vert->type = GJK_PT_VERTEX;
    gjkSupportCopy(&vert->v, v);

    vert->dist = gjkVec3Len2(&vert->v.v);
    gjkVec3Copy(&vert->witness, &vert->v.v);

    gjkListInit(&vert->edges);

    // add vertex to list
    gjkListAppend(&pt->vertices, &vert->list);

    // update position in .nearest array
    _gjkPtNearestUpdate(pt, (gjk_pt_el_t *)vert);

    return vert;
}

gjk_pt_edge_t *gjkPtAddEdge(gjk_pt_t *pt, gjk_pt_vertex_t *v1,
                                          gjk_pt_vertex_t *v2)
{
    const gjk_vec3_t *a, *b;
    gjk_pt_edge_t *edge;

    edge = GJK_ALLOC(gjk_pt_edge_t);
    edge->type = GJK_PT_EDGE;
    edge->vertex[0] = v1;
    edge->vertex[1] = v2;
    edge->faces[0] = edge->faces[1] = NULL;

    a = &edge->vertex[0]->v.v;
    b = &edge->vertex[1]->v.v;
    edge->dist = gjkVec3PointSegmentDist2(gjk_vec3_origin, a, b, &edge->witness);

    gjkListAppend(&edge->vertex[0]->edges, &edge->vertex_list[0]);
    gjkListAppend(&edge->vertex[1]->edges, &edge->vertex_list[1]);

    gjkListAppend(&pt->edges, &edge->list);

    // update position in .nearest array
    _gjkPtNearestUpdate(pt, (gjk_pt_el_t *)edge);

    return edge;
}

gjk_pt_face_t *gjkPtAddFace(gjk_pt_t *pt, gjk_pt_edge_t *e1,
                                          gjk_pt_edge_t *e2,
                                          gjk_pt_edge_t *e3)
{
    const gjk_vec3_t *a, *b, *c;
    gjk_pt_face_t *face;
    gjk_pt_edge_t *e;
    size_t i;

    face = GJK_ALLOC(gjk_pt_face_t);
    face->type = GJK_PT_FACE;
    face->edge[0] = e1;
    face->edge[1] = e2;
    face->edge[2] = e3;

    // obtain triplet of vertices
    a = &face->edge[0]->vertex[0]->v.v;
    b = &face->edge[0]->vertex[1]->v.v;
    e = face->edge[1];
    if (e->vertex[0] != face->edge[0]->vertex[0]
            && e->vertex[0] != face->edge[0]->vertex[1]){
        c = &e->vertex[0]->v.v;
    }else{
        c = &e->vertex[1]->v.v;
    }
    face->dist = gjkVec3PointTriDist2(gjk_vec3_origin, a, b, c, &face->witness);


    for (i = 0; i < 3; i++){
        if (face->edge[i]->faces[0] == NULL){
            face->edge[i]->faces[0] = face;
        }else{
            face->edge[i]->faces[1] = face;
        }
    }

    gjkListAppend(&pt->faces, &face->list);

    // update position in .nearest array
    _gjkPtNearestUpdate(pt, (gjk_pt_el_t *)face);

    return face;
}


void gjkPtRecomputeDistances(gjk_pt_t *pt)
{
    gjk_pt_vertex_t *v;
    gjk_pt_edge_t *e;
    gjk_pt_face_t *f;
    const gjk_vec3_t *a, *b, *c;
    gjk_real_t dist;

    gjkListForEachEntry(&pt->vertices, v, list){
        dist = gjkVec3Len2(&v->v.v);
        v->dist = dist;
        gjkVec3Copy(&v->witness, &v->v.v);
    }

    gjkListForEachEntry(&pt->edges, e, list){
        a = &e->vertex[0]->v.v;
        b = &e->vertex[1]->v.v;
        dist = gjkVec3PointSegmentDist2(gjk_vec3_origin, a, b, &e->witness);
        e->dist = dist;
    }

    gjkListForEachEntry(&pt->faces, f, list){
        // obtain triplet of vertices
        a = &f->edge[0]->vertex[0]->v.v;
        b = &f->edge[0]->vertex[1]->v.v;
        e = f->edge[1];
        if (e->vertex[0] != f->edge[0]->vertex[0]
                && e->vertex[0] != f->edge[0]->vertex[1]){
            c = &e->vertex[0]->v.v;
        }else{
            c = &e->vertex[1]->v.v;
        }

        dist = gjkVec3PointTriDist2(gjk_vec3_origin, a, b, c, &f->witness);
        f->dist = dist;
    }
}

gjk_pt_el_t *gjkPtNearest(gjk_pt_t *pt)
{
    if (!pt->nearest){
        _gjkPtNearestRenew(pt);
    }
    return pt->nearest;
}


void gjkPtDumpSVT(gjk_pt_t *pt, const char *fn)
{
    FILE *fout;

    fout = fopen(fn, "a");
    if (fout == NULL)
        return;

    gjkPtDumpSVT2(pt, fout);

    fclose(fout);
}

void gjkPtDumpSVT2(gjk_pt_t *pt, FILE *fout)
{
    gjk_pt_vertex_t *v, *a, *b, *c;
    gjk_pt_edge_t *e;
    gjk_pt_face_t *f;
    size_t i;

    fprintf(fout, "-----\n");

    fprintf(fout, "Points:\n");
    i = 0;
    gjkListForEachEntry(&pt->vertices, v, list){
        v->id = i++;
        fprintf(fout, "%lf %lf %lf\n",
                gjkVec3X(&v->v.v), gjkVec3Y(&v->v.v), gjkVec3Z(&v->v.v));
    }

    fprintf(fout, "Edges:\n");
    gjkListForEachEntry(&pt->edges, e, list){
        fprintf(fout, "%d %d\n", e->vertex[0]->id, e->vertex[1]->id);
    }

    fprintf(fout, "Faces:\n");
    gjkListForEachEntry(&pt->faces, f, list){
        a = f->edge[0]->vertex[0];
        b = f->edge[0]->vertex[1];
        c = f->edge[1]->vertex[0];
        if (c == a || c == b){
            c = f->edge[1]->vertex[1];
        }
        fprintf(fout, "%d %d %d\n", a->id, b->id, c->id);
    }
}
