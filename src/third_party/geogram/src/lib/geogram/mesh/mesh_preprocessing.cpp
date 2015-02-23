/*
 *  Copyright (c) 2012-2014, Bruno Levy
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  * Neither the name of the ALICE Project-Team nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 * 
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  If you modify this software, you should include a notice giving the
 *  name of the person performing the modification, the date of modification,
 *  and the reason for such modification.
 *
 *  Contact: Bruno Levy
 *
 *     Bruno.Levy@inria.fr
 *     http://www.loria.fr/~levy
 *
 *     ALICE Project
 *     LORIA, INRIA Lorraine, 
 *     Campus Scientifique, BP 239
 *     54506 VANDOEUVRE LES NANCY CEDEX 
 *     FRANCE
 *
 */

#include <geogram/mesh/mesh_preprocessing.h>
#include <geogram/mesh/mesh_topology.h>
#include <geogram/mesh/mesh_private.h>
#include <geogram/mesh/mesh_geometry.h>
#include <geogram/mesh/index.h>
#include <geogram/mesh/mesh_halfedges.h>
#include <geogram/basic/geometry_nd.h>
#include <geogram/basic/stopwatch.h>

#include <stack>

namespace {

    using namespace GEO;

    /**
     * \brief
     * Compute the signed volume of the pyramid that connects
     * the origin to facet f.
     * \details Summing all the signed volumes
     * of the facets of a closed surface results in the signed
     * volume of the interior of the surface (volumes outside
     * the surface cancel-out).
     * \param[in] M the mesh
     * \param[in] f index of the facet
     * \return the signed volume of the pyramid that connects facet \p f to
     *  the origin
     */
    double signed_volume(const Mesh& M, index_t f) {
        double result = 0;
        index_t v0 = M.corner_vertex_index(M.facet_begin(f));
        const vec3& p0 = Geom::mesh_vertex(M, v0);
        for(index_t c =
            M.facet_begin(f) + 1; c + 1 < M.facet_end(f); c++
        ) {
            index_t v1 = M.corner_vertex_index(c);
            const vec3& p1 = Geom::mesh_vertex(M, v1);
            index_t v2 = M.corner_vertex_index(c + 1);
            const vec3& p2 = Geom::mesh_vertex(M, v2);
            result += dot(p0, cross(p1, p2));
        }
        return result;
    }
}

/****************************************************************************/

namespace GEO {

    void expand_border(Mesh& M, double epsilon) {
        if(epsilon == 0.0) {
            return;
        }
        vector<vec3> border_normal;
        border_normal.assign(M.nb_vertices(), vec3(0.0, 0.0, 0.0));
        for(index_t f = 0; f < M.nb_facets(); f++) {
            vec3 N = Geom::mesh_facet_normal(M, f);
            for(index_t c1 = M.facet_begin(f); c1 < M.facet_end(f); c1++) {
                if(M.corner_adjacent_facet(c1) == -1) {
                    index_t c2 = M.next_around_facet(f, c1);
                    index_t v1 = M.corner_vertex_index(c1);
                    index_t v2 = M.corner_vertex_index(c2);
                    const vec3& p1 = Geom::mesh_vertex(M, v1);
                    const vec3& p2 = Geom::mesh_vertex(M, v2);
                    vec3 Ne = cross(p2 - p1, N);
                    border_normal[v1] += Ne;
                    border_normal[v2] += Ne;
                }
            }
        }
        for(index_t v = 0; v < M.nb_vertices(); v++) {
            double s = length(border_normal[v]);
            if(s > 0.0) {
                Geom::mesh_vertex_ref(M, v) +=
                    epsilon * (1.0 / s) * border_normal[v];
            }
        }
    }

    // == connected components and small facets ================================

    void remove_small_facets(Mesh& M, double min_facet_area) {
        vector<index_t> remove_f(M.nb_facets(), 0);
        for(index_t f = 0; f < M.nb_facets(); f++) {
            if(M.facet_area(f, 3) < min_facet_area) {
                remove_f[f] = 1;
            }
        }
        M.remove_facets(remove_f);
    }

    void remove_small_connected_components(Mesh& M, double min_area, index_t min_facets) {
        vector<index_t> component;
        index_t nb_components = get_connected_components(M, component);
        vector<double> comp_area(nb_components, 0.0);
        vector<index_t> comp_facets(nb_components, 0);
        for(index_t f = 0; f < M.nb_facets(); f++) {
            comp_area[component[f]] += M.facet_area(f, 3);
            ++comp_facets[component[f]];
        }

        Logger::out("Components")
            << "Nb connected components=" << comp_area.size() << std::endl;
        index_t nb_remove = 0;
        for(index_t c = 0; c < comp_area.size(); c++) {
            if(comp_area[c] < min_area || comp_facets[c] < min_facets) {
                nb_remove++;
            }
        }

        if(nb_remove == 0) {
            Logger::out("Components")
                << "Mesh does not have small connected component (good)"
                << std::endl;
            return;
        }

        index_t nb_f_remove = 0;
        vector<index_t> remove_f(M.nb_facets(), 0);
        for(index_t f = 0; f < M.nb_facets(); ++f) {
            if(
                comp_area[component[f]] < min_area || 
                comp_facets[component[f]] < min_facets
            ) {
                remove_f[f] = 1;
                nb_f_remove++;
            }
        }
        M.remove_facets(remove_f);

        Logger::out("Components")
            << "Removed " << nb_remove << " connected components"
            << "(" << nb_f_remove << " facets)"
            << std::endl;
    }

    // ============== orient_normals ========================================

    void orient_normals(Mesh& M) {
        vector<index_t> component;
        index_t nb_components = get_connected_components(M, component);
        vector<double> comp_signed_volume(nb_components, 0.0);
        for(index_t f = 0; f < M.nb_facets(); ++f) {
            comp_signed_volume[component[f]] += signed_volume(M, f);
        }
        for(index_t f = 0; f < M.nb_facets(); ++f) {
            if(comp_signed_volume[component[f]] < 0.0) {
                MeshMutator::flip_facet(M, f);
            }
        }
    }

    void invert_normals(Mesh& M) {
        for(index_t f = 0; f < M.nb_facets(); ++f) {
            MeshMutator::flip_facet(M, f);
        }
    }
}

