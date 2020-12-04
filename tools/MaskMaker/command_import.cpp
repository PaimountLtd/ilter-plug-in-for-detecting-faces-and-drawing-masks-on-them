/*
*
* Copyright (C) 2017 General Workings Inc
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/
#include "stdafx.h"
#include "utils.h"
#include "command_import.h"
#include "command_morph_import.h"

#define MAX_BONES_PER_SKIN		(8)

#define INHERIT_TYPE_RrSs 0
#define INHERIT_TYPE_RSrs 1
#define INHERIT_TYPE_Rrs 2

#define ALIGNED(XXX) (((size_t)(XXX) & 0xF) ? (((size_t)(XXX) + 0x10) & 0xFFFFFFFFFFFFFFF0ULL) : (size_t)(XXX))

// Vertex Buffer structs from libOBS
//
struct vec3 {
	union {
		struct {
			float x, y, z, w;
		};
		float ptr[4];
		__m128 m;
	};
	vec3() :m() {}
};
struct gs_tvertarray {
	size_t width;
	void *array;
};
struct gs_vb_data {
	size_t num;
	struct vec3 *points;
	struct vec3 *normals;
	struct vec3 *tangents;
	uint32_t *colors;

	size_t num_tex;
	struct gs_tvertarray *tvarray;
};

// wrapper for gs_vb_data
class GSVertexBuffer : public gs_vb_data {
public:
	GSVertexBuffer(size_t numVerts) {
		num = numVerts;
		points = new vec3[num];
		normals = new vec3[num];
		tangents = new vec3[num];
		colors = new uint32_t[num]();
		num_tex = 8;
		tvarray = new gs_tvertarray[num_tex];
		for (int i = 0; i < 8; i++) {
			tvarray[i].width = 4;
			tvarray[i].array = new float[num * 4]();
		}
	}
	~GSVertexBuffer() {
		delete[] points;
		delete[] normals;
		delete[] tangents;
		delete[] colors;
		for (int i = 0; i < num_tex; i++) {
			delete[] tvarray[i].array;
		}
		delete[] tvarray;
	}
	// tex coords
	//
	// 0 | u v
	// 1 | 0 1 2 3
	// 2 | 4 5 6 7
	// 3 | 8 9 10 11
	// 4 | 12 13 14 15
	// 5 | 16 17 18 19
	// 6 | 20 21 22 23
	// 7 | 24 25 26 27
	//
	void set_tex_coord(size_t vidx, float u, float v) {
		float* p = (float*)tvarray[0].array;
		assert(vidx < num);
		p[vidx * 4 + 0] = u;
		p[vidx * 4 + 1] = v;
	}
	void set_tex_coord(size_t vidx, int tidx, float v) {
		assert(vidx < num);
		assert(tidx < (4 * 7));
		float* p = (float*)tvarray[tidx / 4 + 1].array;
		p[vidx * 4 + (tidx % 4)] = v;
	}

	// 16 byte alignment
	size_t align(size_t s) {
		while (s & 0xFULL)
			s++;
		return s;
	}
	uint8_t* align(uint8_t* s) {
		while ((size_t)s & 0xFULL)
			s++;
		return s;
	}

	// raw buffer output
	size_t	size() {
		// us
		size_t s = sizeof(gs_vb_data);
		s = align(s);
		// arrays
		s += sizeof(vec3) * num; // points
		s = align(s);
		s += sizeof(vec3) * num; // normals
		s = align(s);
		s += sizeof(vec3) * num; // tangents
		s = align(s);
		s += sizeof(uint32_t) * num; // colors
		s = align(s);
		s += sizeof(gs_tvertarray) * num_tex; // tvarray
		s = align(s);
		// texture vert arrays
		for (int i = 0; i < num_tex; i++) {
			s += sizeof(float) * tvarray[i].width * num; // uvs
			s = align(s);
		}
		return s;
	}
	void get_data(uint8_t* buffer) {
		assert(buffer);
		assert(((size_t)buffer & 0xFULL) == 0);
		uint8_t* pbuff = buffer;

		gs_vb_data* tmp = (gs_vb_data*)pbuff;
		pbuff += sizeof(gs_vb_data);
		pbuff = align(pbuff);

		tmp->num = num;
		tmp->num_tex = num_tex;

		tmp->points = (vec3*)((size_t)pbuff - (size_t)buffer);
		memcpy(pbuff, points, sizeof(vec3) * num);
		pbuff += sizeof(vec3) * num;
		pbuff = align(pbuff);

		tmp->normals = (vec3*)((size_t)pbuff - (size_t)buffer);
		memcpy(pbuff, normals, sizeof(vec3) * num);
		pbuff += sizeof(vec3) * num;
		pbuff = align(pbuff);

		tmp->tangents = (vec3*)((size_t)pbuff - (size_t)buffer);
		memcpy(pbuff, tangents, sizeof(vec3) * num);
		pbuff += sizeof(vec3) * num;
		pbuff = align(pbuff);

		tmp->colors = (uint32_t*)((size_t)pbuff - (size_t)buffer);
		memcpy(pbuff, colors, sizeof(uint32_t) * num);
		pbuff += sizeof(uint32_t) * num;
		pbuff = align(pbuff);

		tmp->tvarray = (gs_tvertarray*)((size_t)pbuff - (size_t)buffer);
		gs_tvertarray* tmptv = (gs_tvertarray*)pbuff;
		pbuff += sizeof(gs_tvertarray) * num_tex;
		pbuff = align(pbuff);

		for (int i = 0; i < num_tex; i++) {
			tmptv->width = tvarray[i].width;
			tmptv->array = (void*)((size_t)pbuff - (size_t)buffer);
			memcpy(pbuff, tvarray[i].array, sizeof(float) * tvarray[i].width * num);
			pbuff += sizeof(float) * tvarray[i].width * num;
			pbuff = align(pbuff);
			tmptv++;
		}

		// sanity check 
		size_t pos = (size_t)pbuff - (size_t)buffer;
		size_t s = size();
		assert(pos == s);
	}
};


int CheckNodeNames(aiNode* node, int count) {
	if (!node)
		return count;
	if (node->mName.length == 0) {
		char temp[256];
		snprintf(temp, sizeof(temp), "node%d", count++);
		node->mName = temp;
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		count = CheckNodeNames(node->mChildren[i], count);
	}
	return count;
}

int LightNumber(const aiScene* scene, const aiNode* node) {
	for (unsigned int i = 0; i < scene->mNumLights; i++) {
		if (strcmp(scene->mLights[i]->mName.C_Str(), node->mName.C_Str()) == 0) {
			return i;
		}
	}
	return -1;
}

bool HasMeshes(aiNode* node) {
	if (node->mNumMeshes > 0)
		return true;
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		if (HasMeshes(node->mChildren[i]))
			return true;
	}
	return false;
}


bool HasLight(const aiScene* scene, aiNode* node) {
	for (unsigned int i = 0; i < scene->mNumLights; i++) {
		if (strcmp(scene->mLights[i]->mName.C_Str(), node->mName.C_Str()) == 0) {
			return true;
		}
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		if (HasLight(scene, node->mChildren[i]))
			return true;
	}
	return false;
}

void RemovePostRotationNodes(aiNode* node) {
	if (!node)
		return;

	// is this one of those shitty nodes?
	string nodeName = node->mName.C_Str();
	if (nodeName.find("PostRotation") != std::string::npos) {

		// we should have 1 child
		if (node->mNumChildren != 1)
			throw std::logic_error("bad assumption");

		// remove ourselves from the heirarchy
		for (unsigned int i = 0; i < node->mParent->mNumChildren; i++) {
			if (node->mParent->mChildren[i] == node) {
				node->mParent->mChildren[i] = node->mChildren[0];
				break;
			}
		}
		node->mChildren[0]->mParent = node->mParent;
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		RemovePostRotationNodes(node->mChildren[i]);
	}
}

string AddNodes(const aiScene* scene, aiNode* node, json* parts) {
	if (!node)
		return string();

	string nodeName = node->mName.C_Str();
	bool needs_local_nodes = false;
	string source_node_name = string();
	if (nodeName != "root") {
		json part;
		if (node->mParent) {
			string parentName = node->mParent->mName.C_Str();
			if (parentName == "root") {
				// we need to see if this node should really be
				// parented to "root", or parented to "world"
				// - if there are no meshes, make it world
				if (!HasMeshes(node) && HasLight(scene, node)) {
					parentName = "world";
				}
			}
			if (parentName.length() > 0)
				part["parent"] = parentName;
		}

		aiVector3D pos, scl;
		aiQuaterniont<float> rot;
		node->mTransformation.Decompose(scl, rot, pos);

		json p;
		p["x"] = pos.x;
		p["y"] = -pos.y; // flip y
		p["z"] = pos.z;
		part["position"] = p;

		json r;
		r["x"] = rot.x;
		r["y"] = -rot.y; // flip y
		r["z"] = rot.z;
		r["w"] = -rot.w; // flip rot
		part["qrotation"] = r;

		json s;
		s["x"] = scl.x;
		s["y"] = scl.y;
		s["z"] = scl.z;
		part["scale"] = s;

		// insert inherit type if other than the default
		// INHERIT_TYPE_RrSs: Apply parent scaling after child scaling.
		// INHERIT_TYPE_RSrs: What logically should happen: First parent rotation and scaling, and then child's. 
		// INHERIT_TYPE_Rrs:  Parent scaling is ignored.
		if (node->mMetaData) {
			int type;
			if (node->mMetaData->Get("InheritType", type))
			{
				needs_local_nodes = true;
				source_node_name = node->mName.C_Str();
				if (type != INHERIT_TYPE_RSrs)
				{
					part["inherit-type"] = type;
				}
			}
		}

		// add mesh resources
		json rez;
		for (unsigned int i = 0; i < node->mNumMeshes; i++) {
			char temp[256];
			snprintf(temp, sizeof(temp), "%d", i);
			string r = scene->mMeshes[node->mMeshes[i]]->mName.C_Str();
			r += "Model";
			rez[temp] = r;
		}

		// we might be a light transform
		int lightNum = LightNumber(scene, node);
		if (lightNum >= 0) {
			char temp1[256];
			snprintf(temp1, sizeof(temp1), "%d", node->mNumMeshes);
			char temp2[256];
			snprintf(temp2, sizeof(temp2), "light%d", lightNum);
			rez[temp1] = temp2;
		}

		if (node->mNumMeshes > 0 || lightNum >= 0)
			part["resources"] = rez;

		if (!needs_local_nodes && node->mNumChildren == 1) {
			string local_to = AddNodes(scene, node->mChildren[0], parts);
			if (local_to.length() > 0) {
				part["local-to"] = local_to;
			}
			(*parts)[node->mName.C_Str()] = part;
			return local_to;
		}

		(*parts)[node->mName.C_Str()] = part;
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		AddNodes(scene, node->mChildren[i], parts);
	}

	return source_node_name;
}

aiNode* FindNode(aiNode* node, const string& name) {
	if (!node)
		return nullptr;
	if (name == node->mName.C_Str())
		return node;
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		aiNode* n = FindNode(node->mChildren[i], name);
		if (n)
			return n;
	}
	return nullptr;
}


string getMaterialTexture(aiMaterial* mtl, aiTextureType tt) {
	int nt = mtl->GetTextureCount(tt);
	string p = "";
	if (nt > 0) {
		aiString path;
		mtl->GetTexture(tt, 0, &path);
		p = path.C_Str();
	}
	return p;
}

string lightTypeToString(aiLightSourceType t) {
	switch (t) {
	case aiLightSource_DIRECTIONAL:
		return "directional";
	case aiLightSource_POINT:
		return "point";
	case aiLightSource_SPOT:
		return "spot";
	case aiLightSource_AMBIENT:
		return "ambient";
	case aiLightSource_AREA:
		return "area";
	}
	return "";
}

string AnimBehaviourToString(aiAnimBehaviour b) {
	if (b == aiAnimBehaviour_DEFAULT)
		return "repeat";
	if (b == aiAnimBehaviour_CONSTANT)
		return "constant";
	if (b == aiAnimBehaviour_LINEAR)
		return "linear";
	if (b == aiAnimBehaviour_REPEAT)
		return "repeat";
	return "repeat";
}

struct VtxToBone {
	int		bone;	// index into mesh bones list
	float	weight;
};

struct Vtx {
	int						index;
	std::vector<VtxToBone>	bones;
};

struct Tri {
	bool					touched;
	std::vector<int>		bones; // indices into mesh bones list
};

bool AllTrianglesTouched(Tri* tris, int numTris) {
	for (int j = 0; j < numTris; j++)
		if (!tris[j].touched)
			return false;
	return true;
}

int GetBoneIndex(const std::vector<int>& bones, int b) {
	for (unsigned int i = 0; i < bones.size(); i++)
		if (bones[i] == b)
			return i;
	return -1;
}

bool HasBone(const std::vector<VtxToBone>& v, int b) {
	for (unsigned int i = 0; i < v.size(); i++) 
		if (v[i].bone == b)
			return true;
	return false;
}

bool HasInt(const std::vector<int>& v, int x) {
	for (unsigned int i = 0; i < v.size(); i++)
		if (v[i] == x)
			return true;
	return false;
}

vec3 GetCenter(const GSVertexBuffer& vertices) {
	vec3 center;
	memset(&center, 0, sizeof(vec3));

	for (int i = 0; i < vertices.num; i++) {
		center.x += vertices.points[i].x;
		center.y += vertices.points[i].y;
		center.z += vertices.points[i].z;
	}
	center.x /= (float)vertices.num;
	center.y /= (float)vertices.num;
	center.z /= (float)vertices.num;

	return center;
}


void command_import(Args& args) {

	// get filename
	string resFile = args.value("file");
	if (resFile.length() == 0) {
		cout << "You must specify a file with import." << endl;
		return;
	}

	cout << "Importing '" << resFile << "'..." << endl;

	// make new json
	json j = args.createNewJson();
	j["description"] = "MaskMaker import of " + Utils::get_filename(resFile) + "." + Utils::get_extension(resFile);
	args.jptr = &j;

	// ASSIMP : Import the scene from whatever file it is (dae/fbx/...)
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(resFile,
		aiProcess_TransformUVCoords |
		aiProcess_Triangulate |
		aiProcess_GenNormals |
		aiProcess_CalcTangentSpace |
		//aiProcess_OptimizeGraph |   BOO! THIS GETS RID OF LOCATORS todo: mod assimp!
		//aiProcess_OptimizeMeshes |
		aiProcess_SortByPType);

	// If the import failed, report it
	if (!scene) {
		cout << "Assimp is unable to import '" << resFile << "'." << endl;
		return;
	}

	// Get a list of all the textures
	map<std::tuple<string,unsigned int>, string> textureFiles;
	for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
		auto tex_map = scene->mMaterials[i]->GetAllTextures();
		for (auto &kv : tex_map)
		{
			textureFiles[std::make_tuple(kv.first,i)] = kv.second;
		}
	}

	json rez;

	// Add all the meshes
	cout << "Importing " << scene->mNumMeshes << " meshes..." << endl;
	// first clean up the names
	for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
		// sometimes meshes don't have names
		if (scene->mMeshes[i]->mName.length == 0) {
			char temp[256];
			snprintf(temp, sizeof(temp), "mesh%d", i);
			scene->mMeshes[i]->mName = temp;
		}
		// or have identical names
		else {
			std::string current_name = scene->mMeshes[i]->mName.C_Str();
			size_t uidx = 0;
			for (unsigned int j = i + 1; j < scene->mNumMeshes; j++) {
				if (current_name == scene->mMeshes[j]->mName.C_Str())
				{
					scene->mMeshes[j]->mName = current_name + std::to_string(++uidx);
				}
			}
			if (uidx > 0)
				scene->mMeshes[i]->mName = current_name + "0";
		}
	}

	for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[i];

		if (!mesh->mTangents) {
			cout << "*** MESH HAS NO TANGENTS! (NO NORMAL MAPPING) ***" << endl;
		}
		if (!mesh->mTextureCoords[0]) {
			cout << "*** MESH HAS NO TEXTURE COORDINATES! (NO TEXTURE MAPPING) ***" << endl;
		}

		// is this a skinned mesh?
		if (mesh->mNumBones > 0) {

			// We need to build back-references from vertices and triangles to bones
			// Build those now
			Vtx* verts = new Vtx[mesh->mNumVertices];
			Tri* tris = new Tri[mesh->mNumFaces];

			// create connections from vertices -> bones
			for (unsigned int j = 0; j < mesh->mNumBones; j++) {
				aiBone* bone = mesh->mBones[j];
				// Add vtx -> bone connections
				for (unsigned int k = 0; k < bone->mNumWeights; k++) {
					// no dupes
					if (!HasBone(verts[bone->mWeights[k].mVertexId].bones, j)) {
						VtxToBone v2b;
						v2b.bone = j;
						v2b.weight = bone->mWeights[k].mWeight;
						verts[bone->mWeights[k].mVertexId].bones.emplace_back(v2b);
					}
				}
			}

			// create connections from triangles -> bones
			for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
				aiFace& face = mesh->mFaces[j];
				assert(face.mNumIndices == 3);
				// Triangle not touched
				tris[j].touched = false;
				// Add tri -> bone connections from triangle vertex bones
				for (unsigned int v = 0; v < 3; v++) {
					for (unsigned int k = 0; k < verts[face.mIndices[v]].bones.size(); k++) {
						int b = verts[face.mIndices[v]].bones[k].bone;
						if (!HasInt(tris[j].bones, b)) {
							tris[j].bones.push_back(b);
						}
					}
				}
			}

			// sanity checks
			for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
				if (verts[j].bones.size() == 0) {
					cout << "WARNING! SKINNED MESH HAS ENTIRELY UNWEIGHTED VERTEX!" << endl;
				}
				if (verts[j].bones.size() > MAX_BONES_PER_SKIN) {
					cout << "WARNING! SKINNED MESH VERTEX " << j << " HAS TOO MANY WEIGHTS! " << verts[j].bones.size() << endl;
					for (unsigned int k = 0; k < verts[j].bones.size(); k++) {
						cout << " vert bone index: " << verts[j].bones[k].bone << " : " << verts[j].bones[k].weight << endl;
					}
				}
				float total = 0.0f;
				for (unsigned int k = 0; k < verts[j].bones.size(); k++) {
					total += verts[j].bones[k].weight;
					if (verts[j].bones[k].weight < 0.001) {
						cout << "WARNING! SKINNED MESH HAS VERTEX WITH ZERO WEIGHT!" << endl;
					}
				}
				if (total < 0.99f) {
					cout << "WARNING! SKINNED MESH HAS VERTEX WITH NON-UNITY SUM WEIGHTS!" << endl;
				}
			}
			for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
				if (tris[j].bones.size() > MAX_BONES_PER_SKIN) {
					cout << "WARNING! SKINNED MESH TRIANGLE " << j << " HAS TOO MANY WEIGHTS! " << tris[j].bones.size() << endl;
				}
			}

			// create the skinned mesh json object
			json o;
			o["type"] = "skinned-model";

			// set material
			char temp[256];
			snprintf(temp, sizeof(temp), "material%d", mesh->mMaterialIndex);
			o["material"] = temp;

			// add bones list
			json bnz;
			for (unsigned int j = 0; j < mesh->mNumBones; j++) {
				aiBone* bone = mesh->mBones[j];
				// for lack of a better idea

				// decompose matrix 
				aiVector3D pos, scl;
				aiQuaterniont<float> rot;
				bone->mOffsetMatrix.Decompose(scl, rot, pos);

				json bn;
				json p;
				p["x"] = pos.x;
				p["y"] = -pos.y; // flip y
				p["z"] = pos.z;
				bn["position"] = p;

				json r;
				r["x"] = rot.x;
				r["y"] = -rot.y; // flip y
				r["z"] = rot.z;
				r["w"] = -rot.w; // flip rot
				bn["qrotation"] = r;

				json s;
				s["x"] = scl.x;
				s["y"] = scl.y;
				s["z"] = scl.z;
				bn["scale"] = s;

				// add the name
				bn["name"] = string(bone->mName.C_Str());

				char ttt[32];
				snprintf(ttt, sizeof(ttt), "%d", j);
				bnz[ttt] = bn;
			}
			o["bones"] = bnz;

			// reuse these lists for all the skins
			int numIndices = 0;
			int numVertices = 0;
			unsigned int* indices = new unsigned int[mesh->mNumFaces * 3];
			GSVertexBuffer vertices(mesh->mNumVertices);

			// Keep going until we've touched all the triangles
			int numSkins = 0;
			json sknz;
			while (!AllTrianglesTouched(tris, mesh->mNumFaces)) {

				// Clear vtx indices
				for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
					verts[j].index = -1;
				}

				// Reset Skin vars
				numIndices = 0;
				numVertices = 0;
				vertices.num = mesh->mNumVertices;
				std::vector<int> bones;

				// Walk triangles
				for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
					if (!tris[j].touched) {
						aiFace& face = mesh->mFaces[j];

						// How many new bones will this triangle introduce?
						int numNewBones = 0;
						for (unsigned int k = 0; k < tris[j].bones.size(); k++) {
							bool found = false;
							for (unsigned int l = 0; l < bones.size(); l++) {
								if (bones[l] == tris[j].bones[k]) {
									found = true;
									break;
								}
							}
							if (!found)
								numNewBones++;
						}
						
						// Can we add this triangle?
						if ((bones.size() + numNewBones) <= MAX_BONES_PER_SKIN) {

							// Add the new bones from triangle
							for (unsigned int k = 0; k < tris[j].bones.size(); k++) {
								bool found = false;
								for (unsigned int l = 0; l < bones.size(); l++) {
									if (bones[l] == tris[j].bones[k]) {
										found = true;
										break;
									}
								}
								if (!found) {
									bones.push_back(tris[j].bones[k]);
								}
							}

							// Add each index/vertex
							for (unsigned int k = 0; k < 3; k++) {
								int v = mesh->mFaces[j].mIndices[k];
								// need to add vertex?
								if (verts[v].index < 0) {
									// Add the new vertex
									vertices.points[numVertices].x = mesh->mVertices[v].x;
									vertices.points[numVertices].y = -mesh->mVertices[v].y;
									vertices.points[numVertices].z = mesh->mVertices[v].z;
									vertices.normals[numVertices].x = mesh->mNormals[v].x;
									vertices.normals[numVertices].y = -mesh->mNormals[v].y;
									vertices.normals[numVertices].z = mesh->mNormals[v].z;
									if (mesh->mTangents) {
										vertices.tangents[numVertices].x = mesh->mTangents[v].x;
										vertices.tangents[numVertices].y = mesh->mTangents[v].y;
										vertices.tangents[numVertices].z = mesh->mTangents[v].z;
									}
									if (mesh->mTextureCoords[0]) {
										vertices.set_tex_coord(numVertices, 
											mesh->mTextureCoords[0][v].x,
											1.0f - mesh->mTextureCoords[0][v].y);
									}
									// use extra tex coords to store bones & weights for shader
									vertices.set_tex_coord(numVertices, 0, (float)verts[v].bones.size());
									for (unsigned int b = 0; b < verts[v].bones.size(); b++) {
										int bb = (b + 1) * 2; // skip first, 2 for each bone
										assert(bb < (4 * 7));
										vertices.set_tex_coord(numVertices, bb, (float)GetBoneIndex(bones, verts[v].bones[b].bone));
										vertices.set_tex_coord(numVertices, bb + 1, verts[v].bones[b].weight);
									}
									verts[v].index = numVertices;
									numVertices++;
								}
								indices[numIndices++] = verts[v].index;
							}

							// This triangle is done
							tris[j].touched = true;
						}
					} // end: can we add this triangle?
				} // end: for each triangle

				// Break endless loop
				if (numVertices == 0) {
					cout << "COULD NOT CREATE SKINNED MESH. BAILING." << endl;
					break;
				}

				vertices.num = numVertices;
				cout << "Creating skin with " << numVertices << " vertices, " << numIndices / 3 << " triangles" << endl;

				// encode 
				size_t vbuffSize = vertices.size();
				uint8_t* vbuff = new uint8_t[vbuffSize + 16]();
				uint8_t* aligned = (uint8_t*)ALIGNED(vbuff);
				vertices.get_data(aligned);
				string vertexDataBase64 =
					base64_encodeZ(aligned, vbuffSize);
				string indexDataBase64 =
					base64_encodeZ((uint8_t*)indices, sizeof(unsigned int) * numIndices);
				delete[] vbuff;

				// make mesh name
				char tt[1024];
				snprintf(tt, sizeof(tt), "%s_skin%d", mesh->mName.C_Str(), numSkins++);

				// center
				json jcenter;
				vec3 center = GetCenter(vertices);
				jcenter["x"] = center.x;
				jcenter["y"] = center.y;
				jcenter["z"] = center.z;

				// Add mesh resource
				json mo;
				mo["type"] = "mesh";
				mo["vertex-buffer"] = vertexDataBase64;
				mo["index-buffer"] = indexDataBase64;
				mo["center"] = jcenter;
				rez[tt] = mo;

				// Add skin
				json skn;
				json sknbnz;
				for (unsigned int j = 0; j < bones.size(); j++) {
					char ttt[32];
					snprintf(ttt, sizeof(ttt), "%d", j);
					sknbnz[ttt] = bones[j];
				}
				skn["bones"] = sknbnz;
				skn["mesh"] = tt;
				sknz[tt] = skn;

			} // end: while: until all triangles are touched

			// set our skins
			o["skins"] = sknz;

			// Add a skinned model to resources list
			snprintf(temp, sizeof(temp), "%sModel", mesh->mName.C_Str());
			rez[temp] = o;

			// clean up
			delete[] verts;
			delete[] tris;
			delete[] indices;
		}
		else {
			// Create vertex list
			GSVertexBuffer vertices(mesh->mNumVertices);
			for (unsigned int j = 0; j < mesh->mNumVertices; j++) {

				// Add the new vertex
				vertices.points[j].x = mesh->mVertices[j].x;
				vertices.points[j].y = -mesh->mVertices[j].y;
				vertices.points[j].z = mesh->mVertices[j].z;
				vertices.normals[j].x = mesh->mNormals[j].x;
				vertices.normals[j].y = -mesh->mNormals[j].y;
				vertices.normals[j].z = mesh->mNormals[j].z;
				if (mesh->mTangents) {
					vertices.tangents[j].x = mesh->mTangents[j].x;
					vertices.tangents[j].y = mesh->mTangents[j].y;
					vertices.tangents[j].z = mesh->mTangents[j].z;
				}
				if (mesh->mTextureCoords[0]) {
					vertices.set_tex_coord(j,
						mesh->mTextureCoords[0][j].x,
						1.0f - mesh->mTextureCoords[0][j].y);
				}
			}
			// encode vertices
			size_t vbuffSize = vertices.size();
			uint8_t* vbuff = new uint8_t[vbuffSize + 16]();
			uint8_t* aligned = (uint8_t*)ALIGNED(vbuff);
			vertices.get_data(aligned);
			string vertexDataBase64 =
				base64_encodeZ(aligned, vbuffSize);

			delete[] vbuff;

			// Build index list
			unsigned int* indices = new unsigned int[mesh->mNumFaces * 3]();
			int indIdx = 0;
			for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
				aiFace& face = mesh->mFaces[j];
				assert(face.mNumIndices == 3);
				indices[indIdx++] = face.mIndices[0];
				indices[indIdx++] = face.mIndices[1];
				indices[indIdx++] = face.mIndices[2];
			}
			// encode indices
			string indexDataBase64 =
				base64_encodeZ((uint8_t*)indices, sizeof(unsigned int) * indIdx);

			// center
			json jcenter;
			vec3 center = GetCenter(vertices);
			jcenter["x"] = center.x;
			jcenter["y"] = center.y;
			jcenter["z"] = center.z;

			// Add mesh resource
			json o;
			o["type"] = "mesh";
			o["vertex-buffer"] = vertexDataBase64;
			o["index-buffer"] = indexDataBase64;
			o["center"] = jcenter;
			rez[mesh->mName.C_Str()] = o;

			// clean up
			delete[] indices;
		}
	}

	// Add all the textures
	int count = 0;
	cout << "Importing textures..." << endl;
	map<std::tuple<string, unsigned int>, bool> textureHasAlpha;
	for (auto it = textureFiles.begin(); it != textureFiles.end(); it++, count++) {
		json o = args.createImageResourceFromFile(it->second);
		textureHasAlpha[it->first] = args.lastImageHadAlpha;
		if (args.lastImageHadAlpha)
			cout << std::get<string>(it->first) << " " << " has alpha" << endl;
		rez[std::get<string>(it->first)+"-"+std::to_string(std::get<unsigned int>(it->first))] = o;
	}
	cout << "Imported " << count << " textures." << endl;

	// Add all the materials
	count = 0;
	cout << "Importing " << scene->mNumMaterials << " materials..." << endl;
	for (unsigned int i = 0; i < scene->mNumMaterials; i++) {

		aiMaterial* mtl = scene->mMaterials[i];

		char temp[256];
		json params;
		string imgfile;

		for (unsigned int j = 0; j < mtl->mNumProperties; j++) {
			aiMaterialProperty* pp = mtl->mProperties[j];
		}

		float reflectivity;
		mtl->Get(AI_MATKEY_REFLECTIVITY, reflectivity);
		float opacity;
		mtl->Get(AI_MATKEY_OPACITY, opacity);
		float strength;
		mtl->Get(AI_MATKEY_SHININESS_STRENGTH, strength);
		float shininess;
		mtl->Get(AI_MATKEY_SHININESS, shininess);

		// Texture params
		for (auto &kv : textureFiles)
		{
			if (std::get<unsigned int>(kv.first) == i) {
				string key = std::get<string>(kv.first) + "-" + std::to_string(std::get<unsigned int>(kv.first));
				json parm;
				parm["type"] = "texture";
				parm["value"] = key;
				params[std::get<string>(kv.first) + "Tex"] = parm;
				parm["type"] = "integer";
				parm["value"] = 1;
				params[std::get<string>(kv.first) + "Map"] = parm;
			}
		}

		// Opaque flag, set based on textures
		bool opaque = true;
		vector<aiTextureType> ttypes = { aiTextureType_AMBIENT(), aiTextureType_DIFFUSE(),
			aiTextureType_SPECULAR(), aiTextureType_EMISSIVE(), aiTextureType_REFLECTION() };

		for (auto &kv : textureFiles)
		{
			if (std::get<unsigned int>(kv.first) == i) {
				if (textureHasAlpha[kv.first]) {
					opaque = false;
					break;
				}

			}
		}

		// Only set colors if textures aren't set
		int namb = mtl->GetTextureCount(aiTextureType_AMBIENT());
		int ndff = mtl->GetTextureCount(aiTextureType_DIFFUSE());
		int nspc = mtl->GetTextureCount(aiTextureType_SPECULAR());
		int nemm = mtl->GetTextureCount(aiTextureType_EMISSIVE());

		// Color Params
		aiColor4D dcolor = aiColor4D(0.8f, 0.8f, 0.8f, 1.0f);
		if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &dcolor) &&
			ndff == 0) {
			json parm;
			json cc;
			cc["x"] = dcolor.r;
			cc["y"] = dcolor.g;
			cc["z"] = dcolor.b;
			cc["w"] = dcolor.a;
			parm["type"] = "float4";
			parm["value"] = cc;
			params["diffuseColor"] = parm;
			if (dcolor.a < 1.0f) 
				opaque = false;
		}
		aiColor4D scolor = aiColor4D(0.0f, 0.0f, 0.0f, 1.0f);
		if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_SPECULAR, &scolor) &&
			nspc == 0) {
			// assimp stores specularfactor in shininess_strength
			// attenuate specularcolor using this number
			float strength = 1;
			aiGetMaterialFloat(mtl, AI_MATKEY_SHININESS_STRENGTH, &strength);
			json parm;
			json cc;
			cc["x"] = scolor.r * strength;
			cc["y"] = scolor.g * strength;
			cc["z"] = scolor.b * strength;
			cc["w"] = scolor.a;
			parm["type"] = "float4";
			parm["value"] = cc;
			params["specularColor"] = parm;
			if (scolor.a < 1.0f)
				opaque = false;
		}
		aiColor4D acolor = aiColor4D(0.2f, 0.2f, 0.2f, 1.0f);
		if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_AMBIENT, &acolor) &&
			namb == 0) {
			json parm;
			json cc;
			cc["x"] = acolor.r;
			cc["y"] = acolor.g;
			cc["z"] = acolor.b;
			cc["w"] = acolor.a;
			parm["type"] = "float4";
			parm["value"] = cc;
			params["ambientColor"] = parm;
			if (acolor.a < 1.0f)
				opaque = false;
		}
		aiColor4D ecolor = aiColor4D(0.0f, 0.0f, 0.0f, 1.0f);
		if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_EMISSIVE, &ecolor) &&
			nemm == 0) {
			json parm;
			json cc;
			cc["x"] = ecolor.r;
			cc["y"] = ecolor.g;
			cc["z"] = ecolor.b;
			cc["w"] = ecolor.a;
			parm["type"] = "float4";
			parm["value"] = cc;
			params["emissiveColor"] = parm;
			if (ecolor.a < 1.0f)
				opaque = false;
		}

		// Culling
		int two_sided = 0;
		if (AI_SUCCESS == aiGetMaterialInteger(mtl, AI_MATKEY_TWOSIDED, &two_sided) && two_sided) {
			args.kvpairs["culling"] = "neither";
		}

		if (opaque)
			cout << "Material " << count << " is opaque." << endl;
		else
			cout << "Material " << count << " is NOT opaque." << endl;

		int mode[10];
		unsigned int max = 10;
		aiGetMaterialIntegerArray(mtl, AI_MATKEY_UVWSRC_DIFFUSE(0), mode, &max);

		max = 10;
		aiGetMaterialIntegerArray(mtl, AI_MATKEY_UVWSRC_HEIGHT(0), mode, &max);

		// Shininess
		{
			float shininess = 8;
			aiGetMaterialFloat(mtl, AI_MATKEY_SHININESS, &shininess);

			// nope
			//
			//float strength = 1;
			//if (AI_SUCCESS == aiGetMaterialFloat(mtl, AI_MATKEY_SHININESS_STRENGTH, &strength)) {
			//	shininess *= strength;
			//}

			json parm;
			parm["type"] = "float";
			parm["value"] = shininess;
			params["shininess"] = parm;
		}

		// make the material
		string effect = "effectPhong";
		snprintf(temp, sizeof(temp), "material%d", count++);
		rez[temp] = args.createMaterial(params, effect);
		rez[temp]["opaque"] = opaque;
	}

	// Add models
	cout << "Importing " << scene->mNumMeshes << " models..." << endl;
	for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[i];

		// only non-skinned models
		if (mesh->mNumBones == 0) {
			char temp[256];
			snprintf(temp, sizeof(temp), "material%d", mesh->mMaterialIndex);

			// model
			json o;
			o["type"] = "model";
			o["mesh"] = mesh->mName.C_Str();
			o["material"] = temp;

			snprintf(temp, sizeof(temp), "%sModel", mesh->mName.C_Str());
			rez[temp] = o;
		}
	}

	// Add lights
	cout << "Importing " << scene->mNumLights << " lights..." << endl;
	for (unsigned int i = 0; i < scene->mNumLights; i++) {
		aiLight* light = scene->mLights[i];

		// light type
		json o;
		o["type"] = "light";
		o["light-type"] = lightTypeToString(light->mType);

		// position/attenuation factors (not valid for directional lights)
		if (light->mType != aiLightSource_DIRECTIONAL &&
			light->mType != aiLightSource_AMBIENT) {
			json p;
			p["x"] = light->mPosition.x;
			p["y"] = light->mPosition.y;
			p["z"] = light->mPosition.z;
			o["position"] = p;

			o["att0"] = light->mAttenuationConstant;
			o["att1"] = light->mAttenuationLinear;
			o["att2"] = light->mAttenuationQuadratic;
		}

		// direction/up (not valid for point lights)
		if (light->mType != aiLightSource_POINT &&
			light->mType != aiLightSource_AMBIENT) {

			// wow. awesome.
			if (light->mDirection.SquareLength() < 0.0001f) {
				light->mDirection.z = 1.0;
			}
			json p;
			p["x"] = light->mDirection.x;
			p["y"] = light->mDirection.y;
			p["z"] = light->mDirection.z;
			o["direction"] = p;

			// seriously assimp? why even bother?
			if (light->mUp.SquareLength() < 0.0001f) {
				light->mUp.y = 1.0f;
			}
			p["x"] = light->mUp.x;
			p["y"] = light->mUp.y;
			p["z"] = light->mUp.z;
			o["up"] = p;
		}

		// color : ambient
		json c;
		c["x"] = light->mColorAmbient.r;
		c["y"] = light->mColorAmbient.g;
		c["z"] = light->mColorAmbient.b;
		o["ambient"] = c;

		// color : diffuse
		c["x"] = light->mColorDiffuse.r;
		c["y"] = light->mColorDiffuse.g;
		c["z"] = light->mColorDiffuse.b;
		o["diffuse"] = c;

		// color : specular
		c["x"] = light->mColorSpecular.r;
		c["y"] = light->mColorSpecular.g;
		c["z"] = light->mColorSpecular.b;
		o["specular"] = c;

		// spot angles (only valid for spot lights)
		if (light->mType == aiLightSource_SPOT) {

			o["inner-angle"] = light->mAngleInnerCone;
			o["outer-angle"] = light->mAngleOuterCone;
		}

		// area (only valid for area lights)
		if (light->mType == aiLightSource_AREA) {
			json v;
			v["x"] = light->mSize.x;
			v["y"] = light->mSize.y;
			o["area-size"] = v;
		}

		// we rename all the lights 
		char temp[256];
		snprintf(temp, sizeof(temp), "light%d", i);
		rez[temp] = o;
	}

	// Add animations
	cout << "Adding animations..." << endl;
	ImportAnimations(args, scene, rez);

	// Set resources
	j["resources"] = rez;

	// Make sure the nodes have names
	count = 0;
	CheckNodeNames(scene->mRootNode, count);

	// Make their root the facemask root
	scene->mRootNode->mName = "root";

	// hrm
	RemovePostRotationNodes(scene->mRootNode);

	// Add parts
	json parts;
	AddNodes(scene, scene->mRootNode, &parts);
	j["parts"] = parts;

	// write it out
	args.writeJson(j);
	cout << "Done!" << endl << endl;
}


static string g_locator_name = "landmark";
static string g_morph_channel_name = "morph";

string GetChannelName(string nodeName, string varName) {
	if (nodeName.substr(0, g_locator_name.size()) == g_locator_name) {
		// get point index
		int idx = atoi(nodeName.substr(g_locator_name.size()).c_str());
		if (idx >= 0 && idx < NUM_LANDMARK_POINTS) {
			char temp[256];
			snprintf(temp, sizeof(temp), "%s-%d-%s", g_morph_channel_name.c_str(), idx, varName.c_str());
			return temp;
		}
	}
	return "";
}

int GetDeltaIndex(string nodeName) {
	if (nodeName.substr(0, g_locator_name.size()) == g_locator_name) {
		// get point index
		int idx = atoi(nodeName.substr(g_locator_name.size()).c_str());
		return idx;
	}
	return -1;
}

void ImportAnimations(Args& args, const aiScene* scene, json& rez, 
	bool forMorph, aiVector3D* rest_points) {

	for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
		aiAnimation* anim = scene->mAnimations[i];

		char temp[256];

		if (anim->mName.length == 0) {
			snprintf(temp, sizeof(temp), "animation%d", i);
			anim->mName = temp;
		}

		string morphName;
		if (forMorph) {
			assert(rest_points);
			string poseFile = args.value("posefile");
			morphName = Utils::get_filename(poseFile);
		}

		json janim;
		janim["type"] = "animation";
		janim["duration"] = anim->mDuration;
		janim["fps"] = anim->mTicksPerSecond;

		int jchanCount = 0;
		json jchannels;

		// add node anim channels
		for (unsigned int j = 0; j < anim->mNumChannels; j++) {
			aiNodeAnim* chan = anim->mChannels[j];

			// get the node
			aiNode* node = FindNode(scene->mRootNode, chan->mNodeName.C_Str());
			// and transform
			aiVector3D pos, scl;
			aiQuaterniont<float> rot;
			node->mTransformation.Decompose(scl, rot, pos);

			// Position
			if (chan->mNumPositionKeys > 0) {
				// get the key values
				std::vector<float> xkeys((int)std::ceil(anim->mDuration));
				std::vector<float> ykeys((int)std::ceil(anim->mDuration));
				std::vector<float> zkeys((int)std::ceil(anim->mDuration));
				bool xch, ych, zch;
				xch = ych = zch = false;
				for (unsigned int k = 0; k < chan->mNumPositionKeys; k++) {
					const aiVectorKey& key = chan->mPositionKeys[k];
					aiVector3D v = key.mValue;
					if (FLT_NEQ((float)v.x, pos.x))
						xch = true;
					if (FLT_NEQ((float)v.y, pos.y))
						ych = true;
					if (FLT_NEQ((float)v.z, pos.z))
						zch = true;
				}
				bool found_keyframe;
				for (size_t frame = 0; frame < anim->mDuration; frame++)
				{
					double timestamp = (double) frame;
					found_keyframe = false;
					for (size_t keyframe = 0; keyframe < chan->mNumPositionKeys-1; keyframe++)
					{
						const aiVectorKey& key = chan->mPositionKeys[keyframe];
						double current_time = key.mTime;
						const aiVector3D &cv = key.mValue;

						const aiVectorKey& next_key = chan->mPositionKeys[keyframe+1];
						double next_time = next_key.mTime;
						const aiVector3D &nv = key.mValue;

						if (current_time <= timestamp && next_time > timestamp) {
							double delta_time = next_time - current_time;

							// For now do linear interpolation
							// In general case, we'll probably need bezier/spline curve evaluation
							double x = (nv.x - cv.x) / delta_time * (timestamp - current_time) + cv.x;
							double y = (nv.y - cv.y) / delta_time * (timestamp - current_time) + cv.y;
							double z = (nv.z - cv.z) / delta_time * (timestamp - current_time) + cv.z;
							aiVector3D v((float)x, (float)y,(float)z);

							if (forMorph) {
								int idx = GetDeltaIndex(chan->mNodeName.C_Str());
								v = v - rest_points[idx];
								v.y = -v.y;
								v.z = -v.z;
							}
							else {
								v.y = -v.y;
							}

							xkeys[frame] = v.x;
							ykeys[frame] = v.y;
							zkeys[frame] = v.z;
							found_keyframe = true;
							break;
						}
						else if (current_time > timestamp && next_time > timestamp) {
							// the first keyframe is some time in the future
							// use a constant repeat, until we reach to that time
							aiVector3D v((float)cv.x, (float)cv.y, (float)cv.z);

							if (forMorph) {
								int idx = GetDeltaIndex(chan->mNodeName.C_Str());
								v = v - rest_points[idx];
								v.y = -v.y;
								v.z = -v.z;
							}
							else {
								v.y = -v.y;
							}
							xkeys[frame] = v.x;
							ykeys[frame] = v.y;
							zkeys[frame] = v.z;
							found_keyframe = true;
							break;
						}
					}
					if (!found_keyframe) {
						// the frame is far past the last keyframe
						// repeat the last keyframe
						const aiVectorKey& last_key = chan->mPositionKeys[chan->mNumPositionKeys - 1];
						aiVector3D v((float)last_key.mValue.x, (float)last_key.mValue.y, (float)last_key.mValue.z);

						if (forMorph) {
							int idx = GetDeltaIndex(chan->mNodeName.C_Str());
							v = v - rest_points[idx];
							v.y = -v.y;
							v.z = -v.z;
						}
						else {
							v.y = -v.y;
						}
						xkeys[frame] = v.x;
						ykeys[frame] = v.y;
						zkeys[frame] = v.z;
					}
				}
				// only add non-static channels
				if (xch) {
					json jchan;
					if (forMorph) {
						jchan["name"] = morphName;
						jchan["type"] = GetChannelName(chan->mNodeName.C_Str(), "x");
					}
					else {
						jchan["name"] = chan->mNodeName.C_Str();
						jchan["type"] = "part-pos-x";
					}
					jchan["pre-state"] = AnimBehaviourToString(chan->mPreState);
					jchan["post-state"] = AnimBehaviourToString(chan->mPostState);
					jchan["values"] = base64_encodeZ((uint8_t*)xkeys.data(),
						(unsigned int)(sizeof(float) * xkeys.size()));
					snprintf(temp, sizeof(temp), "%d", jchanCount++);
					jchannels[temp] = jchan;
				}
				if (ych) {
					json jchan;
					if (forMorph) {
						jchan["name"] = morphName;
						jchan["type"] = GetChannelName(chan->mNodeName.C_Str(), "y");
					}
					else {
						jchan["name"] = chan->mNodeName.C_Str();
						jchan["type"] = "part-pos-y";
					}
					jchan["pre-state"] = AnimBehaviourToString(chan->mPreState);
					jchan["post-state"] = AnimBehaviourToString(chan->mPostState);
					jchan["values"] = base64_encodeZ((uint8_t*)ykeys.data(),
						(unsigned int)(sizeof(float) * ykeys.size()));
					snprintf(temp, sizeof(temp), "%d", jchanCount++);
					jchannels[temp] = jchan;
				}
				if (zch) {
					json jchan;
					if (forMorph) {
						jchan["name"] = morphName;
						jchan["type"] = GetChannelName(chan->mNodeName.C_Str(), "z");
					}
					else {
						jchan["name"] = chan->mNodeName.C_Str();
						jchan["type"] = "part-pos-z";
					}
					jchan["pre-state"] = AnimBehaviourToString(chan->mPreState);
					jchan["post-state"] = AnimBehaviourToString(chan->mPostState);
					jchan["values"] = base64_encodeZ((uint8_t*)zkeys.data(),
						(unsigned int)(sizeof(float) * zkeys.size()));
					snprintf(temp, sizeof(temp), "%d", jchanCount++);
					jchannels[temp] = jchan;
				}
			}
			// Rotation
			if (chan->mNumRotationKeys > 0 && !forMorph) {
				// get the key values
				std::vector<float> xkeys((int)std::ceil(anim->mDuration));
				std::vector<float> ykeys((int)std::ceil(anim->mDuration));
				std::vector<float> zkeys((int)std::ceil(anim->mDuration));
				std::vector<float> wkeys((int)std::ceil(anim->mDuration));
				bool xch, ych, zch, wch;
				xch = ych = zch = wch = false;
				for (unsigned int k = 0; k < chan->mNumRotationKeys; k++) {
					const aiQuatKey& key = chan->mRotationKeys[k];
					if (FLT_NEQ((float)key.mValue.x, rot.x))
						xch = true;
					if (FLT_NEQ((float)key.mValue.y, rot.y))
						ych = true;
					if (FLT_NEQ((float)key.mValue.z, rot.z))
						zch = true;
					if (FLT_NEQ((float)key.mValue.w, rot.w))
						wch = true;
				}
				bool found_keyframe;
				for (size_t frame = 0; frame < anim->mDuration; frame++)
				{
					double timestamp = (double)frame;
					found_keyframe = false;
					for (size_t keyframe = 0; keyframe < chan->mNumRotationKeys - 1; keyframe++)
					{
						const aiQuatKey& key = chan->mRotationKeys[keyframe];
						double current_time = key.mTime;
						const aiQuaternion &cv = key.mValue;

						const aiQuatKey& next_key = chan->mRotationKeys[keyframe + 1];
						double next_time = next_key.mTime;
						const aiQuaternion &nv = next_key.mValue;

						if (current_time <= timestamp && next_time > timestamp) {
							double delta_time = next_time - current_time;


							aiQuaternion slerp_val;
							aiQuaternion::Interpolate(slerp_val, cv, nv, (timestamp - current_time) / delta_time);
							// Do Slerp for quaternion
							double x = slerp_val.x;
							double y = slerp_val.y;
							double z = slerp_val.z;
							double w = slerp_val.w;
							
							xkeys[frame] = (float)x;
							ykeys[frame] = (float)-y; // flip y
							zkeys[frame] = (float)z;
							wkeys[frame] = (float)-w; // flip rot
							found_keyframe = true;
							break;
						}
						else if (current_time > timestamp && next_time > timestamp) {
							// the first keyframe is some time in the future
							// use a constant repeat, until we reach to that time
							xkeys[frame] = (float)cv.x;
							ykeys[frame] = (float)-cv.y; // flip y
							zkeys[frame] = (float)cv.z;
							wkeys[frame] = (float)-cv.w; // flip rot
							found_keyframe = true;
							break;
						}
					}
					if (!found_keyframe) {
						// the frame is far past the last keyframe
						// repeat the last keyframe
						const aiQuatKey& last_key = chan->mRotationKeys[chan->mNumRotationKeys - 1];
						xkeys[frame] = (float)last_key.mValue.x;
						ykeys[frame] = (float)-last_key.mValue.y; // flip y
						zkeys[frame] = (float)last_key.mValue.z;
						wkeys[frame] = (float)-last_key.mValue.w; // flip rot
					}
				}
				// only add non-static channels
				if (xch) {
					json jchan;
					jchan["name"] = chan->mNodeName.C_Str();
					jchan["type"] = "part-qrot-x";
					jchan["pre-state"] = AnimBehaviourToString(chan->mPreState);
					jchan["post-state"] = AnimBehaviourToString(chan->mPostState);
					jchan["values"] = base64_encodeZ((uint8_t*)xkeys.data(),
						(unsigned int)(sizeof(float) * xkeys.size()));
					snprintf(temp, sizeof(temp), "%d", jchanCount++);
					jchannels[temp] = jchan;
				}
				if (ych) {
					json jchan;
					jchan["name"] = chan->mNodeName.C_Str();
					jchan["type"] = "part-qrot-y";
					jchan["pre-state"] = AnimBehaviourToString(chan->mPreState);
					jchan["post-state"] = AnimBehaviourToString(chan->mPostState);
					jchan["values"] = base64_encodeZ((uint8_t*)ykeys.data(),
						(unsigned int)(sizeof(float) * ykeys.size()));
					snprintf(temp, sizeof(temp), "%d", jchanCount++);
					jchannels[temp] = jchan;
				}
				if (zch) {
					json jchan;
					jchan["name"] = chan->mNodeName.C_Str();
					jchan["type"] = "part-qrot-z";
					jchan["pre-state"] = AnimBehaviourToString(chan->mPreState);
					jchan["post-state"] = AnimBehaviourToString(chan->mPostState);
					jchan["values"] = base64_encodeZ((uint8_t*)zkeys.data(),
						(unsigned int)(sizeof(float) * zkeys.size()));
					snprintf(temp, sizeof(temp), "%d", jchanCount++);
					jchannels[temp] = jchan;
				}
				if (wch) {
					json jchan;
					jchan["name"] = chan->mNodeName.C_Str();
					jchan["type"] = "part-qrot-w";
					jchan["pre-state"] = AnimBehaviourToString(chan->mPreState);
					jchan["post-state"] = AnimBehaviourToString(chan->mPostState);
					jchan["values"] = base64_encodeZ((uint8_t*)wkeys.data(),
						(unsigned int)(sizeof(float) * wkeys.size()));
					snprintf(temp, sizeof(temp), "%d", jchanCount++);
					jchannels[temp] = jchan;
				}
			}
			if (chan->mNumScalingKeys > 0 && !forMorph) {
				// get the key values
				std::vector<float> xkeys((int)std::ceil(anim->mDuration));
				std::vector<float> ykeys((int)std::ceil(anim->mDuration));
				std::vector<float> zkeys((int)std::ceil(anim->mDuration));
				bool xch, ych, zch;
				xch = ych = zch = false;
				for (unsigned int k = 0; k < chan->mNumScalingKeys; k++) {
					const aiVectorKey& key = chan->mScalingKeys[k];
					if (FLT_NEQ((float)key.mValue.x, scl.x))
						xch = true;
					if (FLT_NEQ((float)key.mValue.y, scl.y))
						ych = true;
					if (FLT_NEQ((float)key.mValue.z, scl.z))
						zch = true;
				}
				bool found_keyframe;
				for (size_t frame = 0; frame < anim->mDuration; frame++)
				{
					double timestamp = (double)frame;
					found_keyframe = false;
					for (size_t keyframe = 0; keyframe < chan->mNumScalingKeys - 1; keyframe++)
					{
						const aiVectorKey& key = chan->mScalingKeys[keyframe];
						double current_time = key.mTime;
						const aiVector3D &cv = key.mValue;

						const aiVectorKey& next_key = chan->mScalingKeys[keyframe + 1];
						double next_time = next_key.mTime;
						const aiVector3D &nv = next_key.mValue;

						if (current_time <= timestamp && next_time > timestamp) {
							double delta_time = next_time - current_time;

							// For now do linear interpolation
							// In general case, we'll probably need bezier/spline curve evaluation
							double x = (nv.x - cv.x) / delta_time * (timestamp - current_time) + cv.x;
							double y = (nv.y - cv.y) / delta_time * (timestamp - current_time) + cv.y;
							double z = (nv.z - cv.z) / delta_time * (timestamp - current_time) + cv.z;

							xkeys[frame] = (float)x;
							ykeys[frame] = (float)y;
							zkeys[frame] = (float)z;
							found_keyframe = true;
							break;
						}
						else if (current_time > timestamp && next_time > timestamp) {
							// the first keyframe is some time in the future
							// use a constant repeat, until we reach to that time
							xkeys[frame] = (float)cv.x;
							ykeys[frame] = (float)cv.y;
							zkeys[frame] = (float)cv.z;
							found_keyframe = true;
							break;
						}
					}
					if (!found_keyframe) {
						// the frame is far past the last keyframe
						// repeat the last keyframe
						const aiVectorKey& last_key = chan->mScalingKeys[chan->mNumScalingKeys - 1];
						xkeys[frame] = (float)last_key.mValue.x;
						ykeys[frame] = (float)last_key.mValue.y;
						zkeys[frame] = (float)last_key.mValue.z;
					}
				}
				// only add non-static channels
				if (xch) {
					json jchan;
					jchan["name"] = chan->mNodeName.C_Str();
					jchan["type"] = "part-scl-x";
					jchan["pre-state"] = AnimBehaviourToString(chan->mPreState);
					jchan["post-state"] = AnimBehaviourToString(chan->mPostState);
					jchan["values"] = base64_encodeZ((uint8_t*)xkeys.data(),
						(unsigned int)(sizeof(float) * xkeys.size()));
					snprintf(temp, sizeof(temp), "%d", jchanCount++);
					jchannels[temp] = jchan;
				}
				if (ych) {
					json jchan;
					jchan["name"] = chan->mNodeName.C_Str();
					jchan["type"] = "part-scl-y";
					jchan["pre-state"] = AnimBehaviourToString(chan->mPreState);
					jchan["post-state"] = AnimBehaviourToString(chan->mPostState);
					jchan["values"] = base64_encodeZ((uint8_t*)ykeys.data(),
						(unsigned int)(sizeof(float) * ykeys.size()));
					snprintf(temp, sizeof(temp), "%d", jchanCount++);
					jchannels[temp] = jchan;
				}
				if (zch) {
					json jchan;
					jchan["name"] = chan->mNodeName.C_Str();
					jchan["type"] = "part-scl-z";
					jchan["pre-state"] = AnimBehaviourToString(chan->mPreState);
					jchan["post-state"] = AnimBehaviourToString(chan->mPostState);
					jchan["values"] = base64_encodeZ((uint8_t*)zkeys.data(),
						(unsigned int)(sizeof(float) * zkeys.size()));
					snprintf(temp, sizeof(temp), "%d", jchanCount++);
					jchannels[temp] = jchan;
				}
			}
		}

		janim["channels"] = jchannels;
		rez[anim->mName.C_Str()] = janim;
	}
}


