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

#include "mask-resource-emitter.h"
#include "plugin/exceptions.h"
#include "plugin/plugin.h"


Mask::Resource::Emitter::Emitter(Mask::MaskData* parent, std::string name, obs_data_t* data)
	: IBase(parent, name) {

	// Model
	if (!obs_data_has_user_value(data, S_MODEL)) {
		PLOG_ERROR("Emitter '%s' has no model.", name.c_str());
		throw std::logic_error("Emitter has no model.");
	}
	std::string modelName = obs_data_get_string(data, S_MODEL);
	m_model = std::dynamic_pointer_cast<Model>(m_parent->GetResource(modelName));
	if (m_model == nullptr) {
		PLOG_ERROR("<Emitter '%s'> Dependency on model '%s' could not be resolved.",
			m_name.c_str(), modelName.c_str());
		throw std::logic_error("Emitter depends on non-existing model.");
	}
	if (m_model->GetType() != Type::Model) {
		PLOG_ERROR("<Emitter '%s'> Resolved model dependency on '%s' is not a model.",
			m_name.c_str(), modelName.c_str());
		throw std::logic_error("Model dependency of Emitter is not a model.");
	}

	// Lifetime
	if (!obs_data_has_user_value(data, S_LIFETIME)) {
		PLOG_ERROR("Emitter '%s' has no lifetime value.", name.c_str());
		throw std::logic_error("Emitter has no lifetime value.");
	}
	m_lifetime = (float)obs_data_get_double(data, S_LIFETIME);

	// Rate
	m_rateMin = m_rateMax = 4.0f;
	if (obs_data_has_user_value(data, S_RATE)) {
		m_rateMin = m_rateMax = (float)obs_data_get_double(data, S_RATE);
	}
	if (obs_data_has_user_value(data, S_RATE_MIN)) {
		m_rateMin = (float)obs_data_get_double(data, S_RATE_MIN);
	}
	if (obs_data_has_user_value(data, S_RATE_MAX)) {
		m_rateMax = (float)obs_data_get_double(data, S_RATE_MAX);
	}
	if (m_rateMin > m_rateMax) {
		float t = m_rateMin;
		m_rateMin = m_rateMax;
		m_rateMax = t;
	}

	// Friction
	m_frictionMin = m_frictionMax = 1.0f;
	if (obs_data_has_user_value(data, S_FRICTION)) {
		m_frictionMin = m_frictionMax = (float)obs_data_get_double(data, S_FRICTION);
	}
	if (obs_data_has_user_value(data, S_FRICTION_MIN)) {
		m_frictionMin = (float)obs_data_get_double(data, S_FRICTION_MIN);
	}
	if (obs_data_has_user_value(data, S_FRICTION_MAX)) {
		m_frictionMax = (float)obs_data_get_double(data, S_FRICTION_MAX);
	}
	if (m_frictionMin > m_rateMax) {
		float t = m_frictionMin;
		m_frictionMin = m_frictionMax;
		m_frictionMax = t;
	}

	// Force
	vec3_zero(&m_forceMin);
	vec3_zero(&m_forceMax);
	if (obs_data_has_user_value(data, S_FORCE)) {
		obs_data_get_vec3(data, S_FORCE, &m_forceMin);
		vec3_copy(&m_forceMax, &m_forceMin);
	}
	if (obs_data_has_user_value(data, S_FORCE_MIN)) {
		obs_data_get_vec3(data, S_FORCE_MIN, &m_forceMin);
	}
	if (obs_data_has_user_value(data, S_FORCE_MAX)) {
		obs_data_get_vec3(data, S_FORCE_MAX, &m_forceMax);
	}
	if (m_forceMin.x > m_forceMax.x) {
		float t = m_forceMin.x;
		m_forceMin.x = m_forceMax.x;
		m_forceMax.x = t;
	}
	if (m_forceMin.y > m_forceMax.y) {
		float t = m_forceMin.y;
		m_forceMin.y = m_forceMax.y;
		m_forceMax.y = t;
	}
	if (m_forceMin.z > m_forceMax.z) {
		float t = m_forceMin.z;
		m_forceMin.z = m_forceMax.z;
		m_forceMax.z = t;
	}

	// Initial Velocity
	vec3_zero(&m_initialVelocityMin);
	vec3_zero(&m_initialVelocityMax);
	if (obs_data_has_user_value(data, S_INITIAL_VELOCITY)) {
		obs_data_get_vec3(data, S_INITIAL_VELOCITY, &m_initialVelocityMin);
		vec3_copy(&m_initialVelocityMax, &m_initialVelocityMin);
	}
	if (obs_data_has_user_value(data, S_INITIAL_VELOCITY_MIN)) {
		obs_data_get_vec3(data, S_INITIAL_VELOCITY_MIN, &m_initialVelocityMin);
	}
	if (obs_data_has_user_value(data, S_INITIAL_VELOCITY_MAX)) {
		obs_data_get_vec3(data, S_INITIAL_VELOCITY_MAX, &m_initialVelocityMax);
	}
	if (m_initialVelocityMin.x > m_initialVelocityMax.x) {
		float t = m_initialVelocityMin.x;
		m_initialVelocityMin.x = m_initialVelocityMax.x;
		m_initialVelocityMax.x = t;
	}
	if (m_initialVelocityMin.y > m_initialVelocityMax.y) {
		float t = m_initialVelocityMin.y;
		m_initialVelocityMin.y = m_initialVelocityMax.y;
		m_initialVelocityMax.y = t;
	}
	if (m_initialVelocityMin.z > m_initialVelocityMax.z) {
		float t = m_initialVelocityMin.z;
		m_initialVelocityMin.z = m_initialVelocityMax.z;
		m_initialVelocityMax.z = t;
	}

	// Scale
	m_scaleStart = m_scaleEnd = 1.0f;
	if (obs_data_has_user_value(data, S_SCALE_START)) {
		m_scaleStart = (float)obs_data_get_double(data, S_SCALE_START);
	}
	if (obs_data_has_user_value(data, S_SCALE_END)) {
		m_scaleEnd = (float)obs_data_get_double(data, S_SCALE_END);
	}

	// Alpha
	m_alphaStart = m_alphaEnd = 1.0f;
	if (obs_data_has_user_value(data, S_ALPHA_START)) {
		m_alphaStart = (float)obs_data_get_double(data, S_ALPHA_START);
	}
	if (obs_data_has_user_value(data, S_ALPHA_END)) {
		m_alphaEnd = (float)obs_data_get_double(data, S_ALPHA_END);
	}

	// Num Particles
	if (!obs_data_has_user_value(data, S_NUM_PARTICLES)) {
		PLOG_ERROR("Emitter '%s' has no num-particles value.", name.c_str());
		throw std::logic_error("Emitter has no num-particles value.");
	}
	m_numParticles = (int)obs_data_get_int(data, S_NUM_PARTICLES);

	// Z sort offset
	m_zSortOffset = 0.0f;
	if (obs_data_has_user_value(data, S_Z_SORT_OFFSET)) {
		m_zSortOffset = (float)obs_data_get_double(data, S_Z_SORT_OFFSET);
	}

	// World Space
	m_worldSpace = true;
	if (obs_data_has_user_value(data, S_WORLD_SPACE)) {
		m_worldSpace = obs_data_get_bool(data, S_WORLD_SPACE);
	}	
	
	// Inverse rate
	m_inverseRate = false;
	if (obs_data_has_user_value(data, S_INVERSE_RATE)) {
		m_inverseRate = obs_data_get_bool(data, S_INVERSE_RATE);
	}

	// Seed rand
	time_t seconds_since_epoch = ::time(0);
	srand((unsigned int)seconds_since_epoch);
}

Mask::Resource::Emitter::~Emitter() {

}

Mask::Resource::Type Mask::Resource::Emitter::GetType() {
	return Mask::Resource::Type::Emitter;
}

float Mask::Resource::Emitter::RandFloat(float min, float max) {
	float a = (float)rand() / (float)RAND_MAX;
	return a * (max - min) + min; 
}


void Mask::Resource::Emitter::Update(Mask::Part* part, float time) {

	m_parent->instanceDatas.Push(m_id);

	// get our instance data
	std::shared_ptr<EmitterInstanceData> instData =
		m_parent->instanceDatas.GetData<EmitterInstanceData>();
	instData->Init(m_numParticles, this);

	// update our model
	Particle* p = instData->particles;
	for (int i = 0; i < m_numParticles; i++, p++) {
		if (p->state == Particle::State::ALIVE) {
			m_parent->instanceDatas.Push(p->id);
			m_model->Update(part, time);
			m_parent->instanceDatas.Pop();
		}
	}

	// use scale to control emission
	bool zeroScale = false;
	if (part->global.x.x < 0.000001f &&
		part->global.y.y < 0.000001f &&
		part->global.z.z < 0.000001f) {
		zeroScale = true;
	}

	// Emit particle?
	if (!zeroScale)
		instData->elapsed += time;
	if (instData->delta_time < instData->elapsed && !zeroScale) {

		int numToEmit = 1;
		if (instData->delta_time > 0.000001f)
			numToEmit = (int)(instData->elapsed / instData->delta_time);
		while (numToEmit-- > 0) {
			// find dead particle
			p = instData->particles;
			int idx = 0;
			for (idx = 0; idx < m_numParticles; idx++, p++) {
				if (p->state == Particle::State::DEAD)
					break;
			}
			if (idx < m_numParticles) {
				// actually spawn a new particle
				instData->elapsed = 0.0f;
				p->elapsed = 0.0f;
				p->state = Particle::State::SPAWNED;

				// we will set up transform when rendering
				vec3_zero(&p->position);
				vec3_set(&p->velocity,
					RandFloat(m_initialVelocityMin.x, m_initialVelocityMax.x) * time,
					RandFloat(m_initialVelocityMin.y, m_initialVelocityMax.y) * time,
					RandFloat(m_initialVelocityMin.z, m_initialVelocityMax.z) * time);
			}
		}

		// random emit rate
		if (m_inverseRate)
			// seconds between particles
			instData->delta_time = RandFloat(m_rateMin, m_rateMax);
		else
			// particles / second
			instData->delta_time = 1.0f / RandFloat(m_rateMin, m_rateMax);
	}

	// Update particles
	p = instData->particles;
	for (int i = 0; i < m_numParticles; i++, p++) {
		if (p->state == Particle::State::ALIVE) {
			p->elapsed += time;
			if (p->elapsed > m_lifetime) {
				// kill particle
				p->state = Particle::State::DEAD;
				continue;
			}

			// velocity
			vec3 v;
			vec3_mulf(&v, &p->velocity, time);
			vec3_add(&p->position, &p->position, &v);
			// friction
			vec3_mulf(&p->velocity, &p->velocity, 
				RandFloat(m_frictionMin, m_frictionMax));
			// force
			vec3 f;
			vec3_set(&f,
				RandFloat(m_forceMin.x, m_forceMax.x),
				RandFloat(m_forceMin.y, m_forceMax.y),
				RandFloat(m_forceMin.z, m_forceMax.z));
			vec3_mulf(&f, &f, time);
			vec3_add(&p->velocity, &p->velocity, &f);
		}
	}

	m_parent->instanceDatas.Pop();
}

void Mask::Resource::Emitter::Render(Mask::Part* part) {

	m_parent->instanceDatas.Push(m_id);

	// get our global matrix
	matrix4 global;
	gs_matrix_get(&global);

	// get our instance data
	std::shared_ptr<EmitterInstanceData> instData =
		m_parent->instanceDatas.GetData<EmitterInstanceData>();
	if (instData->particles == nullptr) {
		m_parent->instanceDatas.Pop();
		return;
	}

	// add particles as sorted draw objects 
	Particle* p = instData->particles;
	for (int i = 0; i < m_numParticles; i++, p++) {
		if (p->state != Particle::State::DEAD) {
			// first time spawned
			if (p->state == Particle::State::SPAWNED) {
				if (m_worldSpace) {
					p->position.x = global.t.x;
					p->position.y = global.t.y;
					p->position.z = global.t.z;
					vec3_transform(&(p->velocity), &(p->velocity), &global);
				}
				p->state = Particle::State::ALIVE;
			}
			p->sortDrawPart = part;
			m_parent->instanceDatas.Push(p->id);
			m_parent->AddSortedDrawObject(p);
			m_parent->instanceDatas.Pop();
		}
	}

	m_parent->instanceDatas.Pop();
}

bool Mask::Resource::Emitter::IsDepthOnly() {
	return false;
}

bool Mask::Resource::Emitter::IsOpaque() {
	return false;
}



float Mask::Resource::Particle::SortDepth() {
	float z = position.z;
	if (!emitter->m_worldSpace) {
		matrix4 m;
		gs_matrix_get(&m);
		z += m.t.z;
	}
	// Z sorting offset. Useful for forcing particles in front of transparent objects.
	z += emitter->m_zSortOffset;

	return z;
}

void Mask::Resource::Particle::SortedRender() {

	// global alpha
	std::shared_ptr<AlphaInstanceData> aid =
		emitter->GetParent()->instanceDatas.GetData<AlphaInstanceData>
		(AlphaInstanceDataId);

	float saved_alpha = aid->alpha;

	matrix4 m;
	gs_matrix_get(&m);

	gs_matrix_push();
	gs_matrix_identity();

	gs_matrix_translate(&position);
	if (!emitter->m_worldSpace) {
		gs_matrix_translate3f(m.t.x, m.t.y, m.t.z);
	}
	gs_matrix_rotaa4f(1.0f, 0.0f, 0.0f, M_PI);
	gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, M_PI);

	float lambda = elapsed / emitter->m_lifetime;
	float s = lambda * (emitter->m_scaleEnd - emitter->m_scaleStart) + emitter->m_scaleStart;
	gs_matrix_scale3f(s, s, s);
	aid->alpha = lambda * (emitter->m_alphaEnd - emitter->m_alphaStart) + emitter->m_alphaStart;
	aid->alpha *= saved_alpha;

	emitter->m_model->DirectRender(sortDrawPart);

	gs_matrix_pop();
	aid->alpha = saved_alpha;
}
