#ifndef _MDynamicsUtil
#define _MDynamicsUtil
//-
// ==========================================================================
// Copyright (C) 1995 - 2006 Autodesk, Inc., and/or its licensors.  All
// rights reserved.
//
// The coded instructions, statements, computer programs, and/or related
// material (collectively the "Data") in these files contain unpublished
// information proprietary to Autodesk, Inc. ("Autodesk") and/or its
// licensors,  which is protected by U.S. and Canadian federal copyright law
// and by international treaties.
//
// The Data may not be disclosed or distributed to third parties or be
// copied or duplicated, in whole or in part, without the prior written
// consent of Autodesk.
//
// The copyright notices in the Software and this entire statement,
// including the above license grant, this restriction and the following
// disclaimer, must be included in all copies of the Software, in whole
// or in part, and all derivative works of the Software, unless such copies
// or derivative works are solely in the form of machine-executable object
// code generated by a source language processor.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
// AUTODESK DOES NOT MAKE AND HEREBY DISCLAIMS ANY EXPRESS OR IMPLIED
// WARRANTIES INCLUDING, BUT NOT LIMITED TO, THE WARRANTIES OF
// NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE,
// OR ARISING FROM A COURSE OF DEALING, USAGE, OR TRADE PRACTICE. IN NO
// EVENT WILL AUTODESK AND/OR ITS LICENSORS BE LIABLE FOR ANY LOST
// REVENUES, DATA, OR PROFITS, OR SPECIAL, DIRECT, INDIRECT, OR
// CONSEQUENTIAL DAMAGES, EVEN IF AUTODESK AND/OR ITS LICENSORS HAS
// BEEN ADVISED OF THE POSSIBILITY OR PROBABILITY OF SUCH DAMAGES.
// ==========================================================================
//+
//
// CLASS:    MDynamicsUtil
//
// ****************************************************************************

#if defined __cplusplus

// ****************************************************************************
// INCLUDED HEADER FILES

class MObject;
class MStatus;
class MDoubleArray;
class MVectorArray;
class MString;

// ****************************************************************************
// CLASS DECLARATION (MDynamicsUtil)

//! \ingroup OpenMayaFX
//! \brief Utility class for Maya dynamics 
/*!
	MDynamicsUtil provides utility methods related to Maya dynamics (eg.
	particles and fluids).
*/
class OPENMAYAFX_EXPORT MDynamicsUtil
{
public:

	static bool			hasValidDynamics2dTexture( const MObject& node,
								   const MObject& texAttr,
								   MStatus* status = NULL );
	static MStatus		evalDynamics2dTexture( const MObject& node,
									const MObject& texAttr,
									MDoubleArray& uCoords,
									MDoubleArray& vCoords,
									MVectorArray* resultColors,
									MDoubleArray* resultAlphas );
	static bool			inRunup();
	static bool			runupIfRequired();
	static bool			addNodeTypeToRunup(const MString& nodeTypeName);
	static bool			removeNodeTypeFromRunup(const MString& nodeTypeName);

protected:
// No protected members
private:
// No private members
};

#endif /* __cplusplus */
#endif /* _MDynamicsUtil */