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

#include <ringmesh/ringmesh_tests_config.h>

#include <geogram/basic/command_line.h>

#include <geogram/mesh/mesh_io.h>

#include <ringmesh/geomodel/geomodel_api.h>
#include <ringmesh/geomodel/geomodel_validity.h>
#include <ringmesh/geomodel/geomodel_builder_from_mesh.h>

#include <ringmesh/io/io.h>

/*! 
 * Test the creation of a GeoModel from a conformal surface mesh 
 * @todo Test on other datasets: nested spheres.
 * @author Jeanne Pellerin
 */

int main()
{
    using namespace RINGMesh ;

    try {
        default_configure() ;

        std::string file_name = ringmesh_test_data_path ;
        file_name += "modelA6.mesh" ;

        // Set an output log file
        std::string log_file( ringmesh_test_output_path ) ;
        log_file += "log.txt" ;
        GEO::FileLogger* file_logger = new GEO::FileLogger( log_file ) ;
        Logger::instance()->register_client( file_logger ) ;

        Logger::out( "TEST" ) << "Test GeoModel building from Surface"
            << std::endl ;

        GEO::Mesh in ;
        GEO::mesh_load( file_name, in ) ;
        GeoModel model ;

        GeoModelBuilderSurfaceMesh builder( model, in ) ;
        builder.build_polygonal_surfaces_from_connected_components() ;
        builder.from_surfaces.build() ;
        print_geomodel( model ) ;

        //Checking the validity of loaded model
#ifdef RINGMESH_DEBUG
        GEO::CmdLine::set_arg( "in:intersection_check", true ) ;
#else
        GEO::CmdLine::set_arg( "in:intersection_check", false ) ;
#endif

        if( !is_geomodel_valid( model ) ) {
            throw RINGMeshException( "RINGMesh Test",
                "Failed when loading model " + model.name()
                    + ": the loaded model is not valid." ) ;
        }

        // Save and reload the model
        std::string output_file( ringmesh_test_output_path ) ;
        output_file += "saved_modelA6.mesh" ;
        geomodel_save( model, output_file ) ;

        // Compute mesh with duplicated points to compares number
        // of mesh elements and mesh entities
        GEO::Mesh surface_meshes ;
        for( index_t s = 0; s < model.nb_surfaces(); s++ ) {
            const Surface& surface = model.surface( s ) ;
            index_t vertex_it = surface_meshes.vertices.create_vertices(
                surface.nb_vertices() ) ;
            for( index_t v = 0; v < surface.nb_vertices(); v++ ) {
                surface_meshes.vertices.point( vertex_it + v ) = surface.vertex(
                    v ) ;
            }
            index_t facet_it = surface_meshes.facets.create_triangles(
                surface.nb_mesh_elements() ) ;
            for( index_t f = 0; f < surface.nb_mesh_elements(); f++ ) {
                for( index_t v = 0; v < surface.nb_mesh_element_vertices( f );
                    v++ ) {
                    surface_meshes.facets.set_vertex( facet_it + f, v,
                        vertex_it + surface.mesh_element_vertex_index( f, v ) ) ;
                }
            }
        }
        surface_meshes.facets.connect() ;

        // Save computed mesh
        std::string output_file2( ringmesh_test_output_path ) ;
        output_file2 += "saved_modelA6_dupl_points.mesh" ;
        GEO::mesh_save( surface_meshes, output_file2 ) ;

        GeoModel reloaded_model ;

        GeoModelBuilderSurfaceMesh builder2( reloaded_model, surface_meshes ) ;
        builder2.build_polygonal_surfaces_from_connected_components() ;
        builder2.from_surfaces.build() ;
        print_geomodel( reloaded_model ) ;

        // Checking if building has been successfully done
        if( !is_geomodel_valid( reloaded_model ) ) {
            throw RINGMeshException( "RINGMesh Test",
                "Failed when reloading model " + reloaded_model.name()
                    + ": the reloaded model is not valid." ) ;
        }

        // Checking number of mesh elements
        if( surface_meshes.vertices.nb() != in.vertices.nb() ) {
            throw RINGMeshException( "RINGMesh Test",
                "Error when building model: not same number of vertices "
                    "than input mesh." ) ;
        }
        if( surface_meshes.facets.nb() != in.facets.nb() ) {
            throw RINGMeshException( "RINGMesh Test",
                "Error when building model: not same number of facets "
                    "than input mesh." ) ;
        }
        if( surface_meshes.cells.nb() != in.cells.nb() ) {
            throw RINGMeshException( "RINGMesh Test",
                "Error when building model: not same number of cells "
                    "than input mesh." ) ;
        }

        // Checking number of GeoModelMeshEntities
        if( reloaded_model.nb_corners() != model.nb_corners() ) {
            throw RINGMeshException( "RINGMesh Test",
                "Error when reload model: not same number of corners "
                    "between saved model and reload model." ) ;
        }
        if( reloaded_model.nb_lines() != model.nb_lines() ) {
            throw RINGMeshException( "RINGMesh Test",
                "Error when reload model: not same number of lines "
                    "between saved model and reload model." ) ;
        }
        if( reloaded_model.nb_surfaces() != model.nb_surfaces() ) {
            throw RINGMeshException( "RINGMesh Test",
                "Error when reload model: not same number of surfaces "
                    "between saved model and reload model." ) ;
        }
        if( reloaded_model.nb_regions() != model.nb_regions() ) {
            throw RINGMeshException( "RINGMesh Test",
                "Error when reload model: not same number of regions "
                    "between saved model and reload model." ) ;
        }

    } catch( const RINGMeshException& e ) {
        Logger::err( e.category() ) << e.what() << std::endl ;
        return 1 ;
    } catch( const std::exception& e ) {
        Logger::err( "Exception" ) << e.what() << std::endl ;
        return 1 ;
    }
    Logger::out( "TEST" ) << "SUCCESS" << std::endl ;
    return 0 ;

}
