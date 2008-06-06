#include "solvespace.h"

const hParam   Param::NO_PARAM = { 0 };
#define NO_PARAM (Param::NO_PARAM)

const hGroup Group::HGROUP_REFERENCES = { 1 };

#define gs (SS.GW.gs)

void Group::AddParam(IdList<Param,hParam> *param, hParam hp, double v) {
    Param pa;
    memset(&pa, 0, sizeof(pa));
    pa.h = hp;
    pa.val = v;

    param->Add(&pa);
}

void Group::MenuGroup(int id) {
    Group g;
    ZERO(&g);
    g.visible = true;

    if(id >= RECENT_IMPORT && id < (RECENT_IMPORT + MAX_RECENT)) {
        strcpy(g.impFile, RecentFile[id-RECENT_IMPORT]);
        id = GraphicsWindow::MNU_GROUP_IMPORT;
    }

    SS.GW.GroupSelection();

    switch(id) {
        case GraphicsWindow::MNU_GROUP_3D:
            g.type = DRAWING_3D;
            g.name.strcpy("draw-in-3d");
            break;

        case GraphicsWindow::MNU_GROUP_WRKPL:
            g.type = DRAWING_WORKPLANE;
            g.name.strcpy("draw-in-plane");
            if(gs.points == 1 && gs.n == 1) {
                g.subtype = WORKPLANE_BY_POINT_ORTHO;

                Vector u = SS.GW.projRight, v = SS.GW.projUp;
                u = u.ClosestOrtho();
                v = v.Minus(u.ScaledBy(v.Dot(u)));
                v = v.ClosestOrtho();

                g.predef.q = Quaternion::From(u, v);
                g.predef.origin = gs.point[0];
            } else if(gs.points == 1 && gs.lineSegments == 2 && gs.n == 3) {
                g.subtype = WORKPLANE_BY_LINE_SEGMENTS;

                g.predef.origin = gs.point[0];
                g.predef.entityB = gs.entity[0];
                g.predef.entityC = gs.entity[1];

                Vector ut = SS.GetEntity(g.predef.entityB)->VectorGetNum();
                Vector vt = SS.GetEntity(g.predef.entityC)->VectorGetNum();
                ut = ut.WithMagnitude(1);
                vt = vt.WithMagnitude(1);

                if(fabs(SS.GW.projUp.Dot(vt)) < fabs(SS.GW.projUp.Dot(ut))) {
                    SWAP(Vector, ut, vt);
                    g.predef.swapUV = true;
                }
                if(SS.GW.projRight.Dot(ut) < 0) g.predef.negateU = true;
                if(SS.GW.projUp.   Dot(vt) < 0) g.predef.negateV = true;
            } else {
                Error("Bad selection for new drawing in workplane.");
                return;
            }
            SS.GW.ClearSelection();
            break;

        case GraphicsWindow::MNU_GROUP_EXTRUDE:
            g.type = EXTRUDE;
            g.opA = SS.GW.activeGroup;
            g.color = RGB(100, 100, 100);
            g.predef.entityB = SS.GW.ActiveWorkplane();
            g.subtype = ONE_SIDED;
            g.name.strcpy("extrude");
            break;

        case GraphicsWindow::MNU_GROUP_ROT: {
            Vector n;
            if(gs.points == 1 && gs.n == 1 && SS.GW.LockedInWorkplane()) {
                g.predef.p = (SS.GetEntity(gs.point[0]))->PointGetNum();
                Entity *w = SS.GetEntity(SS.GW.ActiveWorkplane());
                n = (w->Normal()->NormalN());
                g.activeWorkplane = w->h;
            } else if(gs.points == 1 && gs.vectors == 1 && gs.n == 2) {
                g.predef.p = (SS.GetEntity(gs.point[0]))->PointGetNum();
                n = SS.GetEntity(gs.vector[0])->VectorGetNum();
            } else {
                Error("Bad selection for new rotation.");
                return;
            }
            n = n.WithMagnitude(1);
            g.predef.q = Quaternion::From(0, n.x, n.y, n.z);
            g.type = ROTATE;
            g.opA = SS.GW.activeGroup;
            g.exprA = Expr::From(3)->DeepCopyKeep();
            g.subtype = ONE_SIDED;
            g.name.strcpy("rotate");
            SS.GW.ClearSelection();
            break;
        }

        case GraphicsWindow::MNU_GROUP_TRANS:
            g.type = TRANSLATE;
            g.opA = SS.GW.activeGroup;
            g.exprA = Expr::From(3)->DeepCopyKeep();
            g.subtype = ONE_SIDED;
            g.name.strcpy("translate");
            break;

        case GraphicsWindow::MNU_GROUP_IMPORT: {
            g.type = IMPORTED;
            g.opA = SS.GW.activeGroup;
            if(strlen(g.impFile) == 0) {
                if(!GetOpenFile(g.impFile, SLVS_EXT, SLVS_PATTERN)) return;
            }
            g.name.strcpy("import");
            break;
        }

        default: oops();
    }
    SS.UndoRemember();
    SS.group.AddAndAssignId(&g);
    if(g.type == IMPORTED) {
        SS.ReloadAllImported();
    }
    SS.GenerateAll();
    SS.GW.activeGroup = g.h;
    if(g.type == DRAWING_WORKPLANE) {
        SS.GetGroup(g.h)->activeWorkplane = g.h.entity(0);
    }
    SS.GetGroup(g.h)->Activate();
    SS.GW.AnimateOntoWorkplane();
    TextWindow::ScreenSelectGroup(0, g.h.v);
    SS.later.showTW = true;
}

char *Group::DescriptionString(void) {
    static char ret[100];
    if(name.str[0]) {
        sprintf(ret, "g%03x-%s", h.v, name.str);
    } else {
        sprintf(ret, "g%03x-(unnamed)", h.v);
    }
    return ret;
}

void Group::Activate(void) {
    if(type == EXTRUDE || type == IMPORTED) {
        SS.GW.showFaces = true;
    } else {
        SS.GW.showFaces = false;
    }
    SS.MarkGroupDirty(h); // for good measure; shouldn't be needed
    SS.later.generateAll = true;
    SS.later.showTW = true;
}

void Group::Generate(IdList<Entity,hEntity> *entity,
                     IdList<Param,hParam> *param)
{
    Vector gn = (SS.GW.projRight).Cross(SS.GW.projUp);
    Vector gp = SS.GW.projRight.Plus(SS.GW.projUp);
    Vector gc = (SS.GW.offset).ScaledBy(-1);
    gn = gn.WithMagnitude(200/SS.GW.scale);
    gp = gp.WithMagnitude(200/SS.GW.scale);
    int a, i;
    switch(type) {
        case DRAWING_3D:
            break;

        case DRAWING_WORKPLANE: {
            Quaternion q;
            if(subtype == WORKPLANE_BY_LINE_SEGMENTS) {
                Vector u = SS.GetEntity(predef.entityB)->VectorGetNum();
                Vector v = SS.GetEntity(predef.entityC)->VectorGetNum();
                u = u.WithMagnitude(1);
                Vector n = u.Cross(v);
                v = (n.Cross(u)).WithMagnitude(1);

                if(predef.swapUV) SWAP(Vector, u, v);
                if(predef.negateU) u = u.ScaledBy(-1);
                if(predef.negateV) v = v.ScaledBy(-1);
                q = Quaternion::From(u, v);
            } else if(subtype == WORKPLANE_BY_POINT_ORTHO) {
                // Already given, numerically.
                q = predef.q;
            } else oops();

            Entity normal;
            memset(&normal, 0, sizeof(normal));
            normal.type = Entity::NORMAL_N_COPY;
            normal.numNormal = q;
            normal.point[0] = h.entity(2);
            normal.group = h;
            normal.h = h.entity(1);
            entity->Add(&normal);

            Entity point;
            memset(&point, 0, sizeof(point));
            point.type = Entity::POINT_N_COPY;
            point.numPoint = SS.GetEntity(predef.origin)->PointGetNum();
            point.group = h;
            point.h = h.entity(2);
            entity->Add(&point);

            Entity wp;
            memset(&wp, 0, sizeof(wp));
            wp.type = Entity::WORKPLANE;
            wp.normal = normal.h;
            wp.point[0] = point.h;
            wp.group = h;
            wp.h = h.entity(0);
            entity->Add(&wp);
            break;
        }

        case EXTRUDE: {
            AddParam(param, h.param(0), gn.x);
            AddParam(param, h.param(1), gn.y);
            AddParam(param, h.param(2), gn.z);
            int ai, af;
            if(subtype == ONE_SIDED) {
                ai = 0; af = 2;
            } else if(subtype == TWO_SIDED) {
                ai = -1; af = 1;
            } else oops();

            // Get some arbitrary point in the sketch, that will be used
            // as a reference when defining top and bottom faces.
            hEntity pt = { 0 };
            for(i = 0; i < entity->n; i++) {
                Entity *e = &(entity->elem[i]);
                if(e->group.v != opA.v) continue;

                if(e->IsPoint()) pt = e->h;

                e->CalculateNumerical();
                hEntity he = e->h; e = NULL;
                // As soon as I call CopyEntity, e may become invalid! That
                // adds entities, which may cause a realloc.
                CopyEntity(entity, SS.GetEntity(he), ai, REMAP_BOTTOM,
                    h.param(0), h.param(1), h.param(2),
                    NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                    true, false);
                CopyEntity(entity, SS.GetEntity(he), af, REMAP_TOP,
                    h.param(0), h.param(1), h.param(2),
                    NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                    true, false);
                MakeExtrusionLines(entity, he);
            }
            // Remapped versions of that arbitrary point will be used to
            // provide points on the plane faces.
            MakeExtrusionTopBottomFaces(entity, pt);
            break;
        }

        case TRANSLATE: {
            // The translation vector
            AddParam(param, h.param(0), gp.x);
            AddParam(param, h.param(1), gp.y);
            AddParam(param, h.param(2), gp.z);

            int n = (int)(exprA->Eval());
            for(a = 0; a < n; a++) {
                for(i = 0; i < entity->n; i++) {
                    Entity *e = &(entity->elem[i]);
                    if(e->group.v != opA.v) continue;

                    e->CalculateNumerical();
                    CopyEntity(entity, e,
                        a*2 - (subtype == ONE_SIDED ? 0 : (n-1)),
                        (a == (n - 1)) ? REMAP_LAST : a,
                        h.param(0), h.param(1), h.param(2),
                        NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                        true, false);
                }
            }
            break;
        }
        case ROTATE: {
            // The center of rotation
            AddParam(param, h.param(0), gc.x);
            AddParam(param, h.param(1), gc.y);
            AddParam(param, h.param(2), gc.z);
            // The rotation quaternion
            AddParam(param, h.param(3), 15*PI/180);
            AddParam(param, h.param(4), gn.x);
            AddParam(param, h.param(5), gn.y);
            AddParam(param, h.param(6), gn.z);

            int n = (int)(exprA->Eval());
            for(a = 0; a < n; a++) {
                for(i = 0; i < entity->n; i++) {
                    Entity *e = &(entity->elem[i]);
                    if(e->group.v != opA.v) continue;

                    e->CalculateNumerical();
                    CopyEntity(entity, e,
                        a*2 - (subtype == ONE_SIDED ? 0 : (n-1)),
                        (a == (n - 1)) ? REMAP_LAST : a,
                        h.param(0), h.param(1), h.param(2),
                        h.param(3), h.param(4), h.param(5), h.param(6),
                        false, true);
                }
            }
            break;
        }
        case IMPORTED:
            // The translation vector
            AddParam(param, h.param(0), gp.x);
            AddParam(param, h.param(1), gp.y);
            AddParam(param, h.param(2), gp.z);
            // The rotation quaternion
            AddParam(param, h.param(3), 1);
            AddParam(param, h.param(4), 0);
            AddParam(param, h.param(5), 0);
            AddParam(param, h.param(6), 0);
            
            for(i = 0; i < impEntity.n; i++) {
                Entity *ie = &(impEntity.elem[i]);

                CopyEntity(entity, ie, 0, 0,
                    h.param(0), h.param(1), h.param(2),
                    h.param(3), h.param(4), h.param(5), h.param(6),
                    false, false);
            }
            break;

        default: oops();
    }
}

void Group::AddEq(IdList<Equation,hEquation> *l, Expr *expr, int index) {
    Equation eq;
    eq.e = expr;
    eq.h = h.equation(index);
    l->Add(&eq);
}

void Group::GenerateEquations(IdList<Equation,hEquation> *l) {
    Equation eq;
    ZERO(&eq);
    if(type == IMPORTED) {
        // Normalize the quaternion
        ExprQuaternion q = {
            Expr::From(h.param(3)),
            Expr::From(h.param(4)),
            Expr::From(h.param(5)),
            Expr::From(h.param(6)) };
        AddEq(l, (q.Magnitude())->Minus(Expr::From(1)), 0);
    } else if(type == ROTATE) {
        // The axis and center of rotation are specified numerically
#define EC(x) (Expr::From(x))
#define EP(x) (Expr::From(h.param(x)))
        AddEq(l, (EC(predef.p.x))->Minus(EP(0)), 0);
        AddEq(l, (EC(predef.p.y))->Minus(EP(1)), 1);
        AddEq(l, (EC(predef.p.z))->Minus(EP(2)), 2);
        // param 3 is the angle, which is free
        AddEq(l, (EC(predef.q.vx))->Minus(EP(4)), 3);
        AddEq(l, (EC(predef.q.vy))->Minus(EP(5)), 4);
        AddEq(l, (EC(predef.q.vz))->Minus(EP(6)), 5);
    } else if(type == EXTRUDE) {
        if(predef.entityB.v != Entity::FREE_IN_3D.v) {
            // The extrusion path is locked along a line, normal to the
            // specified workplane.
            Entity *w = SS.GetEntity(predef.entityB);
            ExprVector u = w->Normal()->NormalExprsU();
            ExprVector v = w->Normal()->NormalExprsV();
            ExprVector extruden = {
                Expr::From(h.param(0)),
                Expr::From(h.param(1)),
                Expr::From(h.param(2)) };

            AddEq(l, u.Dot(extruden), 0);
            AddEq(l, v.Dot(extruden), 1);
        }
    }
}

hEntity Group::Remap(hEntity in, int copyNumber) {
    int i;
    for(i = 0; i < remap.n; i++) {
        EntityMap *em = &(remap.elem[i]);
        if(em->input.v == in.v && em->copyNumber == copyNumber) {
            // We already have a mapping for this entity.
            return h.entity(em->h.v);
        }
    }
    // We don't have a mapping yet, so create one.
    EntityMap em;
    em.input = in;
    em.copyNumber = copyNumber;
    remap.AddAndAssignId(&em);
    return h.entity(em.h.v);
}

void Group::MakeExtrusionLines(IdList<Entity,hEntity> *el, hEntity in) {
    Entity *ep = SS.GetEntity(in);

    Entity en;
    ZERO(&en);
    if(ep->IsPoint()) {
        // A point gets extruded to form a line segment
        en.point[0] = Remap(ep->h, REMAP_TOP);
        en.point[1] = Remap(ep->h, REMAP_BOTTOM);
        en.group = h;
        en.h = Remap(ep->h, REMAP_PT_TO_LINE);
        en.type = Entity::LINE_SEGMENT;
        el->Add(&en);
    } else if(ep->type == Entity::LINE_SEGMENT) {
        // A line gets extruded to form a plane face; an endpoint of the
        // original line is a point in the plane, and the line is in the plane.
        Vector a = SS.GetEntity(ep->point[0])->PointGetNum();
        Vector b = SS.GetEntity(ep->point[1])->PointGetNum();
        Vector ab = b.Minus(a);

        en.param[0] = h.param(0);
        en.param[1] = h.param(1);
        en.param[2] = h.param(2);
        en.numPoint = a;
        en.numVector = ab;

        en.group = h;
        en.h = Remap(ep->h, REMAP_LINE_TO_FACE);
        en.type = Entity::FACE_XPROD;
        el->Add(&en);
    }
}

void Group::MakeExtrusionTopBottomFaces(IdList<Entity,hEntity> *el, hEntity pt)
{
    if(pt.v == 0) return;
    Group *src = SS.GetGroup(opA);
    Vector n = src->poly.normal;

    Entity en;
    ZERO(&en);
    en.type = Entity::FACE_NORMAL_PT;
    en.group = h;

    en.numNormal = Quaternion::From(0, n.x, n.y, n.z);
    en.point[0] = Remap(pt, REMAP_TOP);
    en.h = Remap(Entity::NO_ENTITY, REMAP_TOP);
    el->Add(&en);

    en.point[0] = Remap(pt, REMAP_BOTTOM);
    en.h = Remap(Entity::NO_ENTITY, REMAP_BOTTOM);
    el->Add(&en);
}

void Group::CopyEntity(IdList<Entity,hEntity> *el,
                       Entity *ep, int timesApplied, int remap,
                       hParam dx, hParam dy, hParam dz,
                       hParam qw, hParam qvx, hParam qvy, hParam qvz,
                       bool asTrans, bool asAxisAngle)
{
    Entity en;
    memset(&en, 0, sizeof(en));
    en.type = ep->type;
    en.h = Remap(ep->h, remap);
    en.timesApplied = timesApplied;
    en.group = h;
    en.construction = ep->construction;

    switch(ep->type) {
        case Entity::WORKPLANE:
            // Don't copy these.
            return;

        case Entity::LINE_SEGMENT:  
            en.point[0] = Remap(ep->point[0], remap);
            en.point[1] = Remap(ep->point[1], remap);
            break;

        case Entity::CUBIC:
            en.point[0] = Remap(ep->point[0], remap);
            en.point[1] = Remap(ep->point[1], remap);
            en.point[2] = Remap(ep->point[2], remap);
            en.point[3] = Remap(ep->point[3], remap);
            break;

        case Entity::CIRCLE:
            en.point[0] = Remap(ep->point[0], remap);
            en.normal   = Remap(ep->normal, remap);
            en.distance = Remap(ep->distance, remap);
            break;

        case Entity::ARC_OF_CIRCLE:
            en.point[0] = Remap(ep->point[0], remap);
            en.point[1] = Remap(ep->point[1], remap);
            en.point[2] = Remap(ep->point[2], remap);
            en.normal   = Remap(ep->normal, remap);
            break;

        case Entity::POINT_N_COPY:
        case Entity::POINT_N_TRANS:
        case Entity::POINT_N_ROT_TRANS:
        case Entity::POINT_N_ROT_AA:
        case Entity::POINT_IN_3D:
        case Entity::POINT_IN_2D:
            if(asTrans) {
                en.type = Entity::POINT_N_TRANS;
                en.param[0] = dx;
                en.param[1] = dy;
                en.param[2] = dz;
            } else {
                if(asAxisAngle) {
                    en.type = Entity::POINT_N_ROT_AA;
                } else {
                    en.type = Entity::POINT_N_ROT_TRANS;
                }
                en.param[0] = dx;
                en.param[1] = dy;
                en.param[2] = dz;
                en.param[3] = qw;
                en.param[4] = qvx;
                en.param[5] = qvy;
                en.param[6] = qvz;
            }
            en.numPoint = ep->actPoint;
            break;

        case Entity::NORMAL_N_COPY:
        case Entity::NORMAL_N_ROT:
        case Entity::NORMAL_N_ROT_AA:
        case Entity::NORMAL_IN_3D:
        case Entity::NORMAL_IN_2D:
            if(asTrans) {
                en.type = Entity::NORMAL_N_COPY;
            } else {
                if(asAxisAngle) {
                    en.type = Entity::NORMAL_N_ROT_AA;
                } else {
                    en.type = Entity::NORMAL_N_ROT;
                }
                en.param[0] = qw;
                en.param[1] = qvx;
                en.param[2] = qvy;
                en.param[3] = qvz;
            }
            en.numNormal = ep->actNormal;
            en.point[0] = Remap(ep->point[0], remap);
            break;

        case Entity::DISTANCE_N_COPY:
        case Entity::DISTANCE:
            en.type = Entity::DISTANCE_N_COPY;
            en.numDistance = ep->actDistance;
            break;

        case Entity::FACE_NORMAL_PT:
        case Entity::FACE_XPROD:
        case Entity::FACE_N_ROT_TRANS:
            if(asTrans || asAxisAngle) return;

            en.type = Entity::FACE_N_ROT_TRANS;
            en.param[0] = dx;
            en.param[1] = dy;
            en.param[2] = dz;
            en.param[3] = qw;
            en.param[4] = qvx;
            en.param[5] = qvy;
            en.param[6] = qvz;
            en.numPoint = ep->numPoint;
            en.numNormal = ep->numNormal;
            break;

        default:
            oops();
    }
    el->Add(&en);
}

void Group::TagEdgesFromLineSegments(SEdgeList *el) {
    int i, j;
    for(i = 0; i < SS.entity.n; i++) {
        Entity *e = &(SS.entity.elem[i]);
        if(e->group.v != opA.v) continue;
        if(e->type != Entity::LINE_SEGMENT) continue;

        Vector p0 = SS.GetEntity(e->point[0])->PointGetNum();
        Vector p1 = SS.GetEntity(e->point[1])->PointGetNum();
        
        for(j = 0; j < el->l.n; j++) {
            SEdge *se = &(el->l.elem[j]);
            if((p0.Equals(se->a) && p1.Equals(se->b))) se->tag = e->h.v;
            if((p0.Equals(se->b) && p1.Equals(se->a))) se->tag = e->h.v;
        }
    }
}
