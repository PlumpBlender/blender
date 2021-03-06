/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file RAS_Deformer.h
 *  \ingroup bgerast
 */

#ifndef __RAS_DEFORMER_H__
#define __RAS_DEFORMER_H__

#ifdef _MSC_VER
#  pragma warning (disable:4786)  /* get rid of stupid stl-visual compiler debug warning */
#endif

#include <stdlib.h>
#include <map>
#include "MT_Vector3.h"

#include "RAS_BoundingBox.h"
#include "RAS_IDisplayArray.h" // For RAS_IDisplayArrayList.
#include "RAS_DisplayArrayBucket.h" // For RAS_DisplayArrayBucketList.

struct DerivedMesh;
class RAS_MeshObject;
class RAS_IPolyMaterial;
class RAS_MeshMaterial;
class SCA_IObject;

class RAS_Deformer
{
public:
	RAS_Deformer(RAS_MeshObject *mesh)
		:m_mesh(mesh),
		m_bDynamic(false),
		m_boundingBox(nullptr)
	{
	}

	virtual ~RAS_Deformer();

	virtual void Relink(std::map<SCA_IObject *, SCA_IObject *>& map) = 0;
	virtual void Apply(RAS_MeshMaterial *meshmat, RAS_IDisplayArray *array) = 0;
	virtual bool Update(void)=0;
	virtual void UpdateBuckets(void)=0;
	virtual RAS_Deformer *GetReplica()=0;
	virtual void ProcessReplica()
	{
		m_displayArrayList.clear();
		m_displayArrayBucketList.clear();
		m_boundingBox = m_boundingBox->GetReplica();
	}
	virtual bool SkipVertexTransform()
	{
		return false;
	}
	virtual bool UseVertexArray()
	{
		return true;
	}
	// true when deformer produces varying vertex (shape or armature)
	bool IsDynamic()
	{
		return m_bDynamic;
	}
	virtual struct DerivedMesh* GetFinalMesh()
	{
		return nullptr;
	}
	virtual struct DerivedMesh* GetPhysicsMesh()
	{
		return nullptr;
	}
	virtual class RAS_MeshObject* GetRasMesh()
	{
		return nullptr;
	}
	virtual const std::vector<std::array<float, 3> >& GetTransVerts() const
	{
		static const std::vector<std::array<float, 3> > emptyList;
		return emptyList;
	}

	RAS_BoundingBox *GetBoundingBox() const
	{
		return m_boundingBox;
	}

	void AddDisplayArray(RAS_IDisplayArray *array, RAS_DisplayArrayBucket *arrayBucket);

protected:
	RAS_MeshObject *m_mesh;
	bool m_bDynamic;

	RAS_IDisplayArrayList m_displayArrayList;
	RAS_DisplayArrayBucketList m_displayArrayBucketList;

	/// Deformer bounding box.
	RAS_BoundingBox *m_boundingBox;
};

#endif

