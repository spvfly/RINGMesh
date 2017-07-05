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

#include <ringmesh/basic/common.h>

#include <geogram/basic/command_line.h>
#include <geogram/basic/stopwatch.h>

#include <ringmesh/basic/command_line.h>
#include <ringmesh/geomodel/geomodel.h>
#include <ringmesh/geomodel/geomodel_mesh_entity.h>
#include <ringmesh/io/io.h>
#include <ringmesh/geomodel/mesh_quality.h>

/*!
 * @author Benjamin Chauvin
 */

namespace {
    using namespace RINGMesh;

    void hello()
    {
        print_header_information();
        Logger::div( "RINGMesh-Mesh-Quality" );
        Logger::out( "", "Welcome to RINGMesh-Mesh-Quality !" );
    }

    void import_arg_group_quality()
    {
        GEO::CmdLine::declare_arg_group( "quality", "Mesh quality" );
        GEO::CmdLine::declare_arg( "quality:mode", 0, "Mesh quality mode" );
    }

    void import_arg_groups()
    {
        CmdLine::import_arg_group( "in" );
        import_arg_group_quality();
        CmdLine::import_arg_group( "out" );
    }

    void check_geomodel_is_3d_meshed_by_simplexes( const GeoModel& geomodel )
    {
        if( geomodel.nb_regions() == 0 ) {
            throw RINGMeshException( "I/O", "Input geomodel has no region." );
        }

        for( index_t reg_itr = 0; reg_itr < geomodel.nb_regions(); ++reg_itr ) {
            if( !geomodel.region( reg_itr ).is_meshed() ) {
                throw RINGMeshException( "I/O",
                    "Region " + GEO::String::to_string( reg_itr )
                        + " is not meshed." );
            }
            if( !geomodel.region( reg_itr ).is_simplicial() ) {
                throw RINGMeshException( "I/O",
                    "Region " + GEO::String::to_string( reg_itr )
                        + " is not simplicial." );
            }
        }
    }

    void run()
    {
        GEO::Stopwatch total( "Total time" );

        std::string geomodel_in_name = GEO::CmdLine::get_arg( "in:geomodel" );
        if( geomodel_in_name.empty() ) {
            throw RINGMeshException( "I/O",
                "Give at least a filename in in:geomodel" );
        }
        GeoModel geomodel;
        geomodel_load( geomodel, geomodel_in_name );
        check_geomodel_is_3d_meshed_by_simplexes( geomodel );

        index_t quality_mode = GEO::CmdLine::get_arg_uint( "quality:mode" );
        compute_prop_tet_mesh_quality(
            static_cast< MeshQualityMode >( quality_mode ), geomodel );

        std::string geomodel_out_name = GEO::CmdLine::get_arg( "out:geomodel" );
        if( geomodel_out_name.empty() ) {
            throw RINGMeshException( "I/O",
                "Give at least a filename in out:geomodel" );
        }
        geomodel_save( geomodel, geomodel_out_name );
    }
}

int main( int argc, char** argv )
{
    using namespace RINGMesh;

    try {

        default_configure();
        hello();
        import_arg_groups();
        if( argc == 1 ) {
            GEO::CmdLine::show_usage();
            return 0;
        }

        std::vector< std::string > filenames;
        if( !GEO::CmdLine::parse( argc, argv, filenames ) ) {
            return 1;
        }

        run();

    } catch( const RINGMeshException& e ) {
        Logger::err( e.category(), e.what() );
        return 1;
    } catch( const std::exception& e ) {
        Logger::err( "Exception", e.what() );
        return 1;
    }
    return 0;
}