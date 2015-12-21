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

/** \file gameengine/Rasterizer/RAS_MeshObject.cpp
 *  \ingroup bgerast
 */

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"

#include "CTR_HashedPtr.h"

#include "RAS_MeshObject.h"
#include "RAS_Polygon.h"
#include "RAS_IPolygonMaterial.h"
#include "RAS_Deformer.h"
#include "MT_Point3.h"

#include <algorithm>

// polygon sorting

struct RAS_MeshObject::polygonSlot
{
	float m_z;
	int m_index[4];

	polygonSlot()
	{
	}

	/* pnorm is the normal from the plane equation that the distance from is
	 * used to sort again. */
	void get(const RAS_TexVert *vertexarray, const unsigned short *indexarray,
	         int offset, int nvert, const MT_Vector3& pnorm)
	{
		MT_Vector3 center(0.0f, 0.0f, 0.0f);
		int i;

		for (i = 0; i < nvert; i++) {
			m_index[i] = indexarray[offset + i];
			center += vertexarray[m_index[i]].getXYZ();
		}

		/* note we don't divide center by the number of vertices, since all
		* polygons have the same number of vertices, and that we leave out
		* the 4-th component of the plane equation since it is constant. */
		m_z = MT_dot(pnorm, center);
	}

	void set(unsigned short *indexarray, int offset, int nvert)
	{
		int i;

		for (i = 0; i < nvert; i++)
			indexarray[offset + i] = m_index[i];
	}
};

struct RAS_MeshObject::backtofront
{
	bool operator()(const polygonSlot &a, const polygonSlot &b) const
	{
		return a.m_z < b.m_z;
	}
};

struct RAS_MeshObject::fronttoback
{
	bool operator()(const polygonSlot &a, const polygonSlot &b) const
	{
		return a.m_z > b.m_z;
	}
};

// mesh object

STR_String RAS_MeshObject::s_emptyname = "";

RAS_MeshObject::RAS_MeshObject(Mesh *mesh)
	:m_bMeshModified(true),
	m_aabbModified(true),
	m_mesh(mesh)
{
	if (m_mesh && m_mesh->key) {
		KeyBlock *kb;
		int count = 0;
		// initialize weight cache for shape objects
		// count how many keys in this mesh
		for (kb = (KeyBlock *)m_mesh->key->block.first; kb; kb = (KeyBlock *)kb->next)
			count++;
		m_cacheWeightIndex.resize(count, -1);
	}
}

RAS_MeshObject::~RAS_MeshObject()
{
	vector<RAS_Polygon *>::iterator it;

	for (it = m_Polygons.begin(); it != m_Polygons.end(); it++)
		delete (*it);

	m_sharedvertex_map.clear();
	m_Polygons.clear();
	m_materials.clear();
}

bool RAS_MeshObject::MeshModified()
{
	return m_bMeshModified;
}

void RAS_MeshObject::UpdateAabb()
{
	bool first = true;
	unsigned int nmat = NumMaterials();
	for (unsigned int imat = 0; imat < nmat; ++imat) {
		RAS_MeshMaterial *mmat = GetMeshMaterial(imat);

		RAS_MeshSlot *slot = mmat->m_baseslot;
		if (!slot)
			continue;

		RAS_DisplayArray *array = slot->GetDisplayArray();
		// for each vertex
		for (unsigned int i = 0; i < array->m_vertex.size(); ++i) {
			RAS_TexVert& v = array->m_vertex[i];
			MT_Vector3 vertpos = v.xyz();

			// For the first vertex of the mesh, only initialize AABB.
			if (first) {
				m_aabbMin = m_aabbMax = vertpos;
				first = false;
			}
			else {
				m_aabbMin.x() = std::min(m_aabbMin.x(), vertpos.x());
				m_aabbMin.y() = std::min(m_aabbMin.y(), vertpos.y());
				m_aabbMin.z() = std::min(m_aabbMin.z(), vertpos.z());
				m_aabbMax.x() = std::max(m_aabbMax.x(), vertpos.x());
				m_aabbMax.y() = std::max(m_aabbMax.y(), vertpos.y());
				m_aabbMax.z() = std::max(m_aabbMax.z(), vertpos.z());
			}
		}
	}

	m_aabbModified = false;
}

void RAS_MeshObject::GetAabb(MT_Point3 &aabbMin, MT_Point3 &aabbMax)
{
	if (m_aabbModified)
		UpdateAabb();

	aabbMin = m_aabbMin;
	aabbMax = m_aabbMax;
}

int RAS_MeshObject::NumMaterials()
{
	return m_materials.size();
}

const STR_String& RAS_MeshObject::GetMaterialName(unsigned int matid)
{
	RAS_MeshMaterial *mmat = GetMeshMaterial(matid);

	if (mmat)
		return mmat->m_bucket->GetPolyMaterial()->GetMaterialName();

	return s_emptyname;
}

RAS_MeshMaterial *RAS_MeshObject::GetMeshMaterial(unsigned int matid)
{
	if ((m_materials.empty() == false) && (matid < m_materials.size())) {
		list<RAS_MeshMaterial>::iterator it = m_materials.begin();
		while (matid--) {
			++it;
		}
		return &*it;
	}

	return NULL;
}

int RAS_MeshObject::NumPolygons()
{
	return m_Polygons.size();
}

RAS_Polygon *RAS_MeshObject::GetPolygon(int num) const
{
	return m_Polygons[num];
}

list<RAS_MeshMaterial>::iterator RAS_MeshObject::GetFirstMaterial()
{
	return m_materials.begin();
}

list<RAS_MeshMaterial>::iterator RAS_MeshObject::GetLastMaterial()
{
	return m_materials.end();
}

void RAS_MeshObject::SetName(const char *name)
{
	m_name = name;
}

STR_String& RAS_MeshObject::GetName()
{
	return m_name;
}

const STR_String& RAS_MeshObject::GetTextureName(unsigned int matid)
{
	RAS_MeshMaterial *mmat = GetMeshMaterial(matid);

	if (mmat)
		return mmat->m_bucket->GetPolyMaterial()->GetTextureName();

	return s_emptyname;
}

RAS_MeshMaterial *RAS_MeshObject::GetMeshMaterial(RAS_IPolyMaterial *mat)
{
	// find a mesh material
	for (list<RAS_MeshMaterial>::iterator mit = m_materials.begin(); mit != m_materials.end(); mit++) {
		if (mit->m_bucket->GetPolyMaterial() == mat)
			return &*mit;
	}

	return NULL;
}

int RAS_MeshObject::GetBlenderMaterialId(RAS_IPolyMaterial *mat)
{
	// find a mesh material
	for (list<RAS_MeshMaterial>::iterator mit = m_materials.begin(); mit != m_materials.end(); ++mit) {
		if (mit->m_bucket->GetPolyMaterial() == mat)
			return mit->m_index;
	}

	return -1;
}

void RAS_MeshObject::AddMaterial(RAS_MaterialBucket *bucket, unsigned int index)
{
	RAS_MeshMaterial *mmat = GetMeshMaterial(bucket->GetPolyMaterial());

	// none found, create a new one
	if (!mmat) {
		RAS_MeshMaterial meshmat;
		meshmat.m_bucket = bucket;
		meshmat.m_baseslot = meshmat.m_bucket->AddMesh();
		meshmat.m_baseslot->m_mesh = this;
		meshmat.m_index = index;
		m_materials.push_back(meshmat);
	}
}

RAS_Polygon *RAS_MeshObject::AddPolygon(RAS_MaterialBucket *bucket, int numverts)
{
	// find a mesh material
	RAS_MeshMaterial *mmat = GetMeshMaterial(bucket->GetPolyMaterial());
	// add it to the bucket, this also adds new display arrays
	RAS_MeshSlot *slot = mmat->m_baseslot;

	// create a new polygon
	RAS_DisplayArray *darray = slot->GetDisplayArray();
	RAS_Polygon *poly = new RAS_Polygon(bucket, darray, numverts);
	m_Polygons.push_back(poly);

	return poly;
}

void RAS_MeshObject::SetVertexColor(RAS_IPolyMaterial *mat, MT_Vector4 rgba)
{
	RAS_MeshMaterial *mmat = GetMeshMaterial(mat);
	RAS_MeshSlot *slot = mmat->m_baseslot;
	RAS_DisplayArray *array = slot->GetDisplayArray();

	for (unsigned int i = 0; i < array->m_vertex.size(); i++) {
		array->m_vertex[i].SetRGBA(rgba);
	}
}

void RAS_MeshObject::AddVertex(RAS_Polygon *poly, int i,
                               const MT_Point3& xyz,
                               const MT_Point2 uvs[RAS_TexVert::MAX_UNIT],
                               const MT_Vector4& tangent,
                               const unsigned int rgba,
                               const MT_Vector3& normal,
                               bool flat,
                               int origindex)
{
	RAS_TexVert texvert(xyz, uvs, tangent, rgba, normal, flat, origindex);

	RAS_MeshMaterial *mmat = GetMeshMaterial(poly->GetMaterial()->GetPolyMaterial());
	RAS_MeshSlot *slot = mmat->m_baseslot;
	RAS_DisplayArray *darray = slot->GetDisplayArray();

	{	/* Shared Vertex! */
		/* find vertices shared between faces, with the restriction
		 * that they exist in the same display array, and have the
		 * same uv coordinate etc */
		vector<SharedVertex>& sharedmap = m_sharedvertex_map[origindex];
		vector<SharedVertex>::iterator it;

		for (it = sharedmap.begin(); it != sharedmap.end(); it++) {
			if (it->m_darray != darray)
				continue;
			if (!it->m_darray->m_vertex[it->m_offset].closeTo(&texvert))
				continue;

			// found one, add it and we're done
			if (poly->IsVisible())
				slot->AddPolygonVertex(it->m_offset);
			poly->SetVertexOffset(i, it->m_offset);
			return;
		}
	}

	// no shared vertex found, add a new one
	int offset = slot->AddVertex(texvert);
	if (poly->IsVisible())
		slot->AddPolygonVertex(offset);
	poly->SetVertexOffset(i, offset);

	{ 	// Shared Vertex!
		SharedVertex shared;
		shared.m_darray = darray;
		shared.m_offset = offset;
		m_sharedvertex_map[origindex].push_back(shared);
	}
}

int RAS_MeshObject::NumVertices(RAS_IPolyMaterial *mat)
{
	RAS_MeshMaterial *mmat = GetMeshMaterial(mat);
	RAS_MeshSlot *slot = mmat->m_baseslot;
	return slot->GetDisplayArray()->m_vertex.size();
}

RAS_TexVert *RAS_MeshObject::GetVertex(unsigned int matid, unsigned int index)
{
	RAS_MeshMaterial *mmat = GetMeshMaterial(matid);

	if (!mmat)
		return NULL;

	RAS_MeshSlot *slot = mmat->m_baseslot;
	RAS_DisplayArray *array = slot->GetDisplayArray();

	if (index < array->m_vertex.size()) {
		return &array->m_vertex[index];
	}

	return NULL;
}

const float *RAS_MeshObject::GetVertexLocation(unsigned int orig_index)
{
	vector<SharedVertex>& sharedmap = m_sharedvertex_map[orig_index];
	vector<SharedVertex>::iterator it = sharedmap.begin();
	return it->m_darray->m_vertex[it->m_offset].getXYZ();
}

void RAS_MeshObject::AddMeshUser(void *clientobj, RAS_MeshSlotList& meshslots, RAS_Deformer *deformer)
{
	for (list<RAS_MeshMaterial>::iterator it = m_materials.begin(); it != m_materials.end(); ++it) {
		RAS_MeshSlot *ms = it->m_bucket->CopyMesh(it->m_baseslot);
		ms->m_clientObj = clientobj;
		ms->SetDeformer(deformer);
		it->m_slots.insert(clientobj, ms);
		meshslots.push_back(ms);
	}
}

void RAS_MeshObject::RemoveFromBuckets(void *clientobj)
{
	for (list<RAS_MeshMaterial>::iterator it = m_materials.begin(); it != m_materials.end(); ++it) {
		RAS_MeshSlot **msp = it->m_slots[clientobj];

		if (!msp)
			continue;

		RAS_MeshSlot *ms = *msp;

		it->m_bucket->RemoveMesh(ms);
		it->m_slots.remove(clientobj);
	}
}

void RAS_MeshObject::EndConversion()
{
#if 0
	m_sharedvertex_map.clear(); // SharedVertex
	vector<vector<SharedVertex> >   shared_null(0);
	shared_null.swap(m_sharedvertex_map);   /* really free the memory */
#endif
}

void RAS_MeshObject::SortPolygons(RAS_MeshSlot& ms, const MT_Transform &transform)
{
	// Limitations: sorting is quite simple, and handles many
	// cases wrong, partially due to polygons being sorted per
	// bucket.
	//
	// a) mixed triangles/quads are sorted wrong
	// b) mixed materials are sorted wrong
	// c) more than 65k faces are sorted wrong
	// d) intersecting objects are sorted wrong
	// e) intersecting polygons are sorted wrong
	//
	// a) can be solved by making all faces either triangles or quads
	// if they need to be z-sorted. c) could be solved by allowing
	// larger buckets, b) and d) cannot be solved easily if we want
	// to avoid excessive state changes while drawing. e) would
	// require splitting polygons.

	RAS_DisplayArray *array = ms.GetDisplayArray();

	unsigned int nvert = 3;
	unsigned int totpoly = array->m_index.size() / nvert;

	if (totpoly <= 1)
		return;

	// Extract camera Z plane...
	const MT_Vector3 pnorm(transform.getBasis()[2]);
	// unneeded: const MT_Scalar pval = transform.getOrigin()[2];

	vector<polygonSlot> poly_slots(totpoly);

	// get indices and z into temporary array
	for (unsigned int j = 0; j < totpoly; j++)
		poly_slots[j].get(array->m_vertex.data(), array->m_index.data(), j * nvert, nvert, pnorm);

	// sort (stable_sort might be better, if flickering happens?)
	std::sort(poly_slots.begin(), poly_slots.end(), backtofront());

	// get indices from temporary array again
	for (unsigned int j = 0; j < totpoly; j++)
		poly_slots[j].set(array->m_index.data(), j * nvert, nvert);
}


bool RAS_MeshObject::HasColliderPolygon()
{
	int numpolys = NumPolygons();
	for (int p = 0; p < numpolys; p++) {
		if (m_Polygons[p]->IsCollider())
			return true;
	}

	return false;
}
