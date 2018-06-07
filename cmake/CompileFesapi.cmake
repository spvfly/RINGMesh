# Copyright (c) 2018, Association Scientifique pour la Geologie et ses
# Applications (ASGA). All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of ASGA nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL ASGA BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#     http://www.ring-team.org
#
#     RING Project
#     Ecole Nationale Superieure de Geologie - GeoRessources
#     2 Rue du Doyen Marcel Roubault - TSA 70605
#     54518 VANDOEUVRE-LES-NANCY
#     FRANCE

set(FESAPI_PATH ${PROJECT_SOURCE_DIR}/third_party/fesapi)
set(FESAPI_PATH_BIN ${PROJECT_BINARY_DIR}/third_party/fesapi)
set(FESAPI_INSTALL_PREFIX ${FESAPI_PATH_BIN}/install
    CACHE INTERNAL "Fesapi install directory")

ExternalProject_Add(fesapi_ext
    PREFIX ${FESAPI_PATH_BIN}
    SOURCE_DIR ${FESAPI_PATH}
    CMAKE_GENERATOR ${CMAKE_GENERATOR}
    CMAKE_ARGS
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_MESSAGE=LAZY
    CMAKE_CACHE_ARGS
        -DBUILD_TESTING:BOOL=OFF
        -DCMAKE_INSTALL_PREFIX:PATH=${FESAPI_INSTALL_PREFIX}
        -DCMAKE_INSTALL_LIBDIR:PATH=lib
        -DCMAKE_MACOSX_RPATH:BOOL=ON
#        -DZLIB_ROOT:PATH=${ZLIB_ROOT}
        -DHDF5_C_RELEASE_PATH:PATH=${HDF5_C_RELEASE_PATH}
        -DMINIZIP_INSTALL_PREFIX:PATH=${MINIZIP_INSTALL_PREFIX}
        -DCMAKE_INSTALL_RPATH:PATH=${FESAPI_INSTALL_PREFIX}/lib
    BINARY_DIR ${FESAPI_PATH_BIN}
    BUILD_ALWAYS 1
    INSTALL_DIR ${FESAPI_INSTALL_PREFIX}
#    DEPENDS zlib_ext minizip_ext
    DEPENDS minizip_ext
)