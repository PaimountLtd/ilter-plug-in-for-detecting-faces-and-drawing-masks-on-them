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
#include <inttypes.h>
#include <string>
#include <map>
#include "plugin/exceptions.h"
#include "mask/mask-resource.h"
#include <sys/stat.h>
#include <fstream>
extern "C" {
	#pragma warning( push )
	#pragma warning( disable: 4201 )
	#include <libobs/util/platform.h>
	#include <libobs/obs.h>
	#include <libobs/graphics/graphics.h>
	#pragma warning( pop )
}

namespace GS {
	class Texture {
		public:
		enum Type : uint8_t {
			Normal,
			Volume,
			Cube
		};

		enum Flags : uint32_t {
			Dynamic,
			BuildMipMaps,
		};

		using Cache = Mask::Resource::Cache;
		using CacheableType = Cache::CacheableType;

		public:
		/*!
		 * \brief Create a new texture from data
		 *
		 *
		 *
		 * \param width
		 * \param height
		 * \param format
		 * \param mip_levels
		 * \param mip_data
		 * \param flags
		 */
		Texture(uint32_t width, uint32_t height, gs_color_format format, 
			uint32_t mip_levels, const uint8_t **mip_data, uint32_t flags,
			Cache *cache);

		/*!
		 * \brief Create a new volume texture from data
		 *
		 *
		 *
		 * \param width
		 * \param height
		 * \param depth
		 * \param format
		 * \param mip_levels
		 * \param mip_data
		 * \param flags
		 */
		Texture(uint32_t width, uint32_t height, uint32_t depth, 
			gs_color_format format, uint32_t mip_levels, 
			const uint8_t **mip_data, uint32_t flags,
			Cache *cache);

		/*!
		 * \brief Create a new cube texture from data
		 *
		 *
		 * \param name
		 * \param size
		 * \param format
		 * \param mip_levels
		 * \param mip_data
		 * \param flags
		 */
		Texture(std::string name,uint32_t size, gs_color_format format, 
			uint32_t mip_levels, const uint8_t **mip_data,
			uint32_t flags,
			Cache *cache);

		/*!
		* \brief Load a texture from a file
		*
		* Creates a new #GS::Texture from a file located on disk. If the
		* file can not be found, accessed or read, a #Plugin::file_not_found_error
		* will be thrown. If there is an error reading the file, a
		* #Plugin::io_error will be thrown.
		*
		* \param file File to create the texture from.
		*/
		Texture(std::string file, Cache *cache);

		/*!
		* \brief Default constructor
		*/
		Texture() 
			: m_texture(nullptr), m_destroy(true), m_cache(nullptr) {}

		/*!
		* \brief Wrapper constructur
		*/
		Texture(gs_texture* tex, bool destroy=false) 
			: m_texture(tex), m_destroy(destroy), m_cache(nullptr) {}

		/*!
		 * \brief Destructor
		 *
		 *
		 *
		 * \return
		 */
		virtual ~Texture();

		/*!
		 * \brief
		 *
		 *
		 *
		 * \param unit
		 * \return void
		 */
		void Load(int unit);

		/*!
		 * \brief
		 *
		 *
		 *
		 * \return gs_texture_t*
		 */
		gs_texture_t* GetObject();

		protected:
		gs_texture_t* m_texture;
		bool m_destroy;
		std::string m_name;

		// cache manager from FM instance
		// currently only caching cubemaps
		Cache *m_cache;

	};
}
