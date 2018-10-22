/*
* Face Masks for SlOBS
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

#pragma once
#include "mask-resource.h"
#include "gs/gs-vertexbuffer.h"
#include "mask-resource-mesh.h"
#include "mask-resource-material.h"
#include <map>
#include <string>

namespace Mask {
	namespace Resource {

		static const int MAX_BONES_PER_SKIN = 8;

		struct BonesList {
			int											numBones;
			std::array<matrix4*, MAX_BONES_PER_SKIN>	bones;
		};

		class SkinnedModel : public IBase, public SortedDrawObject {
		public:
			SkinnedModel(Mask::MaskData* parent, std::string name, obs_data_t* data);
			virtual ~SkinnedModel();

			virtual Type GetType() override;
			virtual void Update(Mask::Part* part, float time) override;
			virtual void Render(Mask::Part* part) override;
			virtual bool IsDepthOnly() override;
			virtual bool IsStatic() override;
			virtual bool IsRotationDisabled() override;

			virtual float	SortDepth() override;
			virtual void	SortedRender() override;

			void DirectRender(Mask::Part* part);

			bool IsOpaque();

			std::shared_ptr<Material> GetMaterial() {
				return m_material;
			}

		protected:

			struct Bone {
				std::shared_ptr<Part>	part;
				matrix4					offset;
				matrix4					global;
				//Bone& operator=(const Bone& b) {
				//	part = b.part;
				//	matrix4_copy(&offset, &(b.offset));
				//	matrix4_copy(&global, &(b.global));
				//	return *this;
				//}
			};
			struct Skin {
				std::shared_ptr<Mesh>	mesh;
				std::vector<int>		bones;
			};

			std::vector<Bone>			m_bones;
			std::vector<Skin>			m_skins;
			std::shared_ptr<Material>	m_material;

			std::array<size_t, 8>		m_boneIds;
		};
	}
}
