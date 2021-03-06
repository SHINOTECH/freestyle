#include "sculptor.h"
#include "../engine/timer.h"

Sculptor::Sculptor() :
    topHandler(this),
    currentOp(-1)
{}

Sculptor::~Sculptor() {
    for (int i = 0; i < (int) ops.size(); i++)
        delete ops[i];
}

void Sculptor::loop(QuasiUniformMesh::Point vCenterPos) {
    if (currentOp != -1) {
        vortex::Timer t, tswitch, tbuild, top;
        t.start();

        assert(params.valid());

        tbuild.start();
        buildField(vCenterPos);
        tbuild.stop();
        std::cout << "Timer build : " << tbuild.value() << std::endl;

        if(field_vertices.size() > 1)
        {
            top.start();
            Operator *op = getOperator(currentOp);
            qum->update_normals();
            op->applyDeformation(qum, vcenter, field_vertices, radius, params.getDMove());
            top.stop();
            std::cout << "Timer op : " << top.value() << std::endl;

            //*
            tswitch.start();
            switch(op->getTopologicalChange()) {
                case Operator::NONE:
                    QuasiUniformMeshConverter::makeUniformField(*qum, field_edges, params.getMinEdgeLength(), params.getMaxEdgeLength());
                    break;
                case Operator::GENUS:
                    // Stuff with Topological handler
                    QuasiUniformMesh::VertexHandle vCourant;
                    QuasiUniformMesh::VertexHandle vParcours;
                    QuasiUniformMesh::Point p1;
                    QuasiUniformMesh::Point p2;

                    //Parcours des sommets du champ en cours de modification
                    for(unsigned int i = 0; i < field_vertices.size(); i++)
                    {
                        vCourant = field_vertices[i].first;
                        if(!qum->status(vCourant).deleted())
                        {
                            //Parcours de tous les sommets du maillage
                            for(QuasiUniformMesh::VertexIter v_it = qum->vertices_sbegin(); v_it != qum->vertices_end(); ++v_it)
                            {
                                if (*v_it == vCourant) {
                                    break;
                                }

                                bool sommetAdjacent = false;
                                vParcours = *v_it;

                                //Parcours du premier anneau de vCourant
                                for(QuasiUniformMesh::VertexVertexIter vv_it = qum->vv_iter(vCourant); vv_it.is_valid(); ++vv_it)
                                {
                                    //Si le sommet courant du premier anneau est identique au sommet en cours de parcours
                                    if (*vv_it == vParcours) {
                                        sommetAdjacent = true;
                                        break;
                                    }
                                    //Parcours du premier anneau du sommet courant du premier anneau de vCourant
                                    for(QuasiUniformMesh::VertexVertexIter vv_it2 = qum->vv_iter(vParcours); vv_it2.is_valid(); ++vv_it2)
                                    {
                                        //Si le sommet courant est le même
                                        if (*vv_it2 == *vv_it) {
                                            sommetAdjacent = true;
                                            break;
                                        }
                                    }
                                    if (sommetAdjacent) {
                                        break;
                                    }
                                }

                                //Si le sommet est adjacent au sommet courant
                                if (sommetAdjacent) {
                                    break;
                                }

                                //Test dthickness
                                p1 = qum->point(vCourant);
                                p2 = qum->point(vParcours);
                                float dthickness = calcDist(p1, p2);
                                if (dthickness <= params.getDThickness()) {
                                    std::cout << "Appel de HandleJoinVertex" << std::endl;
                                    topHandler.handleJoinVertex(vCourant, vParcours);
                                    QuasiUniformMeshConverter::makeUniformField(*qum, connecting_edges, params.getMinEdgeLength(), params.getMaxEdgeLength());
                                    break;
                                }
                            }
                        }
                    }
                    QuasiUniformMeshConverter::makeUniformField(*qum, field_edges, params.getMinEdgeLength(), params.getMaxEdgeLength());

                    break;
            }
            qum->garbage_collection();
            tswitch.stop();
            std::cout << "Timer switch : " << tswitch.value() << std::endl;
            //*/
        }

        t.stop();
        std::cout << "Timer loop : " << t.value() << std::endl;
   }
}

float Sculptor::getRadius() const {
    return radius;
}

void Sculptor::setRadius(float value) {
    radius = value;
}

void Sculptor::buildField(QuasiUniformMesh::Point vCenterPos)
{
    field_vertices.clear();
    field_edges.clear();
    float minDist = FLT_MAX;

    for(QuasiUniformMesh::VertexIter v_it = qum->vertices_sbegin(); v_it != qum->vertices_end(); ++v_it)
    {
        float dist = calcDist(qum->point(*v_it), vCenterPos);

        if(dist < radius) {
            field_vertices.push_back(std::pair<QuasiUniformMesh::VertexHandle, float>(*v_it, dist));

            if (dist < minDist) {
                vcenter = *v_it;
                minDist = dist;
            }
        }
    }

    for(QuasiUniformMesh::EdgeIter e_it = qum->edges_sbegin(); e_it != qum->edges_end(); ++e_it)
    {
        QuasiUniformMesh::HalfedgeHandle heh0 = qum->halfedge_handle(*e_it, 0);
        QuasiUniformMesh::VertexHandle vh1 = qum->to_vertex_handle(heh0);
        QuasiUniformMesh::VertexHandle vh2 = qum->from_vertex_handle(heh0);
        bool v1 = false, v2 = false;
        int i = 0;

        while (i < (int) field_vertices.size() && !(v1 && v2))
        {
            OpenMesh::VertexHandle vh = field_vertices[i++].first;

            if(vh1 == vh)
                v1 = true;
            else if(vh2 == vh)
                v2 = true;
        }

        if(v1 && v2)
            field_edges.push_back(*e_it);
    }
}

void Sculptor::getMinMaxAvgEdgeLength(float &min, float &max, float &avg)
{
    min = FLT_MAX;
    max = 0;
    avg = 0;

    for(QuasiUniformMesh::EdgeIter e_it = qum->edges_sbegin(); e_it != qum->edges_end(); ++e_it)
    {
        float length = qum->calc_edge_length(*e_it);

        if(length < min)
            min = length;
        else if(length > max)
            max = length;

        avg += length;
    }

    avg /= qum->n_edges();
}
