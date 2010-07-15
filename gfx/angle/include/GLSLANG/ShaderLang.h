//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef _COMPILER_INTERFACE_INCLUDED_
#define _COMPILER_INTERFACE_INCLUDED_

#include "ResourceLimits.h"

#ifdef _WIN32
#define C_DECL __cdecl
#else
#define __fastcall
#define C_DECL
#endif

//
// This is the platform independent interface between an OGL driver
// and the shading language compiler/linker.
//

#ifdef __cplusplus
	extern "C" {
#endif
//
// Driver must call this first, once, before doing any other
// compiler/linker operations.
//
int ShInitialize();
//
// Driver should call this at shutdown.
//
int __fastcall ShFinalize();
//
// Types of languages the compiler can consume.
//
typedef enum {
	EShLangVertex,
	EShLangFragment,
	EShLangCount,
} EShLanguage;

//
// Types of output the linker will create.
//
typedef enum {
	EShExVertexFragment,
	EShExFragment
} EShExecutable;

//
// Optimization level for the compiler.
//
typedef enum {
	EShOptNoGeneration,
	EShOptNone,
	EShOptSimple,       // Optimizations that can be done quickly
	EShOptFull,         // Optimizations that will take more time
} EShOptimizationLevel;

//
// Build a table for bindings.  This can be used for locating
// attributes, uniforms, globals, etc., as needed.
//
typedef struct {
	const char* name;
	int binding;
} ShBinding;

typedef struct {
	int numBindings;
	ShBinding* bindings;  // array of bindings
} ShBindingTable;

//
// ShHandle held by but opaque to the driver.  It is allocated,
// managed, and de-allocated by the compiler/linker. It's contents 
// are defined by and used by the compiler and linker.  For example,
// symbol table information and object code passed from the compiler 
// to the linker can be stored where ShHandle points.
//
// If handle creation fails, 0 will be returned.
//
typedef void* ShHandle;

//
// Driver calls these to create and destroy compiler/linker
// objects.
//
ShHandle ShConstructCompiler(const EShLanguage, int debugOptions);  // one per shader
ShHandle ShConstructLinker(const EShExecutable, int debugOptions);  // one per shader pair
ShHandle ShConstructUniformMap();                 // one per uniform namespace (currently entire program object)
void ShDestruct(ShHandle);

//
// The return value of ShCompile is boolean, indicating
// success or failure.
//
// The info-log should be written by ShCompile into 
// ShHandle, so it can answer future queries.
//
int ShCompile(
	const ShHandle,
	const char* const shaderStrings[],
	const int numStrings,
	const EShOptimizationLevel,
	const TBuiltInResource *resources,
	int debugOptions
	);


//
// Similar to ShCompile, but accepts an opaque handle to an
// intermediate language structure.
//
int ShCompileIntermediate(
	ShHandle compiler,
	ShHandle intermediate,
	const EShOptimizationLevel,
	int debuggable           // boolean
	);

int ShLink(
	const ShHandle,               // linker object
	const ShHandle h[],           // compiler objects to link together
	const int numHandles,
	ShHandle uniformMap,          // updated with new uniforms
	short int** uniformsAccessed,  // returned with indexes of uniforms accessed
	int* numUniformsAccessed); 	

int ShLinkExt(
	const ShHandle,               // linker object
	const ShHandle h[],           // compiler objects to link together
	const int numHandles);

//
// ShSetEncrpytionMethod is a place-holder for specifying
// how source code is encrypted.
//
void ShSetEncryptionMethod(ShHandle);

//
// All the following return 0 if the information is not
// available in the object passed down, or the object is bad.
//
const char* ShGetInfoLog(const ShHandle);
const char* ShGetObjectCode(const ShHandle);
const void* ShGetExecutable(const ShHandle);
int ShSetVirtualAttributeBindings(const ShHandle, const ShBindingTable*);   // to detect user aliasing
int ShSetFixedAttributeBindings(const ShHandle, const ShBindingTable*);     // to force any physical mappings
int ShGetPhysicalAttributeBindings(const ShHandle, const ShBindingTable**); // for all attributes
//
// Tell the linker to never assign a vertex attribute to this list of physical attributes
//
int ShExcludeAttributes(const ShHandle, int *attributes, int count);

//
// Returns the location ID of the named uniform.
// Returns -1 if error.
//
int ShGetUniformLocation(const ShHandle uniformMap, const char* name);

enum TDebugOptions {
	EDebugOpNone               = 0x000,
	EDebugOpIntermediate       = 0x001,
	EDebugOpAssembly           = 0x002,
	EDebugOpObjectCode         = 0x004,
	EDebugOpLinkMaps           = 0x008
};
#ifdef __cplusplus
	}
#endif

#endif // _COMPILER_INTERFACE_INCLUDED_
