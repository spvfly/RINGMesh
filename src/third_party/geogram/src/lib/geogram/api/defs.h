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

#ifndef __GEOGRAM_API_DEFS__
#define __GEOGRAM_API_DEFS__

/**
 * \file geogram/api/defs.h
 * \brief Basic definitions for the Geogram C API
 */

/**
 * \brief Linkage declaration for geogram symbols.
 */

#if defined(_MSC_VER) && defined(GEO_DYNAMIC)
#ifdef geogram_EXPORTS
#define GEOGRAM_API __declspec(dllexport) 
#else
#define GEOGRAM_API __declspec(dllimport) 
#endif
#else
#define GEOGRAM_API
#endif

/**
 * \brief Opaque identifier of a mesh.
 * \details Used by the C API.
 */
typedef int GeoMesh;

/**
 * \brief Represents dimension (e.g. 3 for 3d, 4 for 4d ...).
 * \details Used by the C API.
 */
typedef unsigned char geo_coord_index_t;

/**
 * \brief Represents indices.
 * \details Used by the C API.
 */
typedef unsigned int geo_index_t;

/**
 * \brief Represents possibly negative indices.
 * \details Used by the C API.
 */
typedef int geo_signed_index_t;

/**
 * \brief Represents floating-point coordinates.
 * \details Used by the C API.
 */
typedef double geo_coord_t;

/**
 * \brief Represents truth values.
 * \details Used by the C API.
 */
typedef int geo_boolean;

/**
 * \brief Thruth values (geo_boolean).
 * \details Used by the C API.
 */
enum {
    GEO_FALSE = 0,
    GEO_TRUE = 1
};

#endif

