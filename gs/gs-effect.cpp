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

#include "gs-effect.h"

GS::Effect::Effect(std::string file, Cache *cache):m_cache(cache) {
	m_name = file;
	obs_enter_graphics();
	m_effect = nullptr;
	if (m_cache != nullptr)
		m_cache->load(CacheableType::Effect, m_name, (void **)&m_effect);
	if (m_effect == nullptr)
	{
		char* errorMessage = nullptr;
		m_effect = gs_effect_create_from_file(m_name.c_str(), &errorMessage);
		if (!m_effect || errorMessage) {
			std::string error(errorMessage);
			bfree((void*)errorMessage);
			obs_leave_graphics();
			throw std::runtime_error(error);
		}
		if (!m_cache->add(CacheableType::Effect, m_name, (void *)m_effect))
			blog(LOG_WARNING, "Caching effect failed: %s", m_name.c_str());
	}
	obs_leave_graphics();
}

GS::Effect::Effect(std::string code, std::string name, Cache *cache):m_cache(cache) {
	m_name = name;
	obs_enter_graphics();
	m_effect = nullptr;
	if (m_cache != nullptr)
		m_cache->load(CacheableType::Effect, m_name, (void **)&m_effect);
	if (m_effect == nullptr)
	{
		char* errorMessage = nullptr;
		m_effect = gs_effect_create(code.c_str(), m_name.c_str(), &errorMessage);
		if (!m_effect || errorMessage) {
			std::string error(errorMessage);
			bfree((void*)errorMessage);
			obs_leave_graphics();
			throw std::runtime_error(error);
		}
		if(!m_cache->add(CacheableType::Effect, m_name, (void *)m_effect))
			blog(LOG_WARNING, "Caching effect failed: %s", m_name.c_str());
	}
	obs_leave_graphics();
}

GS::Effect::~Effect() {
	m_cache->try_destroy_resource(m_name, m_effect,
		CacheableType::Effect);
}

gs_effect_t* GS::Effect::GetObject() {
	return m_effect;
}

std::vector<GS::EffectParameter> GS::Effect::GetParameters() {
	size_t num = gs_effect_get_num_params(m_effect);
	std::vector<GS::EffectParameter> ps;
	ps.reserve(num);
	for (size_t idx = 0; idx < num; idx++) {
		ps.emplace_back(EffectParameter(gs_effect_get_param_by_idx(m_effect, idx)));
	}
	return ps;
}

GS::EffectParameter GS::Effect::GetParameterByName(std::string name) {
	gs_eparam_t* param = gs_effect_get_param_by_name(m_effect, name.c_str());
	if (!param)
		throw std::invalid_argument("parameter with name not found");
	return EffectParameter(param);
}

GS::EffectParameter::EffectParameter(gs_eparam_t* param) {
	if (!param)
		throw std::invalid_argument("param is null");

	m_param = param;
	gs_effect_get_param_info(m_param, &m_paramInfo);
}

GS::EffectParameter::Type GS::EffectParameter::GetType() {
	switch (m_paramInfo.type) {
		case GS_SHADER_PARAM_BOOL:
			return Type::Boolean;
		case GS_SHADER_PARAM_FLOAT:
			return Type::Float;
		case GS_SHADER_PARAM_VEC2:
			return Type::Float2;
		case GS_SHADER_PARAM_VEC3:
			return Type::Float3;
		case GS_SHADER_PARAM_VEC4:
			return Type::Float4;
		case GS_SHADER_PARAM_INT:
			return Type::Integer;
		case GS_SHADER_PARAM_INT2:
			return Type::Integer2;
		case GS_SHADER_PARAM_INT3:
			return Type::Integer3;
		case GS_SHADER_PARAM_INT4:
			return Type::Integer4;
		case GS_SHADER_PARAM_MATRIX4X4:
			return Type::Matrix;
		case GS_SHADER_PARAM_TEXTURE:
			return Type::Texture;
		//case GS_SHADER_PARAM_STRING:
		//	return Type::String;
		default:
		case GS_SHADER_PARAM_UNKNOWN:
			return Type::Unknown;
	}
}

void GS::EffectParameter::SetBoolean(bool v) {
	if (GetType() != Type::Boolean)
		throw std::bad_cast();
	gs_effect_set_bool(m_param, v);
}

void GS::EffectParameter::SetBooleanArray(bool v[], size_t sz) {
	if (GetType() != Type::Boolean)
		throw std::bad_cast();
	gs_effect_set_val(m_param, v, sz);
}

void GS::EffectParameter::SetFloat(float_t x) {
	if (GetType() != Type::Float)
		throw std::bad_cast();
	gs_effect_set_float(m_param, x);
}

void GS::EffectParameter::SetFloat2(vec2& v) {
	if (GetType() != Type::Float2)
		throw std::bad_cast();
	gs_effect_set_vec2(m_param, &v);
}

void GS::EffectParameter::SetFloat2(float_t x, float_t y) {
	if (GetType() != Type::Float2)
		throw std::bad_cast();
	vec2 v = { x, y };
	gs_effect_set_vec2(m_param, &v);
}

void GS::EffectParameter::SetFloat3(vec3& v) {
	if (GetType() != Type::Float3)
		throw std::bad_cast();
	gs_effect_set_vec3(m_param, &v);
}

void GS::EffectParameter::SetFloat3(float_t x, float_t y, float_t z) {
	if (GetType() != Type::Float3)
		throw std::bad_cast();
	vec3 v = { x, y, z };
	gs_effect_set_vec3(m_param, &v);
}

void GS::EffectParameter::SetFloat4(vec4& v) {
	if (GetType() != Type::Float4)
		throw std::bad_cast();
	gs_effect_set_vec4(m_param, &v);
}

void GS::EffectParameter::SetFloat4(float_t x, float_t y, float_t z, float_t w) {
	if (GetType() != Type::Float4)
		throw std::bad_cast();
	vec4 v = { x, y, z, w };
	gs_effect_set_vec4(m_param, &v);
}

void GS::EffectParameter::SetFloatArray(float_t v[], size_t sz) {
	// this could be Float2/Float3/Float4
	//if (GetType() != Type::Float)
	//	throw std::bad_cast();
	gs_effect_set_val(m_param, v, sz);
}

void GS::EffectParameter::SetInteger(int32_t x) {
	if (GetType() != Type::Integer)
		throw std::bad_cast();
	gs_effect_set_int(m_param, x);
}

void GS::EffectParameter::SetInteger2(int32_t x, int32_t y) {
	if (GetType() != Type::Integer2)
		throw std::bad_cast();
	int32_t v[] = { x, y };
	gs_effect_set_val(m_param, v, 2);
}

void GS::EffectParameter::SetInteger3(int32_t x, int32_t y, int32_t z) {
	if (GetType() != Type::Integer3)
		throw std::bad_cast();
	int32_t v[] = { x, y, z };
	gs_effect_set_val(m_param, v, 3);
}

void GS::EffectParameter::SetInteger4(int32_t x, int32_t y, int32_t z, int32_t w) {
	if (GetType() != Type::Integer4)
		throw std::bad_cast();
	int32_t v[] = { x, y, z, w };
	gs_effect_set_val(m_param, v, 4);
}

void GS::EffectParameter::SetIntegerArray(int32_t v[], size_t sz) {
	if (GetType() != Type::Integer)
		throw std::bad_cast();
	gs_effect_set_val(m_param, v, sz);
}

void GS::EffectParameter::SetMatrix(matrix4& v) {
	if (GetType() != Type::Matrix)
		throw std::bad_cast();
	gs_effect_set_matrix4(m_param, &v);
}

void GS::EffectParameter::SetTexture(std::shared_ptr<GS::Texture> v) {
	if (GetType() != Type::Texture)
		throw std::bad_cast();
	gs_effect_set_texture(m_param, v->GetObject());
}

void  GS::EffectParameter::SetSampler(gs_sampler_state* ss) {
	if (GetType() != Type::Texture)
		throw std::bad_cast();
	gs_effect_set_next_sampler(m_param, ss);
}
