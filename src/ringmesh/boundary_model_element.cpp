/*
 * Copyright (c) 2012-2015, Association Scientifique pour la Geologie et ses Applications (ASGA)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  Contacts:
 *     Arnaud.Botella@univ-lorraine.fr
 *     Antoine.Mazuyer@univ-lorraine.fr
 *     Jeanne.Pellerin@wias-berlin.de
 *
 *     http://www.gocad.org
 *
 *     GOCAD Project
 *     Ecole Nationale Supérieure de Géologie - Georessources
 *     2 Rue du Doyen Marcel Roubault - TSA 70605
 *     54518 VANDOEUVRE-LES-NANCY
 *     FRANCE
 */

/*! \author Jeanne Pellerin and Arnaud Botella */

#include <ringmesh/boundary_model_element.h>
#include <ringmesh/boundary_model.h>
#include <ringmesh/utils.h>

#include <geogram/basic/geometry_nd.h>
#include <geogram/mesh/triangle_intersection.h>
#include <geogram/mesh/mesh.h>
#include <geogram/mesh/mesh_geometry.h>
#include <geogram/mesh/mesh_AABB.h>
#include <geogram/mesh/mesh_topology.h>
#include <geogram/mesh/mesh_intersection.h>
//#include <geogram/mesh/mesh_reorder.h>
//#include <geogram/mesh/mesh_preprocessing.h>

#include <set>
#include <stack>
#include <fstream>


namespace {
    /* Definition of functions that we do not want exported in the interface */
    using namespace RINGMesh ;
    using namespace GEO ;
    using GEO::index_t ;
    using GEO::vec3 ;

    /*!
    * @brief Checks that the model vertex indices of @param E 
    *       are in a valid range
    */
    bool check_range_model_vertex_ids( const BoundaryModelMeshElement& E )
    {
        /// Check that the stored model vertex indices are in a valid range
        for( index_t i = 0; i < E.nb_vertices(); ++i ) {
            if( E.model_vertex_id( i ) == NO_ID
                && E.model_vertex_id( i ) >= E.model().nb_vertices() ) {
                return false ;
            }
        }
        return true ;
    }

    /*!
    * @brief Detects duplicated vertices in a BMME
    * @details Uses model indices.
    *
    * @param E The BoundaryModel element to check
    * @param duplicated For each vertex if it is duplicated refers to the
    *    model vertex id for the point in the surface otherwise NO_ID is stored.
    *    The points that are duplicated must be in the boundary
    *    of the element to have a valid element.
    *
    */
    index_t detect_duplicated_vertices(
        const BoundaryModelMeshElement& E,
        std::vector< index_t >& duplicated )
    {
        const GEO::Mesh& M = E.mesh() ;
        duplicated.resize( M.vertices.nb(), NO_ID ) ;

        for( index_t v = 0; v < M.vertices.nb(); ++v ) {
            if( duplicated[ v ] == NO_ID ) {
                index_t gv = E.model_vertex_id( v ) ;

                const std::vector< BoundaryModelVertices::VertexInBME >&
                    colocated = E.model().vertices.bme_vertices( gv ) ;
                if( colocated.size() > 0 ) {
                    int count = 0 ;
                    for( index_t i = 0; i < colocated.size(); ++i ) {
                        if( colocated[ i ].bme_id == E.bme_id() ) {
                            count++ ;
                        }
                    }
                    ringmesh_assert( count > 0 ) ;
                    if( count > 1 ) {
                        duplicated[ v ] = gv ;
                    }
                }
            }
        }
        return duplicated.size() - std::count(
            duplicated.begin(), duplicated.end(), NO_ID ) ;
    }

    /*!
    * @brief Count the number of times each vertex is in an edge or facet
    *
    * @param[in] M The mesh 
    * @param[out] nb Resized to the number of vertices of the mesh.
    *      Number of times one vertex appear in an edge or facet of the mesh.
    */
    void count_vertex_occurences(
        const GEO::Mesh& M,
        std::vector< index_t >& nb ) 
    {        
        nb.resize( M.vertices.nb(), 0 ) ;
        for( index_t e = 0; e < M.edges.nb(); ++e ) {
            ++nb[ M.edges.vertex( e, 0 ) ] ;
            ++nb[ M.edges.vertex( e, 1 ) ] ;
        }
        for( index_t f = 0; f < M.facets.nb(); ++f ) {
            for( index_t co = M.facets.corners_begin( f ) ;
                 co < M.facets.corners_end( f ); ++co
                 ) {
                ++nb[ M.facet_corners.vertex( co ) ] ;
            }
        }
    }



    /*---------------------------------------------------------------------------*/
    /* ----- Code copied and modified from geogram\mesh\mesh_intersection.cpp ---*/
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
    */
    


    /**
    * \brief Computes the intersection between two triangular facets in
    *  a mesh
    * \param[in] M the mesh
    * \param[in] f1 index of the first facet
    * \param[in] f2 index of the second facet
    * \param[out] sym symbolic representation of the intersection (if any)
    * \return true if facets \p f1 and \p f2 have an intersection, false
    *  otherwise
    */
    bool triangles_intersect(
        const Mesh& M, index_t f1, index_t f2,
        vector<TriangleIsect>& sym
        )
    {
        geo_debug_assert( M.facets.nb_vertices( f1 ) == 3 );
        geo_debug_assert( M.facets.nb_vertices( f2 ) == 3 );
        index_t c1 = M.facets.corners_begin( f1 );
        const vec3& p1 = GEO::Geom::mesh_vertex( M, M.facet_corners.vertex( c1 ) );
        const vec3& p2 = GEO::Geom::mesh_vertex( M, M.facet_corners.vertex( c1 + 1 ) );
        const vec3& p3 = GEO::Geom::mesh_vertex( M, M.facet_corners.vertex( c1 + 2 ) );
        index_t c2 = M.facets.corners_begin( f2 );
        const vec3& q1 = GEO::Geom::mesh_vertex( M, M.facet_corners.vertex( c2 ) );
        const vec3& q2 = GEO::Geom::mesh_vertex( M, M.facet_corners.vertex( c2 + 1 ) );
        const vec3& q3 = GEO::Geom::mesh_vertex( M, M.facet_corners.vertex( c2 + 2 ) );
        return triangles_intersections( p1, p2, p3, q1, q2, q3, sym );
    }

    /**
    * \brief Tests whether two facets are adjacent
    * \details Two facets are adjacents if they share an edge
    *          In a Surface two facets are adjacent if they are stored as such
    *          in the Mesh, but they can also share an edge along the boundary of the 
    *          Surface - checked with the global model indices
    *
    * \param[in] S the surface
    * \param[in] f1 index of the first facet
    * \param[in] f2 index of the second facet
    * \return true if facets \p f1 and \p f2 share an edge, false
    *  otherwise
    */
    bool facets_are_adjacent( const Surface& S, index_t f1, index_t f2 )
    {
        if( f1 == f2 ) {
            return true;
        }
        for( index_t v = 0; v < S.nb_vertices_in_facet( f1 ); ++v ) {
            if( S.adjacent( f1, v ) == f2 ) {
                return true;
            } else if( S.adjacent( f1, v ) == NO_ID ) {
                index_t p0 = S.model_vertex_id( f1, v ) ;
                index_t p1 = S.model_vertex_id( f1, S.next_in_facet( f1, v ) );
                // Check if the edge on border is not the same
                // than an edge on the border in f2 - JP
                for( index_t v2 = 0; v2 < S.nb_vertices_in_facet( f2 ); ++v2 ) {
                    if( S.adjacent( f2, v2 ) == NO_ID ) {
                        index_t p02 = S.model_vertex_id( f2, v ) ;
                        index_t p12 = S.model_vertex_id( f2, S.next_in_facet( f2, v ) );
                        if( p0 == p02 && p1 == p12 ) {
                            return true ;
                        } else if( p0 = p12 && p1 == p02 ) {
                            return true ;
                        }
                    }
                }
            }
        }
        return false;
    }

    /**
    * \brief Action class for storing intersections when traversing
    *  a AABBTree.
    */
    class StoreIntersections {
    public:
        /**
        * \brief Constructs the StoreIntersections
        * \param[in] M the mesh
        * \param[out] has_isect the flag that indicates for each facet
        *  whether it has intersections
        */
        StoreIntersections(
            const Surface& S, vector<index_t>& has_isect
            ) :
            S_( S ),
            has_intersection_( has_isect )
        {
            has_intersection_.assign( S.mesh().facets.nb(), 0 );
        }

        /**
        * \brief Determines the intersections between two facets
        * \details It is a callback for AABBTree traversal
        * \param[in] f1 index of the first facet
        * \param[in] f2 index of the second facet
        */
        void operator() ( index_t f1, index_t f2 )
        {
            if(
                !facets_are_adjacent( S_, f1, f2 ) &&
                f1 != f2 &&
                triangles_intersect( S_.mesh(), f1, f2, sym_ )
                ) {
                has_intersection_[ f1 ] = 1;
                has_intersection_[ f2 ] = 1;
            }


        }

    private:
        const Surface& S_;
        vector<index_t>& has_intersection_;
        vector<TriangleIsect> sym_;
    };


    /**
    * \brief Detect intersecting facets in a mesh TRIANGULATED !!
    * \param[in] M the mesh
    * \return
    */
    index_t detect_intersecting_facets( const Surface& S )
    {
        GEO::Mesh& M = S.mesh() ;
        geo_assert( M.vertices.dimension() >= 3 );

        vector<index_t> has_intersection;
        StoreIntersections action( S, has_intersection );
        MeshFacetsAABB AABB( M );
        AABB.compute_facet_bbox_intersections( action );

        return std::count( has_intersection.begin(), has_intersection.end(), 0 ) ;
    }


}



namespace RINGMesh {
    /*!
     * @brief Map the name of a geological type with a value of GEOL_FEATURE
     *
     * @param[in] in Name of the feature
     * @return The geological feature index
     *
     * \todo Keep all the information (add new GEOL_FEATURE) instead of simplifying it.
     */
    BoundaryModelElement::GEOL_FEATURE BoundaryModelElement::
        determine_geological_type( const std::string& in )
    {
        if( in == "" ) {
            return NO_GEOL ;
        }
        if( in == "reverse_fault" ) {
            return FAULT ;
        }
        if( in == "normal_fault" ) {
            return FAULT ;
        }
        if( in == "fault" ) {
            return FAULT ;
        }
        if( in == "top" ) {
            return STRATI ;
        }
        if( in == "none" ) {
            return NO_GEOL ;
        }
        if( in == "unconformity" ) {
            return STRATI ;
        }
        if( in == "boundary" ) {
            return VOI ;
        }

        std::cout << "ERROR" << "Unexpected type in the model file " << in
            << std::endl ;
        return NO_GEOL ;
    }


    /*!
     * @brief Compute an intersection type. Really useful ?
     *
     * @param[in] types TYPEs that intersect
     * @return Intersection type
     */
    BoundaryModelElement::GEOL_FEATURE BoundaryModelElement::determine_type(
        const std::vector< GEOL_FEATURE >& types )
    {
        if( types.size() == 0 ) {
            return NO_GEOL ;
        }

        // Sort and remove duplicates from the in types
        std::vector< GEOL_FEATURE > in = types ;
        std::sort( in.begin(), in.end() ) ;
        index_t new_size = narrow_cast<index_t>( std::unique( in.begin(), in.end() ) - in.begin() ) ;
        in.resize( new_size ) ;

        if( in.size() == 1 ) {
            return in[ 0 ] ;
        }

        if( in.size() == 2 ) {
            if( in[ 0 ] == NO_GEOL ) {
                return NO_GEOL ;
            }
            if( in[ 0 ] == STRATI ) {
                if( in[ 1 ] == FAULT ) {
                    return STRATI_FAULT ;
                }
                if( in[ 1 ] == VOI ) {
                    return STRATI_VOI ;
                }
            } else if( in[ 0 ] == FAULT ) {
                if( in[ 1 ] == VOI ) {
                    return FAULT_VOI ;
                }
            }

            // Other cases ? for corners ? what is the vertex ?
            return NO_GEOL ;
        }
        return NO_GEOL ;
    }


    std::string BoundaryModelElement::type_name( BoundaryModelElement::TYPE t )
    {
        switch( t ) {
            case CORNER: return "CORNER" ;
            case LINE: return "LINE" ;
            case SURFACE: return "SURFACE" ;
            case REGION: return "REGION" ;
            case CONTACT: return "CONTACT" ;
            case INTERFACE: return "INTERFACE" ;
            case LAYER: return "LAYER" ;
            default: return "NO_TYPE_NAME" ;
        }
    }


    std::string BoundaryModelElement::geol_name(
        BoundaryModelElement::GEOL_FEATURE t )
    {
        switch( t ) {
            case STRATI: return "top" ;
            case FAULT: return "fault" ;
            case VOI: return "boundary" ;
            case NO_GEOL: return "none" ;
            default: return "none" ;
                break ;
        }
    }


    /*!
     * @brief Defines the type of the parent of an element of type @param t
     *        If no parent is allowed returns NO_TYPE
     * @details The elements that can have a parent are LINE, SURFACE, and REGION
     */
    BoundaryModelElement::TYPE BoundaryModelElement::parent_type(
        BoundaryModelElement::TYPE t )
    {
        switch( t ) {
            case LINE: return CONTACT ;
            case SURFACE: return INTERFACE ;
            case REGION: return LAYER ;
            default:

                // The others have no parent
                return NO_TYPE ;
        }
    }


    /*!
     * @brief Defines the type of a child of an element of type @param t
     *        If no child is allowed returns NO_TYPE
     * @details The elements that can have a parent are CONTACT, INTERFACE, and LAYER
     */
    BoundaryModelElement::TYPE BoundaryModelElement::child_type(
        BoundaryModelElement::TYPE t )
    {
        switch( t ) {
            case CONTACT: return LINE  ;
            case INTERFACE: return SURFACE ;
            case LAYER: return REGION ;
            default:
                return NO_TYPE ;
        }
    }


    /*!
     * @brief Defines the type of an element on the boundary of an element of type @param t
     *        If no boundary is allowed returns NO_TYPE
     * @details The elements that can have a boundary are LINE, SURFACE, and REGION
     */
    BoundaryModelElement::TYPE BoundaryModelElement::boundary_type(
        BoundaryModelElement::TYPE t )
    {
        switch( t ) {
            case LINE: return CORNER ;
            case SURFACE: return LINE ;
            case REGION: return SURFACE ;
            default:
                return NO_TYPE ;
        }
    }


    /*!
     * @brief Define the type of an element into which boundary an element of type @param t can be
     *        If no in_boundary is allowed returns NO_TYPE
     * @details The elements that can be in the boundary of another are CORNER, LINE, and SURFACE
     */
    BoundaryModelElement::TYPE BoundaryModelElement::in_boundary_type(
        BoundaryModelElement::TYPE t )
    {
        switch( t ) {
            case CORNER: return LINE ;
            case LINE: return SURFACE ;
            case SURFACE: return REGION ;
            default:
                return NO_TYPE ;
        }
    }


    /*!
     * @brief Dimension 0, 1, 2, or 3 of an element of type @param t
     */
    index_t BoundaryModelElement::dimension( BoundaryModelElement::TYPE t )
    {
        switch( t ) {
            case CORNER: return 0 ;
            case LINE: return 1 ;
            case SURFACE: return 2 ;
            case REGION: return 3 ;
            case CONTACT: return 1 ;
            case INTERFACE: return 2 ;
            case LAYER: return 3 ;
            default: return NO_ID ;
        }
    }


    bool BoundaryModelElement::operator==( const BoundaryModelElement& rhs ) const
    {
        if( model_ != rhs.model_ ) {
            return false ;
        }
        if( id_ != rhs.id_ ) {
            return false ;
        }
        if( name_ != rhs.name_ ) {
            return false ;
        }
        if( geol_feature_ != rhs.geol_feature_ ) {
            return false ;
        }
        if( nb_boundaries() != rhs.nb_boundaries() ) {
            return false ;
        }
        if( !std::equal( boundaries_.begin(), boundaries_.end(),
            rhs.boundaries_.begin() ) ) {
            return false ;
        }
        if( !std::equal( sides_.begin(), sides_.end(),
            rhs.sides_.begin() ) ) {
            return false ;
        }
        if( nb_in_boundary() != rhs.nb_in_boundary() ) {
            return false ;
        }
        if( !std::equal( in_boundary_.begin(), in_boundary_.end(),
            rhs.in_boundary_.begin() ) ) {
            return false ;
        }
        if( parent_ != rhs.parent_ ) {
            return false ;
        }
        if( nb_children() != rhs.nb_children() ) {
            return false ;
        }
        if( !std::equal( children_.begin(), children_.end(),
            rhs.children_.begin() ) ) {
            return false ;
        }
        return true ;
    }


    bool BoundaryModelElement::is_connectivity_valid() const
    {
        /// 1. Check the model
        if( !has_model() ) return false ;

        /// 2. Check the validity of identification information
        ///    in the model
        if( bme_id() == bme_t() ) {
            return false ;
        }
        if( bme_id().index >= model().nb_elements( bme_id().type ) ) {
            return false ;
        }
        if( &( model().element( bme_id() ) ) != this ) {
            return false ;
        }

        /// 3. Check that required information for the TYPE is defined
        ///    and that reverse information is stored by the corresponding
        ///    elements
        TYPE T = bme_id().type ;

        // Boundaries
        if( boundary_allowed( T ) ) {
            if( T == REGION ) {
                if( nb_boundaries() == 0 ) {
                    return false ;
                }
                if( nb_boundaries() != sides_.size() ) {
                    return false ;
                }
            }
            // A Line must have 2 corners - they are identical if the Line is closed
            if( T == LINE && nb_boundaries() != 2 ) {
                return false ;
            }
            // No requirement on Surface - it may have no boundary - bubble

            // All elements in the boundary must have this in their
            // in_boundary vector
            for( index_t i = 0; i < nb_boundaries(); ++i ) {
                const BME& E = boundary( i ) ;
                bool found = false ;
                index_t j = 0 ;
                while( !found && j < E.nb_in_boundary() ) {
                    if( E.in_boundary_id( j ) == bme_id() ) {
                        found = true ;
                    }
                    j++ ;
                }
                if( !found ) {
                    return false ;
                }
            }
        }

        // In_boundary
        if( in_boundary_allowed( T ) ) {
            // Fix for a .ml for which VOI Surface are only on the boundary of Universe
            // Can we keep this ? Or should we compute the Region
            if( nb_in_boundary() == 0 ) {
                return false ;
            }

            // All elements in the in_boundary must have this in their
            // boundary vector
            for( index_t i = 0; i < nb_in_boundary(); ++i ) {
                const BME& E = in_boundary( i ) ;
                bool found = false ;
                index_t j = 0 ;
                while( !found && j < E.nb_boundaries() ) {
                    if( E.boundary_id( j ) == bme_id() ) {
                        found = true ;
                    }
                    j++ ;
                }
                if( !found ) {
                    return false ;
                }
            }
        }

        // Parent - High level elements are not mandatory
        // But if the model has elements of the parent type, the element must have a parent
        if( parent_allowed( T ) ) {
            if( model().nb_elements( parent_type( T ) ) > 0 ) {
                if( parent_id() == bme_t() ) {
                    return false ;
                }

                // The parent must have this element in its children
                const BME& E = parent() ;
                bool found = false ;
                index_t j = 0 ;
                while( !found && j < E.nb_children() ) {
                    if( E.child_id( j ) == bme_id() ) {
                        found = true ;
                    }
                    j++ ;
                }
                if( !found ) {
                    return false ;
                }
            }
        }

        // Children
        if( child_allowed( T ) ) {
            if( nb_children() == 0 ) {
                return false ;
            }

            // All children must have this element as a parent
            for( index_t i = 0; i < nb_children(); ++i ) {
                if( child( i ).parent_id() != bme_id() ) {
                    return false ;
                }
            }
        }

        // If no test failed, we are good to go.
        return true ;
    }


    /*!
     * @return Assert that the parent exists and returns it.
     */
    const BoundaryModelElement& BoundaryModelElement::parent() const
    {
        ringmesh_assert( parent_id().is_defined() ) ;
        return model_->element( parent_id() ) ;
    }


    /*!
     *
     * @param[in] x Index of the boundary element
     * @return Asserts that is exists and returns the element on the boundary
     */
    const BoundaryModelElement& BoundaryModelElement::boundary( index_t x ) const
    {
        ringmesh_assert( x < nb_boundaries() ) ;
        return model_->element( boundary_id( x ) ) ;
    }


    /*!
     *
     * @param[in] x Index of the in_boundary element
     * @return Asserts that it exists and returns the element in in_boundary.
     */
    const BoundaryModelElement& BoundaryModelElement::in_boundary( index_t x ) const
    {
        ringmesh_assert( x < nb_in_boundary() ) ;
        return model_->element( in_boundary_id( x ) ) ;
    }


    /*!
     *
     * @param[in] x Index of the child
     * @return Asserts that the child exists and returns it.
     */
    const BoundaryModelElement& BoundaryModelElement::child( index_t x ) const
    {
        ringmesh_assert( x < nb_children() ) ;
        return model_->element( child_id( x ) ) ;
    }


    /*!
     * @brief Copy all attributes except model_ from @param rhs to this element
     * @param[in] rhs To copy from
     * @param[in] model Model to associate to this element
     */
    void BoundaryModelElement::copy_macro_topology(
        const BoundaryModelElement& rhs,
        BoundaryModel& model )
    {
        model_ = &model ;
        id_ = rhs.id_ ;
        name_ = rhs.name_ ;
        geol_feature_ = rhs.geol_feature_ ;
        boundaries_ = rhs.boundaries_ ;
        sides_ = rhs.sides_ ;
        in_boundary_ = rhs.in_boundary_ ;
        parent_ = rhs.parent_ ;
        children_ = rhs.children_ ;
    }


    /*!
     * @brief Checks if this element or one of the element containing it
     * determines the model Volume Of Interest
     * @details This is known with the type of an element
     *
     * @todo To modify ? and test if the element ia around the universe region?
     */
    bool BoundaryModelElement::is_on_voi() const
    {
        if( geol_feature_ == NO_GEOL ) {
            for( index_t j = 0; j < nb_in_boundary(); ++j ) {
                GEOL_FEATURE t = in_boundary( j ).geological_feature() ;
                if( t == VOI || t == STRATI_VOI || t == FAULT_VOI ) {
                    return true ;
                }
            }
        } else if( geol_feature_ == VOI        ||
                   geol_feature_ == STRATI_VOI ||
                   geol_feature_ == FAULT_VOI ) {
            return true ;
        }
        return false ;
    }

    /*********************************************************************/

    BoundaryModelMeshElement::~BoundaryModelMeshElement()
    {
        unbind_attributes() ;
#ifdef RINGMESH_DEBUG
        Utils::print_bounded_attributes( mesh_ ) ;
#endif
    }

    /*!
     * @brief Returns the index of the first point corresponding to the input model index
     * @details Uses the attribute on the BoundaryModelVertices that stores the
     *  corresponding points in BME. Returns NO_ID if no matching point is found.
     *
     * @param model_vertex_id Index of a vertex in BoundaryModelVertices
     */
    index_t BoundaryModelMeshElement::local_id(
        index_t model_vertex_id ) const
    {
        typedef BoundaryModelVertices BMV ;
        const std::vector< BMV::VertexInBME >& bme_vertices =
            model_->vertices.bme_vertices( model_vertex_id ) ;

        for( index_t i = 0; i < bme_vertices.size(); i++ ) {
            const BMV::VertexInBME& info = bme_vertices[ i ] ;
            if( info.bme_id == bme_id() ) {
                return info.v_id ;
            }
        }
        return NO_ID ;
    }

    /*!
     * @brief Binds attributes stored by the BME on the Mesh
     */
    void BoundaryModelMeshElement::bind_attributes()
    {
        model_vertex_id_.bind( mesh_.vertices.attributes(), model_vertex_id_att_name ) ;
    }
    /*!
     * @brief Unbinds attributes stored by the BME on the Mesh
     */
    void BoundaryModelMeshElement::unbind_attributes()
    {
        model_vertex_id_.unbind() ;
    }

    /*!
     * @brief Sets the index of the matching point in the BoundaryModel
     *
     * @param[in] v Vertex index
     * @param[in] model_id Model vertex index in BoundaryModelVertices
     */
    void BoundaryModelMeshElement::set_model_vertex_id(
        index_t vertex_id,
        index_t model_vertex_id )
    {
        ringmesh_assert( vertex_id < nb_vertices() ) ;
        model_vertex_id_[ vertex_id ] = model_vertex_id ;
    }

    /*!
     * @brief Sets the geometrical position of a vertex
     *
     * @param index Index of the vertex to modify
     * @param point New coordinates
     * @param update If true, all the vertices sharing the same geometrical position
     *               in the BoundaryModel have their position updated, if false they
     *               are not.
     *
     * @warning Be careful with this update parameter, it is a very nice source of nasty bugs
     *          I removed on purpose the default value parameter (Jeanne).
     */
    void BoundaryModelMeshElement::set_vertex(
        index_t index,
        const vec3& point,
        bool update )
    {
        ringmesh_debug_assert( index < nb_vertices() ) ;
        if( update )
            model_->vertices.update_point(
            model_->vertices.unique_vertex_id(
            bme_id(), index ), point ) ;
        else
            mesh_.vertices.point( index ) = point ;
    }

    /*!
     * @brief Vertex index in model from local index
     * @param[in] p Vertex index
     * @return The vertex index in the model
     */
    index_t BoundaryModelMeshElement::model_vertex_id( index_t p ) const
    {
        ringmesh_assert( p < nb_vertices() ) ;
        ringmesh_debug_assert( model_vertex_id_[ p ] != NO_ID ) ;
        return model_vertex_id_[ p ] ;
    }

    /*!
     * @brief Returns the coordinates of the point at the given index in the surface
     * @param[in] surf_vertex_id Index of the vertex in the surface
     * @return The coordinates of the vertex
     */
    const vec3& BoundaryModelMeshElement::vertex( index_t v ) const
    {
        ringmesh_assert( v < nb_vertices() ) ;
        return mesh_.vertices.point( v ) ;
    }

    /*!
     * @brief Set the geometrical position of a vertex from a model vertex
     * @details Set also both mapping from (BoundaryModelVertices::unique2bme)
     *          and to (model_vertex_id_) the model vertex.
     *
     * @param index Index of the vertex to modify
     * @param model_vertex Index in BoundaryModelVertices of the vertex giving
     *                     the new position
     */
    void BoundaryModelMeshElement::set_vertex(
        index_t v, index_t model_vertex )
    {
        set_vertex( v, model_->vertices.unique_vertex( model_vertex ), false ) ;
        set_model_vertex_id( v, model_vertex ) ;
        model_->vertices.add_unique_to_bme( model_vertex, bme_id(), v ) ;
    }


    /*!
     * @brief Adds vertices to the mesh
     * @details No update of the model vertices is done
     *
     * @param points Geometric positions of the vertices to add
     * @param clear_mesh If true the mesh if cleared, keeping its attributes
     */
    void BoundaryModelMeshElement::set_vertices(
        const std::vector< vec3 >& points,
        bool clear )
    {
        // Clear the mesh, but keep the attributes and the space
        if( clear ) {
            mesh_.clear( true, true ) ;
        }
        mesh_.vertices.create_vertices( points.size() ) ;
        for( index_t v = 0; v < points.size(); v++ ) {
            set_vertex( v, points[ v ], false ) ;
        }
    }

    /*!
     * @brief Add vertices to the mesh
     * @details No update of the model vertices is done
     *
     * @param points Geometric positions of the vertices to add
     * @param clear_mesh If true the mesh if cleared, keeping its attributes
     */
    void BoundaryModelMeshElement::set_vertices(
        const std::vector< index_t >& model_vertices,
        bool clear )
    {
        // Clear the mesh, but keep the attributes and the space
        if( clear ) {
            mesh_.clear( true, true ) ;
        }
        mesh_.vertices.create_vertices( model_vertices.size() ) ;
        for( index_t v = 0; v < model_vertices.size(); v++ ) {
            set_vertex( v, model_vertices[ v ] ) ;
        }
    }


    /**************************************************************/

    bool Corner::is_mesh_valid() const 
    {
         return mesh_.vertices.nb() == 1 &&
             mesh_.edges.nb()    == 0 &&
             mesh_.facets.nb()   == 0 &&
             mesh_.cells.nb()    == 0 &&
             mesh_.vertices.point( 0 ) != vec3();
     }
    

    /***************************************************************/

    /*!
     * @brief Construct a Line
     *
     * @param[in] model The parent model
     * @param[in] id The index of the line in the lines_ vector of the parent model
     */
    Line::Line(
        BoundaryModel* model,
        index_t id ) :
        BoundaryModelMeshElement( model, LINE, id )
    {
    }

    bool Line::is_mesh_valid() const
    {
        /// 1. Check that the GEO::Mesh has the expected elements
        if( mesh_.vertices.nb() < 2 ) {
            return false ;
        }
        if( mesh_.edges.nb() == 0 ) {
            return false ;
        }
        if( mesh_.facets.nb() != 0 ) {
            return false ;
        }

        if( mesh_.cells.nb() != 0 ) {
            return false ;
        }

        /// 2. Check the validity of the vertices and edges

        // Model indices must be valid
        if( !check_range_model_vertex_ids( *this ) ) {
            return false ;
        }
        // Count the number of edges in which each vertex is
        std::vector< index_t > nb ;
        count_vertex_occurences( mesh(), nb ) ;
        index_t nb0 = 0 ;
        index_t nb1 = 0 ;
        index_t nb2 = 0 ;
        for( index_t i = 0; i < nb.size(); ++i ) {
            if( nb[ i ] == 0 ) ++nb0 ;
            else if( nb[ i ] == 1 ) ++nb1 ;
            else if( nb[ i ] == 2 ) ++nb2 ;
        }

        // Vertices at extremitites must be in only one edge
        if( nb.front() != 1 || nb.back() != 1 ) {
            return false ;
        }               
        // No isolated vertices are allowed
        if( nb0 > 0 ) {
            return false ;
        }
        // Only the two extremities are in only 1 edge 
        // One connected component condition
        if( nb1 != 2 ) {
            return false ;
        }
        // All the others must be in 2 edges and 2 edges only
        // Manifold condition
        if( nb2 != nb.size()-2 ) {
            return false ;
        }
        
        std::vector< index_t > duplicated ;
        // Only extremity vertex can be duplicated
        index_t nb_duplicated = detect_duplicated_vertices( *this, duplicated ) ;
        if( nb_duplicated > 2 ) {
            return false ;
        }
        // If it is the line is closed
        else if( nb_duplicated == 2 ) {
            if( !is_closed() ) {
                return false ;
            }
            if( duplicated.front() == NO_ID ) {
                return false ;
            }
            if( duplicated.front() != duplicated.back() ) {
                return false ;
            }
        }
        else {
            ringmesh_debug_assert( nb_duplicated == 0 ) ;
        }

        // No zero edge length - already ruled out with the duplicated vertex test
        // No self-intersection - let's say there are none
        // No duplicated edge - I would say it is also ruled out with the duplicated
        // vertex test JP
       
        return true ; 
    }


    /*!
     * @brief Adds vertices to the mesh and builds the edges
     * @details No update of the model vertices is done
     *
     * @param points Geometric positions of the vertices to add
     * @param clear_mesh If true the mesh is cleared keeping its attributes
     */
    void Line::set_vertices(
        const std::vector< vec3 >& points,
        bool clear_mesh )
    {
        BoundaryModelMeshElement::set_vertices( points, clear_mesh ) ;
        for( index_t e = 0; e < nb_vertices() - 1; e++ ) {
            mesh_.edges.create_edge( e, e + 1 ) ;
        }
    }

    /*!
     * @brief Adds vertices to the mesh and builds the edges
     * @details See set_vertex(index_t, index_t)
     *
     * @param model_vertices Indices in the model of the points to add
     * @param clear_mesh If true the mesh is cleared keeping its attributes
     */
    void Line::set_vertices(
        const std::vector< index_t >& model_vertices,
        bool clear_mesh )
    {
        BoundaryModelMeshElement::set_vertices( model_vertices, clear_mesh ) ;
        for( index_t e = 0; e < nb_vertices() - 1; e++ ) {
            mesh_.edges.create_edge( e, e + 1 ) ;
        }
    }

    /*!
     * @brief Check if the Line is twice on the boundary of a Surface
     *
     * @param[in] surface The surface to test
     */
    bool Line::is_inside_border( const BoundaryModelElement& surface ) const
    {
        // Find out if this surface is twice in the in_boundary vector
        return std::count( in_boundary_.begin(), in_boundary_.end(),
            surface.bme_id() ) > 1 ;
    }


    /*!
     *
     * @param[in] s Segment index
     * @return The coordinates of the barycenter of the segment
     */
    vec3 Line::segment_barycenter( index_t s ) const
    {
        return 0.5*( vertex( s ) + vertex( s+1 ) ) ;
    }


    /*!
     *
     * @param[in] s Segment index
     * @return The length of the segment
     */
    double Line::segment_length( index_t s ) const
    {
        return length( vertex( s + 1 ) - vertex( s ) ) ;
    }


    /*!
     *
     * @return The length of the Line
     */
    double Line::total_length() const
    {
        double result = 0 ;
        for( index_t s = 0; s < nb_cells(); s++ ) {
            result += segment_length( s ) ;
        }
        return result ;
    }


    /*!
     * @brief Returns true if the Line has exactly the given vertices
     *
     * @param[in] rhs_vertices Vertices to compare to
     */
    bool Line::equal( const std::vector< vec3 >& rhs_vertices ) const
    {
        if( nb_vertices() != rhs_vertices.size() ) {
            return false ;
        }

        bool equal = true ;
        for( index_t i = 0; i < nb_vertices(); i++ ) {
            if( rhs_vertices[i] != mesh_.vertices.point( i ) ) {
                equal = false ;
                break ;
            }
        }
        if( equal ) return true ;

        equal = true ;
        for( index_t i = 0; i < nb_vertices(); i++ ) {
            if( rhs_vertices[i] != mesh_.vertices.point( nb_vertices()-i-1 ) ) {
                equal = false ;
                break ;
            }
        }
        if( equal ) return true ;

        return false ;
    }


    /***********************************************************************/


    bool facet_is_degenerate( const Surface& S, index_t f )
    {
        std::vector< index_t > corners( S.nb_vertices_in_facet( f ) ) ;
        std::vector< index_t > corners_global( S.nb_vertices_in_facet( f ) ) ;
        int v = 0 ;
        for( index_t c = S.facet_begin( f ) ; c < S.facet_end( f ); ++c ) {
            corners[ v ] = c ;
            corners_global[ v ] = S.model_vertex_id( c ) ;
            v++ ;
        }
        std::sort( corners.begin(), corners.end() ) ;
        std::sort( corners_global.begin(), corners_global.end() ) ;
        return std::unique( corners.begin(), corners.end() ) != corners.end() ||
            std::unique( corners_global.begin(), corners_global.end() ) != corners_global.end() ;

        return false ; 
    }
    /********************************************************************/

    Surface::~Surface()
    {
    }

    
    bool Surface::is_mesh_valid() const
    {
        /// 1. Check that the GEO::Mesh has the expected elements
        ///    at least 3 vertices and one facet.
        if( mesh_.vertices.nb() < 3 ) {
            return false ;
        }
        // Is it important to have edges or not ?
        // I would say we do not care (JP) - so no check on that 
        if( mesh_.facets.nb() != 0 ) {
            return false ;
        }
        if( mesh_.cells.nb() == 0 ) {
            return false ;
        }

        /// 2. Check the validity of the facets 

        // No isolated vertices
        std::vector< index_t > nb ;
        count_vertex_occurences( mesh(), nb ) ;
        if( std::count( nb.begin(), nb.end(), 0 ) > 0 ) {
            return false ;
        }

        std::vector< index_t > duplicated ;
        index_t nb_duplicated = detect_duplicated_vertices( *this, duplicated ) ;

        // There might be several duplicated points, but they must be ine one of the
        // boundary lines that are twice in the boundary of this surface

        // No zero area facet
        // No facet incident to the same vertex check local and global indices
        index_t nb_degenerate = 0 ;
        for( index_t f = 0; f < mesh_.facets.nb(); f++ ) {
            if( facet_is_degenerate( *this, f ) ) {
                nb_degenerate++;
            }
        }
        if( nb_degenerate != 0 ) {
            std::cout << "Detected " << nb_degenerate << " degenerate facets in SURFACE "
                << bme_id().index << std::endl ;
            return false ;
        }


        // No duplicated facet
        // Waiting for mesh validity checks in geogram 
        /* GEO::vector< index_t > duplicated ;
           GEO::detect_duplicated_facets( mesh_, vector<index_t>& remove_f ) ;
           if( duplicated.size() > 0 ) {
                return false ;
           } 
        */
        
        // One connected component  
        if( GEO::mesh_nb_connected_components( mesh_ ) != 1 ) {
            return false ;
        }

        // No self-intersection - we need a triangulated mesh
        if( detect_intersecting_facets( *this ) > 0 ) {
            return false ;
        }

        // No non-manifold edges
        // No non-manifold points
        // Orientable surface - No #$@!$ Moebius allowed        
        // Boundary is a closed-manifold line - possibly several connected components
        // Planar polygonal facets 

        return true ; 
    }


    /*!
     * @param[in] f Facet index
     * @param[in] v Vertex index in the facet
     * @return The coordinates of the vertex
     */
    const vec3& Surface::vertex(
        index_t f,
        index_t v ) const
    {
        ringmesh_debug_assert( v < nb_vertices_in_facet( f ) ) ;
        return vertex( surf_vertex_id( f, v ) ) ;
    }
   

    index_t Surface::surf_vertex_id( index_t model_vertex_id ) const
    {
        return local_id( model_vertex_id ) ;
    }


    void Surface::set_geometry(
        const std::vector< vec3 >& vertices,
        const std::vector< index_t >& facets,
        const std::vector< index_t >& facet_ptr )
    {
        set_vertices( vertices ) ;
        set_geometry( facets, facet_ptr ) ;
    }


    void Surface::set_geometry(
        const std::vector< index_t >& model_vertex_ids,
        const std::vector< index_t >& facets,
        const std::vector< index_t >& facet_ptr )
    {
        set_vertices( model_vertex_ids ) ;
        set_geometry( facets, facet_ptr ) ;
    }


    void Surface::set_geometry(
        const std::vector< index_t >& facets,
        const std::vector< index_t >& facet_ptr )
    {
        for( index_t f = 0; f < facet_ptr.size()-1; f++ ) {
            index_t size = facet_ptr[f+1] - facet_ptr[f] ;
            GEO::vector< index_t > facet_vertices( size ) ;
            index_t start = facet_ptr[f] ;
            for( index_t lv = 0; lv < size; lv++ ) {
                facet_vertices[lv] = facets[start++] ;
            }
            mesh_.facets.create_polygon( facet_vertices ) ;
        }
    }
    
    
    /*!
     * @brief Traversal of a surface border
     * @details From the input facet f, get the facet that share vertex v and
     * get the indices of vertex v and of the following vertex in this new facet.
     * The next facet next_f may be the same, and from is required to avoid going back.
     *
     * @param[in] f Index of the facet
     * @param[in] from Index in the facet of the previous point on the border - gives the direction
     * @param[in] v Index in the facet of the point for which we want the next point on border
     * @param[out] next_f Index of the facet containing the next point on border
     * @param[out] v_in_next Index of vertex v in facet next_f
     * @param[out] next_in_next Index of the next vertex on border in facet v_in_next
     */
    void Surface::next_on_border(
        index_t f,
        index_t from,
        index_t v,
        index_t& next_f,
        index_t& v_in_next,
        index_t& next_in_next ) const
    {
        ringmesh_assert( v < nb_vertices_in_facet( f ) ) ;
        ringmesh_assert( is_on_border( f, v ) || is_on_border( f, from ) ) ;

        index_t V = surf_vertex_id( f, v ) ;

        // We want the next triangle that is on the boundary and share V
        // If there is no such triangle, the next vertex on the boundary
        // is the vertex of F neighbor of V that is not from

        // Get the facets around the shared vertex that are on the boundary
        // There must be one (the current one) or two (the next one on boundary)
        std::vector< index_t > facets ;
        index_t nb_around = facets_around_vertex( V, facets, true, f ) ;
        ringmesh_assert( nb_around < 3 && nb_around > 0 ) ;

        next_f = facets[ 0 ] ;

        if( nb_around == 2 ) {
            if( next_f == f ) {next_f = facets[ 1 ] ;}
            ringmesh_debug_assert( next_f != NO_ID ) ;

            // Now get the other vertex that is on the boundary opposite to p1
            v_in_next = facet_vertex_id( next_f, V ) ;
            ringmesh_assert( v_in_next != NO_ID ) ;

            // The edges containing V in next_f are
            // the edge starting at v_in_next and the one ending there
            index_t prev_v_in_next = prev_in_facet( next_f, v_in_next )  ;

            bool e0_on_boundary = is_on_border( next_f, v_in_next ) ;
            bool e1_on_boundary = is_on_border( next_f, prev_v_in_next ) ;

            // Only one must be on the boundary otherwise there is a corner missing
            ringmesh_assert( e0_on_boundary != e1_on_boundary ) ;

            // From the edge that is on boundary get the next vertex on this boundary
            // If the edge starting at p_in_next is on boundary, new_vertex is its next
            // If the edge ending at p_in_next is on boundary, new vertex is its prev
            next_in_next =
                e0_on_boundary ? next_in_facet( next_f, v_in_next ) : prev_v_in_next ;
        } else if( nb_around == 1 ) {
            // V must be in two border edges of facet f
            // Get the id in the facet of the vertex neighbor of v1 that is not v0
            v_in_next = v ;
            if( prev_in_facet( f, v ) == from  ) {
                ringmesh_debug_assert( is_on_border( f, v ) ) ;
                next_in_next = next_in_facet( f, v ) ;
            } else {
                ringmesh_debug_assert( is_on_border( f, prev_in_facet( f, v ) ) ) ;
                next_in_next = prev_in_facet( f, v ) ;
            }
        }
    }


    /*!
     * @brief Get the next edge on the border
     * @param[in] f Input facet index
     * @param[in] e Edge index in the facet
     * @param[out] next_f Next facet index
     * @param[out] next_e Next edge index in the facet
     */
    void Surface::next_on_border(
        index_t f,
        index_t e,
        index_t& next_f,
        index_t& next_e ) const
    {
        index_t v = next_in_facet( f, e ) ;
        return next_on_border( f, e, v, next_f, next_e ) ;
    }

    /*!
     * @brief Get the first facet of the surface that has an edge linking the two vertices (ids in the surface)
     *
     * @param[in] in0 Index of the first vertex in the surface
     * @param[in] in1 Index of the second vertex in the surface
     * @return NO_ID or the index of the facet
     */
    index_t Surface::facet_from_surface_vertex_ids(
        index_t in0,
        index_t in1 ) const
    {
        ringmesh_debug_assert( in0 < mesh_.vertices.nb() && in1 < mesh_.vertices.nb() ) ;

        // Another possible, probably faster, algorithm is to check if the 2 indices
        // are neighbors in facets_ and check that they are in the same facet

        // Check if the edge is in one of the facet
        for( index_t f = 0; f < nb_cells(); ++f ) {
            bool found = false ;
            index_t prev = surf_vertex_id( f, nb_vertices_in_facet( f ) - 1 ) ;
            for( index_t v = 0; v < nb_vertices_in_facet( f ); ++v ) {
                index_t p = surf_vertex_id( f, v ) ;
                if( ( prev == in0 && p == in1 ) ||
                    ( prev == in1 && p == in0 ) )
                {
                    found = true ;
                    break ;
                }
                prev = p ;
            }
            if( found ) {
                return f ;
            }
        }
        return NO_ID ;
    }


    /*!
     * @brief Get the first facet of the surface that has an edge linking the
     * two vertices (ids in the model)
     *
     * @param[in] i0 Index of the first vertex in the model
     * @param[in] i1 Index of the second vertex in the model
     * @return NO_ID or the index of the facet
     */
    index_t Surface::facet_from_model_vertex_ids(
        index_t i0,
        index_t i1 ) const
    {
        index_t facet = NO_ID ;
        index_t edge = NO_ID ;
        edge_from_model_vertex_ids( i0, i1, facet, edge ) ;
        return facet ;
    }


    /*!
     * @brief Determine the facet and the edge in this facet linking the 2 vertices
     * @details There might be two pairs facet-edge. This only gets the first.
     *
     * @param[in] i0 First vertex index in the model
     * @param[in] i1 Second vertex index in the model
     * @param[out] facet NO_ID or facet index in the surface
     * @param[out] edge NO_ID or edge index in the facet
     */
    void Surface::edge_from_model_vertex_ids(
        index_t i0,
        index_t i1,
        index_t& facet,
        index_t& edge ) const
    {
        edge = NO_ID ;

        // If a facet is given, look for the edge in this facet only
        if( facet != NO_ID ) {
            for( index_t v = 0; v < nb_vertices_in_facet( facet ); ++v ) {
                index_t prev = model_vertex_id( facet, prev_in_facet( facet, v )  ) ;
                index_t p = model_vertex_id( facet, v ) ;
                if( ( prev == i0 && p == i1 ) ||
                    ( prev == i1 && p == i0 ) )
                {
                    edge = prev_in_facet( facet, v ) ;
                    return ;
                }
            }
        } else {
            for( index_t f = 0; f < nb_cells(); ++f ) {
                facet = f ;
                edge_from_model_vertex_ids( i0, i1, facet, edge ) ;
                if( edge != NO_ID ) {return ;}
            }
        }

        // If we get here, no facet was found get out
        facet = NO_ID ;
        edge = NO_ID ;
    }


    /*!
     * @brief Determine the facet and the edge linking the 2 vertices with the same orientation
     * @details There might be two pairs facet-edge. This only gets the first.
     *
     * @param[in] i0 First vertex index in the model
     * @param[in] i1 Second vertex index in the model
     * @param[out] facet NO_ID or facet index in the surface
     * @param[out] edge NO_ID or edge index in the facet
     */
    void Surface::oriented_edge_from_model_vertex_ids(
        index_t i0,
        index_t i1,
        index_t& facet,
        index_t& edge ) const
    {
        // Copy from above .. tant pis
        edge = NO_ID ;

        // If a facet is given, look for the oriented edge in this facet only
        if( facet != NO_ID ) {
            for( index_t v = 0; v < nb_vertices_in_facet( facet ); ++v ) {
                index_t p = model_vertex_id( facet, v ) ;
                index_t next = model_vertex_id( facet, next_in_facet( facet, v )  ) ;

                if( p == i0 && next == i1 ) {
                    edge = v ;
                    return ;
                }
            }
        } else {
            for( index_t f = 0; f < nb_cells(); ++f ) {
                facet = f ;
                oriented_edge_from_model_vertex_ids( i0, i1, facet, edge ) ;
                if( edge != NO_ID ) {return ;}
            }
        }
        facet = NO_ID ;
    }


    /*!
     * @brief Convert vertex surface index to an index in a facet
     * @param[in] f Index of the facet
     * @param[in] surf_vertex_id_in Index of the vertex in the surface
     * @return NO_ID or index of the vertex in the facet
     */
    index_t Surface::facet_vertex_id(
        index_t f,
        index_t surf_vertex_id_in ) const
    {
        for( index_t v = 0; v < nb_vertices_in_facet( f ); v++ ) {
            if( surf_vertex_id( f, v ) == surf_vertex_id_in ) {return v ;}
        }
        return NO_ID ;
    }


    /*!
     * @brief Convert model vertex index to an index in a facet
     * @param[in] f Index of the facet
     * @param[in] model_v_id Index of the vertex in the BoundaryModel
     * @return NO_ID or index of the vertex in the facet
     */
    index_t Surface::facet_id_from_model(
        index_t f,
        index_t model_v_id ) const
    {
        for( index_t v = 0; v < nb_vertices_in_facet( f ); v++ ) {
            if( model_vertex_id( f, v ) == model_v_id ) {return v ;}
        }
        return NO_ID ;
    }


    /*!
     * @brief Comparator of two vec3
     */
    struct comp_vec3bis {
        bool operator()(
            const vec3& l,
            const vec3& r ) const
        {
            if( l.x != r.x ) {return l.x < r.x ;}
            if( l.y != r.y ) {return l.y < r.y ;}
            return l.z < r.z ;
        }
    } ;

    /*!
     * @brief Determines the facets around a vertex
     *
     * @param[in] shared_vertex Index ot the vertex in the surface
     * @param[in] result Indices of the facets containing @param shared_vertex
     * @param[in] border_only If true only facets on the border are considered
     * @return The number of facet found
     *
     * \todo Evaluate if this is fast enough !!
     */
    index_t Surface::facets_around_vertex(
        index_t shared_vertex,
        std::vector< index_t >& result,
        bool border_only ) const
    {
        result.resize( 0 ) ;
        for( index_t t = 0; t < nb_cells(); ++t ) {
            for( index_t v = 0; v < nb_vertices_in_facet( t ); v++ ) {
                if( surf_vertex_id( t, v ) == shared_vertex ) {
                    return facets_around_vertex( shared_vertex, result,
                        border_only, t ) ;
                }
            }
        }
        ringmesh_assert_not_reached ;
        return dummy_index_t ;
    }


    /*!
     * @brief Determines the facets around a vertex
     *
     * @param[in] P Index ot the vertex in the surface
     * @param[in] result Indices of the facets containing @param P
     * @param[in] border_only If true only facets on the border are considered
     * @param[in] f0 Index of one facet containing the vertex @param P
     * @return The number of facet found
     *
     * \todo Evaluate if this is fast enough !!
     */
    index_t Surface::facets_around_vertex(
        index_t P,
        std::vector< index_t >& result,
        bool border_only,
        index_t f0 ) const
    {
        result.resize( 0 ) ;

        // Flag the visited facets
        std::vector< index_t > visited ;
        visited.reserve( 10 ) ;

        // Stack of the adjacent facets
        std::stack< index_t > S ;
        S.push( f0 ) ;
        visited.push_back( f0 ) ;

        do {
            index_t t = S.top() ;
            S.pop() ;

            for( index_t v = 0; v < nb_vertices_in_facet( t ); ++v ) {
                if( surf_vertex_id( t, v ) == P ) {
                    index_t adj_P = adjacent( t, v ) ;
                    index_t prev = prev_in_facet( t, v ) ;
                    index_t adj_prev = adjacent( t, prev ) ;

                    if( adj_P != NO_ADJACENT ) {
                        // The edge starting at P is not on the boundary
                        if( !Utils::contains( visited, adj_P ) ) {
                            S.push( adj_P ) ;
                            visited.push_back( adj_P ) ;
                        }
                    }
                    if( adj_prev != NO_ADJACENT ) {
                        // The edge ending at P is not on the boundary
                        if( !Utils::contains( visited, adj_prev ) ) {
                            S.push( adj_prev ) ;
                            visited.push_back( adj_prev  ) ;
                        }
                    }

                    if( border_only ) {
                        if( adj_P == NO_ADJACENT || adj_prev == NO_ADJACENT ) {
                            result.push_back( t ) ;
                        }
                    } else {result.push_back( t ) ;}

                    // We are done with this facet
                    break ;
                }
            }
        } while( !S.empty() ) ;

        return result.size() ;
    }


    /*!
     * @brief Compute the barycenter of a facet
     * @param[in] f Facet index in the surface
     * @return The coordinates of the facet barycenter
     */
    vec3 Surface::facet_barycenter( index_t f ) const
    {
        vec3 barycenter( 0., 0., 0. ) ;
        for( index_t i = 0; i < nb_vertices_in_facet( f ); i++ ) {
            barycenter += vertex( f, i ) ;
        }
        return barycenter / nb_vertices_in_facet( f ) ;
    }


    /*!
     * @brief Compute the area of a facet
     * @param[in] f Facet index in the surface
     * @return The area of the facet
     */
    double Surface::facet_area( index_t f ) const
    {
        double result = 0 ;
        for( index_t i = 1; i + 1 < nb_vertices_in_facet( f ); i++ ) {
            result += Utils::triangle_area(
                vertex( f, 0 ), vertex( f, i ), vertex( f, i + 1 ) ) ;
        }
        return result ;
    }


    /*!
     *
     * @param[in] f Facet index
     * @return Normal to the triangle made by the first 3 vertices
     * of the facet
     *
     * @warning If the facet is not planar calling this has no meaning
     */
    vec3 Surface::facet_normal( index_t f ) const
    {
        const vec3& p0 = vertex( f, 0 )  ;
        const vec3& p1 = vertex( f, 1 )  ;
        const vec3& p2 = vertex( f, 2 )  ;
        vec3 c0 = cross( p0 - p2, p1 - p2 ) ;
        return normalize( c0 ) ;
    }


    /*!
     * @brief Compute the normal to the surface vertices
     * @details The normal at a point is computed as the mean of the normal
     * to its adjacent facets.
     *
     * @param[out] normals Coordinates of the normal vectors to the vertices
     */
    void Surface::vertex_normals( std::vector< vec3 >& normals ) const
    {
        normals.resize( nb_vertices() ) ;
        for( index_t f = 0; f < nb_cells(); f++ ) {
            vec3 normal = facet_normal( f ) ;
            for( index_t p = 0; p < nb_vertices_in_facet( f ); p++ ) {
                index_t id = surf_vertex_id( f, p ) ;
                normals[ id ] += normal ;
            }
        }
        for( index_t p = 0; p < nb_vertices(); p++ ) {
            normals[ p ] = normalize( normals[ p ] ) ;
        }
    }


    /*!
     * @brief Compute closest vertex in a facet to a point
     * @param[in] f Facet index
     * @param[in] v Coordinates of the point to which distance is measured
     * @return Index of the vertex of @param f closest to @param v
     */
    index_t Surface::closest_vertex_in_facet(
        index_t f,
        const vec3& v ) const
    {
        index_t result = 0 ;
        double dist = DBL_MAX ;
        for( index_t p = 0; p < nb_vertices_in_facet( f ); p++ ) {
            double distance = length2( v - vertex( f, p ) ) ;
            if( dist > distance ) {
                dist = distance ;
                result = p ;
            }
        }
        return result ;
    }


    vec3 Surface::edge_barycenter( index_t c ) const
    {
        vec3 result( 0, 0, 0 ) ;

        // Get the facet index
        index_t f = NO_ID ;
        for( ; f < mesh_.facets.nb(); f++ ) {
            if( mesh_.facets.corners_begin(f ) < c  ) {
                break ;
            }
        }
        ringmesh_debug_assert( f != NO_ID ) ;
        index_t v = c - facet_begin( f ) ;
        result += vertex( f, v ) ;
        result += vertex( f, next_in_facet( f, v ) ) ;
        return .5 * result ;
    }


    
    bool is_corner_to_duplicate( const BoundaryModel& BM, index_t corner_id )
    {
        if( BM.corner( corner_id ).nb_in_boundary() > 3 ) {
            return true ;
        } else {
            return false ;
        }
    }

    void update_facet_corner( 
        Surface& S, 
        const std::vector< index_t >& facets, 
        index_t old, 
        index_t neu )
    {
        for( index_t i = 0; i < facets.size(); ++i ) {
            index_t cur_f = facets[ i ] ;
            for( index_t cur_v = 0;
                    cur_v < S.nb_vertices_in_facet( cur_f );
                    cur_v++ )
            {
                if( S.surf_vertex_id( cur_f, cur_v ) == old ) {
                    S.mesh().facets.set_vertex( cur_f, cur_v, neu) ;
                }                   
            }
        }
    }


    /*!
     * @brief Cut a Surface along a Line assuming that the edges of the Line are edges of the Surface
     *  
     * @details First modify to NO_ADJACENT the neighbors along Line edges
     * and then duplicate the points along this new boundary.
     * Duplicate the corner that should be if any.
     * 
     * @pre The Line must not cut the Surface into 2 connected components
     *
     * @param[in] L The Line
     */
    void Surface::cut_by_line( const Line& L )
    {
        for( index_t i = 0; i + 1 < L.nb_vertices(); ++i ) {
            index_t p0 = L.model_vertex_id( i ) ;
            index_t p1 = ( i == L.nb_vertices()-1 ) ? 
                L.model_vertex_id(0) : L.model_vertex_id(i+1) ;

            index_t f = Surface::NO_ID ;
            index_t v = Surface::NO_ID ;
            edge_from_model_vertex_ids( p0, p1, f, v ) ;
            ringmesh_debug_assert( f != Surface::NO_ID && v != Surface::NO_ID ) ;

            index_t f2 = adjacent( f, v ) ;
            index_t v2 = Surface::NO_ID ;
            ringmesh_debug_assert( f2 != Surface::NO_ADJACENT ) ;
            edge_from_model_vertex_ids( p0, p1, f2, v2 ) ;
            ringmesh_debug_assert( v2 != Surface::NO_ID ) ;

            // Virtual cut - set adjacencies to NO_ADJACENT
            set_adjacent( f, v, Surface::NO_ADJACENT ) ;
            set_adjacent( f2, v2, Surface::NO_ADJACENT ) ;
        }

        BoundaryModel& M = const_cast< BoundaryModel& >( model() ) ;

        // Now travel on one side of the "faked" boundary and actually duplicate
        // the vertices in the surface
        // Get started in the surface - find (again) one of the edge that contains
        // the first two vertices of the line
        index_t f = Surface::NO_ID ;
        index_t v = Surface::NO_ID ;
        oriented_edge_from_model_vertex_ids(
            L.model_vertex_id(0), L.model_vertex_id(1), f, v ) ;
        ringmesh_assert( f != Surface::NO_ID && v != Surface::NO_ID ) ;

        index_t id0 = surf_vertex_id( f, v ) ;
        index_t id1 = surf_vertex_id( f, next_in_facet( f, v ) ) ;

        // Stopping criterion
        index_t c0 = L.boundary_id(0).index ;
        index_t c1 = L.boundary_id(1).index ;

        // Wee need to check if we have to duplicate the Corner or not
        // the 2 corners are         
        bool duplicate_c0 = is_corner_to_duplicate( model(), c0 ) ;
        bool duplicate_c1 = is_corner_to_duplicate( model(), c1 ) ;
        // If both shall be duplicated - the line cut completely the surface
        // and this function is not supposed to deal with that situation
        ringmesh_assert( !duplicate_c0 || !duplicate_c1 ) ;
        
        // Index of the model vertex if one corner is to duplicate
        index_t m_corner = duplicate_c0 ? M.corner(c0).model_vertex_id() :
            (duplicate_c1 ? M.corner(c1).model_vertex_id() : NO_ID ) ;

        // Index of the vertex to duplicate in S
        // If c1 is to duplicate we do not know it yet 
        index_t s_corner = duplicate_c0 ? id0 : NO_ID ;

        // Index of the new vertex for the corner in the surface
        index_t s_new_corner = NO_ID ;
        // Create this new point in the surface and set mapping with point in the BM
        if( m_corner != NO_ID ) {
            s_new_corner = mesh_.vertices.create_vertex( M.vertices.unique_vertex( m_corner ).data() ) ;
            set_model_vertex_id( s_new_corner, m_corner ) ;           
            M.vertices.add_unique_to_bme( m_corner, bme_id(), s_new_corner ) ;
        }
            
        /// \todo Check that all vertices on the line are recovered
        while( model_vertex_id( id1 ) != M.corner(c1).model_vertex_id() ) {
            // Get the next vertex on the border
            // Same algorithm than in determine_line_vertices function
            index_t next_f = Surface::NO_ID ;
            index_t id1_in_next = Surface::NO_ID ;
            index_t next_id1_in_next = Surface::NO_ID ;

            // Get the next facet and next triangle on this boundary
            next_on_border( f,
                facet_vertex_id( f, id0 ), facet_vertex_id( f, id1 ),
                next_f, id1_in_next, next_id1_in_next ) ;
            ringmesh_assert(
                next_f != Surface::NO_ID && id1_in_next != Surface::NO_ID
                && next_id1_in_next != Surface::NO_ID ) ;

            index_t next_id1 = surf_vertex_id( next_f, next_id1_in_next ) ;

            // Duplicate the vertex at id1
            // After having determined the next 1 we can probably get both at the same time
            // but I am lazy, and we must be careful not to break next_on_border function (Jeanne)
            std::vector< index_t > facets_around_id1 ;
            facets_around_vertex( id1, facets_around_id1, false, f ) ;

            // Duplicate the vertex in the surface
            index_t new_id1 = mesh_.vertices.create_vertex( vertex( id1 ).data() ) ;
            // Set its model vertex index
            set_model_vertex_id( new_id1, model_vertex_id( id1 ) ) ;
            // Add the mapping from in the model vertices. Should we do this one ?
            M.vertices.add_unique_to_bme( model_vertex_id(id1), bme_id(), new_id1 ) ;

            // Update vertex index in facets 
            update_facet_corner( *this, facets_around_id1, id1, new_id1 ) ;

            // Update
            f = next_f ;
            id0 = new_id1 ;
            id1 = next_id1 ;
        }       
        if( m_corner != NO_ID ){
            if( duplicate_c1 ) {
               s_corner = id1 ;
            }
            ringmesh_assert( s_corner != NO_ID && s_new_corner != NO_ID ) ;            
            std::vector< index_t > facets_around_c ;
            facets_around_vertex( s_corner, facets_around_c, false ) ;            
            update_facet_corner( *this, facets_around_c, s_corner, s_new_corner ) ;
        }
    }





    SurfaceTools::SurfaceTools( const Surface& surface )
        : surface_( surface ), aabb_( nil ), ann_( nil )
    {
    }


    SurfaceTools::~SurfaceTools()
    {
        if( aabb_ ) delete aabb_ ;
        if( ann_ ) delete ann_ ;
    }

    const GEO::MeshFacetsAABB& SurfaceTools::aabb() const
    {
        if( !aabb_ ) {
            SurfaceTools* this_not_const = const_cast< SurfaceTools* >( this ) ;
            this_not_const->aabb_ = new GEO::MeshFacetsAABB(
                const_cast< GEO::Mesh& >( surface_.mesh() ) ) ;
            surface_.model_->vertices.clear() ;
            if( ann_ ) {
                delete ann_ ;
                this_not_const->ann_ = nil ;
            }
        }
        return *aabb_ ;
    }

    const ColocaterANN& SurfaceTools::ann() const
    {
        if( !ann_ ) {
            const_cast< SurfaceTools* >( this )->ann_ = new ColocaterANN(
                surface_.mesh(), ColocaterANN::VERTICES ) ;
        }
        return *ann_ ;
    }



    /*!
     * @brief Compute the size (volume, area, length) of an Element
     *
     * @param[in] E Element to evaluate
     */
    double BoundaryModelElementMeasure::size( const BoundaryModelElement* E )
    {
        double result = 0. ;

        /// If this element has children sum up their sizes
        for( index_t i = 0; i < E->nb_children(); ++i ) {
            result += BoundaryModelElementMeasure::size( &E->child( i ) )  ;
        }
        if( result != 0 ) {return result ;}

        /// Else it is a base element and its size is computed

        // If this is a region
        if( E->bme_id().type == BoundaryModelElement::REGION ) {
            // Compute the volume if this is a region
            for( index_t i = 0; i < E->nb_boundaries(); i++ ) {
                const Surface& surface =
                    dynamic_cast< const Surface& >( E->boundary( i ) ) ;

                for( index_t t = 0; t < surface.nb_cells(); t++ ) {
                    const vec3& p0 = surface.vertex( t, 0 ) ;
                    for( index_t v = 1;
                         v + 1 < surface.nb_vertices_in_facet( t );
                         ++v )
                    {
                        double cur_volume = ( dot( p0,
                                                  cross( surface.vertex( t,
                                                          v ),
                                                      surface.vertex( t, v + 1 ) ) ) )
                                            / static_cast< double >( 6 ) ;
                        E->side( i ) ? result -= cur_volume : result += cur_volume ;
                    }
                }
            }
            return fabs( result ) ;
        } else if( E->bme_id().type == BoundaryModelElement::CORNER ) {
            return 0 ;
        } else if( E->bme_id().type == BoundaryModelElement::LINE ) {
            const Line* L = dynamic_cast< const Line* >( E ) ;
            ringmesh_assert( L != nil ) ;
            for( index_t i = 1; i < E->nb_vertices(); ++i ) {
                result += GEO::Geom::distance( E->vertex( i ), E->vertex( i - 1 ) ) ;
            }
            return result ;
        } else if( E->bme_id().type == BoundaryModelElement::SURFACE ) {
            const Surface* S = dynamic_cast< const Surface* >( E ) ;
            ringmesh_assert( S != nil ) ;

            for( index_t i = 0; i < S->nb_cells(); i++ ) {
                result += S->facet_area( i ) ;
            }
            return result ;
        }
        ringmesh_assert_not_reached ;
        return result ;
    }


    /*!
     * @brief Compute the barycenter of a part of a BoundaryModelElement
     * Only implemented for Surface and Line
     *
     * @param[in] E Pointer to the element
     * @param[in] cells Indices of the segments/facets to consider
     * @return The coordinates of the barycenter of the @param cells
     */
    vec3 BoundaryModelElementMeasure::barycenter(
        const BoundaryModelElement* E,
        const std::vector< index_t >& cells )
    {
        vec3 result( 0., 0., 0. ) ;
        double size = 0 ;

        const Line* L = dynamic_cast< const Line* >( E ) ;
        if( L != nil ) {
            for( index_t i = 0; i < cells.size(); ++i ) {
                result += L->segment_length( cells[ i ] ) * L->segment_barycenter(
                    cells[ i ] ) ;
                size   += L->segment_length( cells[ i ] ) ;
            }
            return size > epsilon ? result / size : result ;
        }
        const Surface* S = dynamic_cast< const Surface* >( E ) ;
        if( S != nil ) {
            for( index_t i = 0; i < cells.size(); ++i ) {
                result += S->facet_area( cells[ i ] ) *
                          S->facet_barycenter( cells[ i ] ) ;
                size   += S->facet_area( cells[ i ] ) ;
            }
            return size > epsilon ? result / size : result ;
        }
        ringmesh_assert_not_reached ;
        return result ;
    }


    /*!
     * @brief Measures the minimal distance between an element and a point
     * Implement only for Surface, Line and Corner
     *
     * @param[in] E Pointer to the element
     * @param[in] p Coordinates of the point to which distance is measured
     */
    double BoundaryModelElementMeasure::distance(
        const BoundaryModelElement* E,
        const vec3& p )
    {
        double result = FLT_MAX ;
        const Line* L = dynamic_cast< const Line* >( E ) ;
        if( L != nil ) {
            for( index_t i = 1; i < L->nb_vertices(); ++i ) {
                // Distance between a vertex and a segment
                const vec3& p0 = L->vertex( i - 1 ) ;
                const vec3& p1 = L->vertex( i ) ;

                double distance_pt_2_segment  = FLT_MAX ;
                vec3 c = ( p1 - p0 ) / 2 ;
                double half = GEO::distance( p1, c ) ;
                double cp_dot_p0p1 = dot( p - c, p1 - p0 ) ;

                if( cp_dot_p0p1 < - half ) {
                    distance_pt_2_segment =  GEO::distance(
                        p0,
                        p ) ;
                } else if( cp_dot_p0p1 >
                           half )
                {
                    distance_pt_2_segment = GEO::distance( p1, p ) ;
                } else {
                    vec3 projection = c + cp_dot_p0p1 * ( p1 - p0 ) ;
                    distance_pt_2_segment = GEO::distance( projection, p ) ;
                }
                result = distance_pt_2_segment <
                         result ? distance_pt_2_segment : result ;
            }
            return result ;
        }
        const Surface* S = dynamic_cast< const Surface* >( E ) ;
        if( S != nil ) {
            for( index_t i = 0; i < S->nb_cells(); i++ ) {
                for( index_t j = 1; j + 1 < S->nb_vertices_in_facet( i ); ++j ) {
                    double cur = GEO::Geom::point_triangle_squared_distance(
                        p, S->vertex( i, 0 ), S->vertex( i, j ), S->vertex( i, j + 1 ) ) ;
                    if( cur < result ) {result = cur ;}
                }
            }
            if( result != FLT_MAX ) {result = sqrt( result ) ;}
            return result ;
        }

        const Corner* C = dynamic_cast < const Corner* >( E ) ;
        if( C != nil ) {
            return GEO::distance( C->vertex(), p ) ;
        }

        // If it is not one of the basic types - compute it for the children
        // if any
        if( E->nb_children() == 0 ) {
            ringmesh_assert_not_reached ;
            return result ;
        } else {
            for( index_t i = 0; i < E->nb_children(); ++i ) {
                double dist = distance( &E->child( i ), p ) ;
                result = ( dist < result ) ? dist : result ;
            }
            return result ;
        }
    }
}
