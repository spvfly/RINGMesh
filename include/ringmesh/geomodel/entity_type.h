/*
 * Copyright (c) 2012-2017, Association Scientifique pour la Geologie et ses Applications (ASGA)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of ASGA nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ASGA BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *     http://www.ring-team.org
 *
 *     RING Project
 *     Ecole Nationale Superieure de Geologie - GeoRessources
 *     2 Rue du Doyen Marcel Roubault - TSA 70605
 *     54518 VANDOEUVRE-LES-NANCY
 *     FRANCE
 */

#pragma once

#include <ringmesh/basic/common.h>

#include <vector>

#include <ringmesh/basic/algorithm.h>

namespace RINGMesh {
    class GeoModelMeshEntityAccess;
    class GeoModelGeologicalEntityAccess;
}

namespace RINGMesh {

    /*
     * @brief Abstract class defining a Geomodel Entity Type
     * This class encapsulate a string which contains the name of the entity type
     * It contains useful operator to compare and display the type
     * It is possible to do cast of an EntityType -> string
     */
    class RINGMESH_API EntityType {
    public:
        bool operator==( const EntityType& type2 ) const
        {
            return type_ == type2.type_;
        }
        bool operator!=( const EntityType& type2 ) const
        {
            return type_ != type2.type_;
        }
        friend std::ostream& operator<<( std::ostream& os, const EntityType& in )
        {
            os << in.type_;
            return os;
        }
        bool operator<( const EntityType& rhs ) const
        {
            return type_ < rhs.type_;
        }

        operator std::string() const
        {
            return type_;
        }
        explicit operator std::string*() const
        {
            return nil;
        }

    private:
        std::string type_;
    protected:
        EntityType( std::string type )
            : type_( type )
        {
        }
        EntityType()
            : type_( default_entity_type_string() )
        {
        }

        void set_type( std::string type )
        {
            type_ = type;
        }
    private:
        const std::string default_entity_type_string()
        {
            return "No_entity_type";
        }
    };

    /*!
     * @brief The MeshEntityType described the type of the meshed entities
     * There are 4 MeshEntityTypes corresponding to the 4 GeoModelMeshEntities:
     * Corner,
     * Line,
     * Surface,
     * Region
     */
    class RINGMESH_API MeshEntityType: public EntityType {
    public:
        MeshEntityType( std::string type )
            : EntityType( type )
        {
        }
        MeshEntityType() = default;
    };

    /*!
     * @brief The GeologicalEntityType described the type of the Geological entities
     * User can defined there own GeologicalEntityType even if there are some already
     * defined (see geomodel_geological_entities.h
     * Contact,
     * Interface,
     * Layer
     */
    class RINGMESH_API GeologicalEntityType: public EntityType {
    public:
        GeologicalEntityType( std::string type )
            : EntityType( type )
        {
        }
        GeologicalEntityType() = default;
    };

    /*!
     * @brief this is the MeshEntityType defined by default.
     * It is mainly used to test the validity of a created MeshEntityType
     */
    class RINGMESH_API ForbiddenMeshEntityType: public MeshEntityType {
    public:
        static ForbiddenMeshEntityType& type_name_static()
        {
            static ForbiddenMeshEntityType entity_type;
            return entity_type;
        }
    private:
        ForbiddenMeshEntityType() = default;
    };

    /*!
     * @brief this is the GeologicalEntityType defined by default.
     * It is mainly used to test the validity of a created GeologicalEntityType
     */
    class RINGMESH_API ForbiddenGeologicalEntityType: public GeologicalEntityType {
    public:
        static ForbiddenGeologicalEntityType& type_name_static()
        {
            static ForbiddenGeologicalEntityType entity_type;
            return entity_type;
        }
    private:
        ForbiddenGeologicalEntityType() = default;
    };

    /*!
     * @brief This entity type stands only for the special case of the
     * Universe which is not a GeomodelGeologicalEntity nor a GeomodelMeshEntity
     */
    class RINGMESH_API UniverseType: public EntityType {
    public:
        UniverseType()
            : EntityType( "Universe" )
        {
        }
    };

    /*!
     * @brief Unique identification of a GeoModelEntity in a GeoModel
     * It contains the EntityType and the index of the entity.
     * It is widely used in the code to easily access/modify/set a GeoModelEntity
     * @todo Should we change this name? Looks like index_t but does not enforce
     *       the programming guidelines [JP]
     */
    template< class Entity_type_template >
    struct gme_id {
        friend GeoModelMeshEntityAccess;
        friend GeoModelGeologicalEntityAccess;
    public:
        index_t index() const
        {
            return index_;
        }

        Entity_type_template type() const
        {
            return type_;
        }

        bool operator!=( const gme_id& rhs ) const
        {
            return type_ != rhs.type_ || index_ != rhs.index_;
        }
        bool operator==( const gme_id& rhs ) const
        {
            return type_ == rhs.type_ && index_ == rhs.index_;
        }

        friend std::ostream& operator<<( std::ostream& os, const gme_id& in )
        {
            os << in.type_ << " " << in.index_;
            return os;
        }
        bool operator<( const gme_id& rhs ) const
        {
            if( type_ != rhs.type_ ) {
                /// @warning Is this now enough for EntityType = std::string?
                /// Did any code relied on that sorting? Maybe mine ... [JP]
                return type_ < rhs.type_;
            } else {
                if( index_ == NO_ID ) return true;
                if( rhs.index_ == NO_ID ) return false;
                return index_ < rhs.index_;
            }
        }

        bool is_defined() const
        {
            return type_ != ForbiddenMeshEntityType::type_name_static()
                && index_ != NO_ID;
        }
        gme_id()
            : type_( ForbiddenMeshEntityType::type_name_static() ), index_( NO_ID )
        {
        }
        gme_id( const Entity_type_template entity_type, index_t index )
            : type_( entity_type ), index_( index )
        {
        }
        gme_id( const gme_id& from )
            : gme_id( from.type_, from.index_ )
        {
        }

    protected:
        Entity_type_template type_;
        /*!
         * Index of the GeoModelEntity in the GeoModel
         */
        index_t index_;
    };

    /*!
     * @brief This template is a specialization of a gme_id to the GeoModelGeologicalEntity
     */
    using gmge_id = gme_id< GeologicalEntityType >;
    /*!
     * @brief This template is a specialization of a gme_id to the GeoModelMeshEntity
     */
    using gmme_id = gme_id< MeshEntityType >;
}