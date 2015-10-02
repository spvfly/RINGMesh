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
*     http://www.ring-team.org
*
*     RING Project
*     Ecole Nationale Sup�rieure de G�ologie - Georessources
*     2 Rue du Doyen Marcel Roubault - TSA 70605
*     54518 VANDOEUVRE-LES-NANCY
*     FRANCE
*/

#include <ringmesh/geo_model_editor.h>

#include <algorithm>
namespace RINGMesh {

    typedef GeoModelElement::gme_t gme_t ;

    /*!
    * @brief Mini-factory. Create an empty element of the right type
    */
    GME* new_element( GME::TYPE T, GeoModel* M, index_t id )
    {

        if( T == GME::CORNER ) {
            return new Corner( M, id ) ;
        } else if( T == GME::LINE ) {
            return new Line( M, id ) ;
        } else if( T == GME::SURFACE ) {
            return new Surface( M, id ) ;
        } else if( T == GME::REGION ) {
            return new Region( M, id ) ;
        } else if( T > GME::REGION && T < GME::NO_TYPE ) {
            return new GeoModelElement( M, T, id ) ;
        } else {
            return nil ;
        }
    }

    /*!
    * @brief Creates a element of the given type and add it to the correct vector
    * The GeoModelElement is created from its type and its index
    *
    * @param[in] type Type of the element to create
    * @return The index of the created element
    */
    gme_t GeoModelEditor::create_element( GME::TYPE type )
    {
        index_t id = model_.nb_elements( type ) ;
        ringmesh_assert( id != NO_ID ) ;
        if( type >= GME::CORNER && type < GME::NO_TYPE ) {
            model_.modifiable_elements( type ).push_back(
                new_element( type, &model_, id ) ) ;
            return gme_t( type, id ) ;
        } else {
            ringmesh_assert_not_reached;
            return gme_t() ;
        }
    }



    void GeoModelEditor::resize_elements( GME::TYPE type, index_t nb )
    {
        if( type >= GME::NO_TYPE ) {
            return ;
        }
        std::vector< GME* >& store = model_.modifiable_elements( type ) ;
        store.resize( nb, nil ) ;
        for( index_t i = 0; i < nb; i++ ) {
            store[ i ] = new_element( type, &model_, i ) ;
        }
    }

    /*!
    * @brief Fill the model universe_
    *
    * @param[in] boundaries Indices of the surfaces on the model boundary
    * plus indication on which side of the surface is universe_ ( outside of the model )
    */
    void GeoModelEditor::set_universe(
        const std::vector< std::pair< index_t, bool > >& boundaries )
    {
        Region& U = model_.universe_ ;
        U.set_name( "Universe" ) ;
        U.set_element_type( GME::REGION ) ;
        U.set_model( &model_ ) ;

        for( index_t i = 0; i < boundaries.size(); ++i ) {
            ringmesh_assert( boundaries[ i ].first < model_.nb_surfaces() ) ;
            U.add_boundary(
                gme_t( GME::SURFACE, boundaries[ i ].first ), boundaries[ i ].second ) ;
        }
    }

    /*!
    * @brief Add to the vector the elements which cannot exist if
    *        an element in the set does not exist.
    * @details These elements are added to the set.
    *          Recursive call till nothing is added.
    *
    * @return True if at least one element was added, otherwise false.
    */
    bool GeoModelEditor::get_dependent_elements( std::set< gme_t >& in ) const
    {
        index_t input_size = in.size() ;

        for( std::set< gme_t >::iterator it( in.begin() ); it != in.end(); ++it ) {
            gme_t cur = *it ;
            /// If an element has children elements - add them 
            if( GME::child_allowed( cur.type ) ) {
                const GME& E = model_.element( cur ) ;
                for( index_t j = 0; j < E.nb_children(); ++j ) {
                    in.insert( E.child_id( j ) ) ;
                }
            }
        }

        /// If a parent has no children anymore - add it 
        for( index_t p = GME::CONTACT; p < GME::NO_TYPE; ++p ) {
            GME::TYPE P = ( GME::TYPE ) p ;
            for( index_t j = 0; j < model_.nb_elements( P ); ++j ) {
                bool no_child = true ;
                const GME& E = model_.element( GME::gme_t( P, j ) ) ;
                for( index_t k = 0; k < E.nb_children(); ++k ) {
                    if( in.count( E.child_id( k ) ) == 0 ) {
                        no_child = false ;
                        break ;
                    }
                }
                if( no_child ) {
                    in.insert( E.gme_id() ) ;
                }
            }
        }

        /// If an element is in the boundary of nothing - add it
        for( index_t t = GME::CORNER; t < GME::REGION; ++t ) {
            GME::TYPE T = ( GME::TYPE ) t ;
            for( index_t j = 0; j < model_.nb_elements( T ); ++j ) {
                bool no_incident = true ;
                const GME& E = model_.element( GME::gme_t( T, j ) ) ;
                for( index_t k = 0; k < E.nb_in_boundary(); ++k ) {
                    if( in.count( E.in_boundary_id( k ) ) == 0 ) {
                        no_incident = false ;
                        break ;
                    }
                }
                if( no_incident ) {
                    in.insert( E.gme_id() ) ;
                }
            }
        }

        if( in.size() != input_size ) {
            return get_dependent_elements( in ) ;
        } else {
            return false ;
        }
    }

    /*!
    * @brief Remove a list of elements of the model
    * @details No check is done on the consistency of this removal
    *          The elements and all references to them are removed.
    *          All dependent elements should be in the set of elements to remove,
    *          with a prior call to get_dependent_elements function.
    *
    * @warning NOT TESTED.
    *          The client is responsible to set the proper connectivity
    *          information between the remaining model elements.
    *
    * @todo TEST IT
    */
    void GeoModelEditor::remove_elements( const std::set< gme_t >& elements )
    {
        if( elements.size() == 0 ) {
            return ;
        }

        // We need to remove elements type by type since they are 
        // stored in different vectors and since we use indices in these 
        // vectors to identify them.
        // Initialize the vector
        std::vector< std::vector< index_t > > to_erase_by_type ;
        to_erase_by_type.reserve( GME::NO_TYPE ) ;
        for( index_t i = GME::CORNER; i < GME::NO_TYPE; ++i ) {
            to_erase_by_type.push_back(
                std::vector< index_t >(
                model_.nb_elements( static_cast<GME::TYPE>( i ) ), 0 ) ) ;
        }
        // Flag the elements to erase
        for( std::set< gme_t >::const_iterator it = elements.begin();
             it != elements.end(); ++it ) {
            gme_t cur = *it ;
            if( cur.type < GME::NO_TYPE ) {
                ringmesh_debug_assert( NO_ID != 0 ) ; // If one day NO_ID changes of value.
                to_erase_by_type[ cur.type ][ cur.index ] = NO_ID ;
            }
        }

        delete_elements( to_erase_by_type ) ;

        // Re-initialize global access to elements
        model_.init_global_model_element_access() ;
    }

    /*!
    * @brief ONGOING WORK. Removes properly some elements of the Boundary Model.
    *
    * @param[in] elements: in input the elements the client wants to
    * remove, in output all the removed elements (dependencies of ).
    *
    * Calls get_dependent_elements on each elements of \p elements_to_remove.
    * Then do remove these elements and updates the universe.
    *
    * @pre Assert that the element to remove is one region that has only one neighbor
    *
    * @todo Finish to implement it for any kind of BME. BC must continue this work.
    *
    * @todo Review : Error in the comments [JP]
    */
    void GeoModelEditor::remove_elements_and_dependencies(
        const std::set< GME::gme_t >& elements_to_remove )
    {
        // Asserts to remove when implementation is completed
        ringmesh_assert( elements_to_remove.size() == 1 &&
                         elements_to_remove.begin()->type == GME::REGION ) ;


        // Copy because it is not logical to have in output the removed elements. BC
        /// @todo Review : youpiii what did the comment on the function just said ? [JP]
        std::set< GME::gme_t > elements = elements_to_remove;
        // TODO Handle the case of several objects in elements

        const GeoModelElement& reg = model_.element( *( elements.begin() ) ) ;
        get_dependent_elements( elements ) ;

        // TODO Dirty duplication of code------------------------
        // We need to remove elements type by type since they are
        // stored in different vectors and since we use indices in these
        // vectors to identify them.
        // Initialize the vector
        std::vector< std::vector< index_t > > to_erase_by_type ;
        to_erase_by_type.reserve( GME::NO_TYPE ) ;
        for( index_t i = GME::CORNER; i < GME::NO_TYPE; ++i ) {
            to_erase_by_type.push_back(
                std::vector< index_t >(
                model_.nb_elements( static_cast<GME::TYPE>( i ) ), 0 ) ) ;
        }
        // Flag the elements to erase
        for( std::set< gme_t >::const_iterator it = elements.begin();
             it != elements.end(); ++it ) {
            gme_t cur = *it ;
            if( cur.type < GME::NO_TYPE ) {
                ringmesh_debug_assert( NO_ID != 0 ) ; // If one day NO_ID changes of value.
                to_erase_by_type[ cur.type ][ cur.index ] = NO_ID ;
            }
        }

        // Number of elements deleted for each TYPE
        std::vector< index_t > nb_removed( to_erase_by_type.size(), 0 ) ;

        /// 1. Get the mapping between old indices of the elements
        ///    and new ones (when elements to remove will actually be removed)
        for( index_t i = 0; i < to_erase_by_type.size(); ++i ) {
            for( index_t j = 0; j < to_erase_by_type[ i ].size(); ++j ) {
                if( to_erase_by_type[ i ][ j ] == NO_ID ) {
                    nb_removed[ i ]++ ;
                } else {
                    to_erase_by_type[ i ][ j ] = j - nb_removed[ i ] ;
                }
            }
        }
        // TODO Dirty duplication of code--------------------------

        std::vector< GME::gme_t > to_add_in_universe ;

        if( reg.gme_id().type == GME::REGION ) {
            index_t nb_added = 0 ;
            for( index_t b_i = 0; b_i < reg.nb_boundaries(); ++b_i ) {
                if( !reg.boundary( b_i ).is_on_voi() ) {
                    to_add_in_universe.push_back( reg.boundary( b_i ).gme_id() ) ;
                    ringmesh_debug_assert(
                        to_erase_by_type[ reg.boundary( b_i ).gme_id().type ][ reg.boundary(
                        b_i ).gme_id().index ] != NO_ID ) ;
                    to_add_in_universe[ nb_added ].index =
                        to_erase_by_type[ reg.boundary( b_i ).gme_id().type ][ reg.boundary(
                        b_i ).gme_id().index ] ;
                    ++nb_added ;
                }
            }
        }

        remove_elements( elements ) ;


        // Update Universe
        std::vector< std::pair< index_t, bool > > oriented_surfaces ;

        for( std::vector< GME::gme_t >::const_iterator itr =
             to_add_in_universe.begin(); itr != to_add_in_universe.end(); ++itr ) {
            oriented_surfaces.push_back( std::pair< index_t, bool >( itr->index, true ) ) ;
        }
        set_universe( oriented_surfaces ) ;

        ringmesh_debug_assert( model_.check_model_validity() ) ;
    }

    /*!
    * @brief Delete elements and remove all references to them in the model
    *
    * @param[in] T Type of the elements
    * @param[in,out] to_erase For each type of element T,
    *        store a vector of the size of model_.nb_elements(T) in which
    *        elements are flagged with NO_ID.
    *        In output it stores the mapping table between old and new indices
    *        for the elements.
    * @todo TEST IT
    */
    void GeoModelEditor::delete_elements(
        std::vector< std::vector< index_t > >& to_erase )
    {
        // Number of elements deleted for each TYPE
        std::vector< index_t > nb_removed( to_erase.size(), 0 ) ;

        /// 1. Get the mapping between old indices of the elements
        ///    and new ones (when elements to remove will actually be removed)
        for( index_t i = 0; i < to_erase.size(); ++i ) {
            for( index_t j = 0; j < to_erase[ i ].size(); ++j ) {
                if( to_erase[ i ][ j ] == NO_ID ) {
                    nb_removed[ i ]++ ;
                } else {
                    to_erase[ i ][ j ] = j - nb_removed[ i ] ;
                }
            }
        }

        /// 2. Effectively delete the elements 
        for( index_t i = 0; i < to_erase.size(); ++i ) {
            for( index_t j = 0; j < to_erase[ i ].size(); ++j ) {
                if( to_erase[ i ][ j ] == NO_ID ) {
                    GME::gme_t cur( static_cast<GME::TYPE>( i ), j ) ;
                    delete model_.element_ptr( cur ) ;
                    // Set the current element to nil
                    model_.modifiable_elements( cur.type )[ cur.index ] = nil ;
                }
            }
            std::vector< GME* >& store = model_.modifiable_elements(
                static_cast<GME::TYPE>( i ) ) ;
            store.erase(
                std::remove( store.begin(), store.end(),
                static_cast<GME*>( nil ) ), store.end() ) ;
        }

        /// 3. Deal with the model vertices
        // We need to do this before changing ids of the BMEs
        // (because we need the old ids to get the new ones)
        // but after deleting the elements otherwise 
        // we are putting back the vertices of the BMMEs to delete
        for( index_t v = 0; v < model_.mesh.vertices.nb(); ++v ) {
            const std::vector< GeoModelMeshVertices::VertexInGME >& cur =
                model_.mesh.vertices.gme_vertices( v ) ;

            for( index_t i = 0; i < cur.size(); ++i ) {
                gme_t id = cur[ i ].gme_id ;
                gme_t new_id( id.type, to_erase[ id.type ][ id.index ] ) ;
                model_.mesh.vertices.set_gme(
                    v, i,
                    GeoModelMeshVertices::VertexInGME( new_id, cur[ i ].v_id ) ) ;
            }
        }
        model_.mesh.erase_invalid_vertices() ;
        model_.mesh.vertices.clear() ;

        /// 4. Update all possible indices in remaining elements
        for( index_t i = 0; i < to_erase.size(); ++i ) {
            GME::TYPE T = static_cast<GME::TYPE>( i ) ;

            // Update all indices stored by the BME of that type 
            ringmesh_debug_assert(
                model_.nb_elements( T ) == to_erase[ i ].size() - nb_removed[ i ] ) ;
            for( index_t j = 0; j < model_.nb_elements( T ); ++j ) {
                GeoModelElement& E = model_.modifiable_element( gme_t( T, j ) ) ;

                // Not the same than j - since we have erased some elements
                index_t old_id = E.gme_id().index ;
                ringmesh_debug_assert( to_erase[ i ][ old_id ] != NO_ID ) ;

                // id_ 
                E.set_id( to_erase[ i ][ old_id ] ) ;
                // boundary_
                if( E.nb_boundaries() > 0 ) {
                    GME::TYPE B = GME::boundary_type( T ) ;
                    ringmesh_debug_assert( B < GME::NO_TYPE ) ;
                    for( index_t k = 0; k < E.nb_boundaries(); ++k ) {
                        E.set_boundary( k,
                                        gme_t( B, to_erase[ B ][ E.boundary_id( k ).index ] ) ) ;
                    }
                }
                // in_boundary
                if( E.nb_in_boundary() > 0 ) {
                    GME::TYPE IB = GME::in_boundary_type( T ) ;
                    ringmesh_debug_assert( IB < GME::NO_TYPE ) ;
                    for( index_t k = 0; k < E.nb_in_boundary(); ++k ) {
                        E.set_in_boundary( k,
                                           gme_t( IB,
                                           to_erase[ IB ][ E.in_boundary_id( k ).index ] ) ) ;
                    }
                }
                // parent_
                if( E.has_parent() ) {
                    GME::TYPE P = GME::parent_type( T ) ;
                    ringmesh_debug_assert( P < GME::NO_TYPE ) ;
                    E.set_parent( gme_t( P, to_erase[ P ][ E.parent_id().index ] ) ) ;
                }
                // children_ 
                if( E.nb_children() > 0 ) {
                    GME::TYPE C = GME::child_type( T ) ;
                    ringmesh_debug_assert( C < GME::NO_TYPE ) ;
                    for( index_t k = 0; k < E.nb_children(); ++k ) {
                        E.set_child( k,
                                     gme_t( C, to_erase[ C ][ E.child_id( k ).index ] ) ) ;
                    }
                }
                // Clean the vectors in the element
                E.erase_invalid_element_references() ;
            }
        }

        // Do not forget the universe
        /// @todo Put the universe in the list of regions - so annoying to think of it each time
        /// @todo BUG ? sides are lost for the universe ? not sure ... [JP]
        {
            Region& U = model_.universe_ ;
            for( index_t i = 0; i < U.nb_boundaries(); ++i ) {
                U.GeoModelElement::set_boundary(
                    i, gme_t( GME::SURFACE, to_erase[ GME::SURFACE ][ U.boundary_id( i ).index ] ) )  ;
            }
            model_.universe_.erase_invalid_element_references() ;
        }
    }
}