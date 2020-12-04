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
#include "mask.h"
#include "mask-resource.h"
#include "gs/gs-effect.h"

extern "C" {
#pragma warning( push )
#pragma warning( disable: 4201 )
#include <libobs/util/platform.h>
#include <libobs/obs-module.h>
#pragma warning( pop ) 
}

namespace Mask {
	namespace Resource {

		enum LightType : uint32_t {
			AMBIENT = 0,
			DIRECTIONAL = 1, 
			POINT = 2,
			SPOT = 3,
			AREA = 4,
			UNDEFINED = 666,
		};

		struct LightInstanceData : public InstanceData {
			LightType	lightType;
			vec3		position;
			vec3		direction;
			vec3		up;
			float		att0;
			float		att1;
			float		att2;
			vec3		ambient;
			vec3		diffuse;
			vec3		specular;
			float		innerAngle;
			float		outerAngle;
			vec2		areaSize;

			LightInstanceData() : lightType(LightType::UNDEFINED) {}
			LightInstanceData(const LightInstanceData& other) { *this = other; }
			LightInstanceData& operator=(const LightInstanceData& other);
		};

		class Light : public IBase {
		public:

			Light(Mask::MaskData* parent, std::string name, obs_data_t* data);
			virtual ~Light();

			virtual Type GetType() override;

			virtual void Update(Mask::Part* part, float time) override;
			virtual void Render(Mask::Part* part) override;

		private:
			const char* const S_LIGHTTYPE = "light-type";
			const char* const S_POSITION = "position";
			const char* const S_DIRECTION = "direction";
			const char* const S_UP = "up";
			const char* const S_ATT0 = "att0";
			const char* const S_ATT1 = "att1";
			const char* const S_ATT2 = "att2";
			const char* const S_AMBIENT = "ambient";
			const char* const S_DIFFUSE = "diffuse";
			const char* const S_SPECULAR = "specular";
			const char* const S_INNERANGLE = "inner-angle";
			const char* const S_OUTERANGLE = "outer-angle";
			const char* const S_AREASIZE = "area-size";

		protected:
			LightInstanceData m_idat;

			LightType GetLightType(obs_data_t* data);
		};
	}
}
