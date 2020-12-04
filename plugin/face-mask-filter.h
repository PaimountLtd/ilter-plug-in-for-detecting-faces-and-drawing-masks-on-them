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
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>

#include "smll/FaceDetector.hpp"
#include "smll/OBSRenderer.hpp"
#include "smll/DetectionResults.hpp"
#include "smll/TriangulationResult.hpp"
#include "smll/MorphData.hpp"


#include "mask/mask.h"
#include "mask/mask-resource.h"

extern "C" {
#pragma warning( push )
#pragma warning( disable: 4201 )
#include <libobs/obs-source.h>
#include <libobs/graphics/matrix4.h>
#pragma warning( pop )
#define private private_var_name
#include <libobs/obs-internal.h>
#undef private
}

#define SSAA_UPSAMPLE_FACTOR 2

using namespace std;

enum convert_type {
	CONVERT_NONE,
	CONVERT_NV12,
	CONVERT_420,
	CONVERT_422_U,
	CONVERT_422_Y,
};

namespace Plugin {
	class FaceMaskFilter {
	public:
		FaceMaskFilter();
		~FaceMaskFilter();

		static const char *get_name(void *);
		static void *create(obs_data_t *, obs_source_t *);
		static void destroy(void *);

	private:
		obs_source_info filter;

		static const int NO_ANTI_ALIASING = 0;
		static const int SSAA_ANTI_ALIASING = 1;
		static const int FXAA_ANTI_ALIASING = 2;

		class Instance {
		public:
			Instance(obs_data_t *, obs_source_t *);
			~Instance();

			// OBS API
			static uint32_t get_width(void *);
			uint32_t get_width();
			static uint32_t get_height(void *);
			uint32_t get_height();
			static void get_defaults(obs_data_t *);
			static obs_properties_t *get_properties(void *);
			void get_properties(obs_properties_t *pr);
			static void update(void *, obs_data_t *);
			void update(obs_data_t *);
			static void activate(void *);
			void activate();
			static void deactivate(void *);
			void deactivate();
			static void show(void *);
			void show();
			static void hide(void *);
			void hide();
			static void video_tick(void *, float);
			void video_tick(float);
			static void video_render(void *, gs_effect_t *);
			void video_render(gs_effect_t *);
			// callbacks
			static bool generate_videos(obs_properties_t *pr, obs_property_t *p, void *data);
			bool generate_videos(obs_properties_t *pr, obs_property_t *p);
			cv::Mat convert_frame_to_gray_mat(obs_source_frame* frame);

			// resource cache manager
			using Cache = Mask::Resource::Cache;
			using CacheableType = Cache::CacheableType;
			Cache m_cache;
			bool caching_done;
			
		protected:
			// face detection thread
			static int32_t StaticThreadMain(Instance*);
			int32_t LocalThreadMain();

			bool SendSourceTextureToThread(gs_texture* sourceTexture);

			// mask data loading thread
			static int32_t StaticMaskDataThreadMain(Instance*);
			int32_t LocalMaskDataThreadMain();

			// misc functions
			Mask::MaskData*	LoadMask(std::string filename);
			void LoadDemo();
			void drawCropRects(int width, int height);
			void drawMotionRects(int width, int height);
			void updateFaces();
			void setupRenderingState();
			gs_texture* RenderSourceTexture(gs_effect_t* effect);
			void clearFramesActiveStatus();

		private:
			// Filter State
			obs_source_t*	source;
			gs_rect			sourceViewport;
			int32_t			canvasWidth, canvasHeight;
			int32_t			baseWidth, baseHeight;
			bool			isActive;
			bool			isVisible;
			bool			videoTicked;
			HANDLE			taskHandle;
			ofstream		logOutput;
			// Face detector
			smll::FaceDetector*		smllFaceDetector;
#if !defined(PUBLIC_RELEASE)
			smll::OBSRenderer*      smllRenderer;
#endif
			gs_effect_t*		antialiasing_effect = nullptr;
			int					m_scale_rate = 1;
			int					antialiasing_method = NO_ANTI_ALIASING;

			// Texture rendering & staging
			gs_texrender_t*		sourceRenderTarget;
			gs_texrender_t*		drawTexRender;
			gs_texrender_t*		alertTexRender;

			// Texture/Target to store lighting info from input stream
			gs_effect_t*		color_grading_filter_effect = nullptr;
			gs_texrender_t*		vidLightTexRender;
			gs_texrender_t*		vidLightTexRenderBack;
			gs_texture_t*		vidLightTex;

			// mask filenames
			std::string			maskFolder;
			std::string			currentMaskFolder;
			std::string			maskFilename;
			std::string			maskFilePath;
			std::string			maskInternal;
			std::string			currentMaskFilename;

			void	checkForMaskUnloading();

			// alert params
			bool				alertActivate;
			float				alertDuration;
			float				alertShowDelay;

			// mask data loading thread
			std::thread			maskDataThread;
			std::mutex			maskDataMutex;
			std::unique_ptr<Mask::MaskData>	maskData;

			bool				loading_mask;
			std::mutex          passFrameToDetection;
			std::mutex          loadMaskDetectionMutex;
			// lock-free atomic flag
			// 1. for signaling to threads to finish their work
			std::atomic_flag mask_load_thread_running = ATOMIC_FLAG_INIT;
			std::atomic_flag detection_thread_running = ATOMIC_FLAG_INIT;
			// 2. for the threads to signal back they're ready to be joined
			std::atomic_flag mask_load_thread_destructing = ATOMIC_FLAG_INIT;
			std::atomic_flag detection_thread_destructing = ATOMIC_FLAG_INIT;

			// alert location
			enum AlertLocation {
				LEFT_BOTTOM,
				LEFT_TOP,
				RIGHT_BOTTOM,
				RIGHT_TOP,

				NUM_ALERT_LOCATIONS
			};

			// alert data
			float				alertElapsedTime;
			bool				alertTriggered;
			bool				alertShown;
			bool				alertsLoaded;

			//test mode
			bool				testMode;
			// demo mode
			std::string			demoModeFolder;
			int					demoCurrentMask;
			bool				demoModeInDelay;
			bool				demoModeGenPreviews;
			bool				demoModeRecord;
			bool				recordTriggered;
			bool				demoModeSavingFrames;
			std::string			beforeText;
			std::string			beforeFile;
			std::string			afterText;
			std::string			afterFile;

			std::vector<std::unique_ptr<Mask::MaskData>>	demoMaskDatas;
			std::vector<std::string> demoMaskFilenames;

			void demoModeRender(gs_texture* vidTex, 
				gs_texture* maskTex, Mask::MaskData* mask_data);

			// For writing thumbnails
			struct PreviewFrame {
				gs_texture_t*	vidtex;

				PreviewFrame(gs_texture_t* v, int w, int h);
				PreviewFrame(const PreviewFrame& other);
				PreviewFrame& operator=(const PreviewFrame& other);
				~PreviewFrame();
			};
			std::vector<PreviewFrame>	previewFrames;
			void WritePreviewFrames();

			// our current face detection results
			smll::DetectionResults		faces;
			smll::TriangulationResult	triangulation;
			TimeStamp					timestamp;
			bool						timestampInited;
			bool						lastTimestampInited;
			TimeStamp					lastActualTimestamp;
			TimeStamp					renderTimestamp;
			smll::ProcessedResults		processedFrameResults;
			int lastResultIndex;
			bool sameFrameResults;

			// flags
			bool				drawMask;
			bool				drawAlert;
			bool				drawFaces;
			bool				drawMorphTris;
			bool				drawFDRect;
			bool				drawMotionRect;
			bool				filterPreviewMode;
			bool				autoBGRemoval;
			bool				cartoonMode;
			bool				logMode;
			bool				lastLogMode;
			// for testing/thumbs/writing textures to files
			gs_stagesurf_t*		testingStage;

			// Detection
			struct ThreadData {

				static const int BUFFER_SIZE = 8;

				std::thread thread;
				std::mutex mutex;

				// frames circular buffer (video_render()'s thread -> detection thread)
				struct Frame {
					smll::MorphData     morphData;
					std::mutex			mutex;
					TimeStamp			timestamp;
					int					resizeWidth;
					int					resizeHeight;
					bool				active;
					cv::Mat             grayImage;
					smll::OBSTexture	capture;
				};
				Frame frame;

				// faces circular buffer (detection thread -> video_tick()'s thread)
				struct CachedResult {
					smll::DetectionResults		detectionResults;
					smll::TriangulationResult	triangulationResults;
					std::mutex					mutex;
					TimeStamp					timestamp;
				};
				int facesIndex;
				std::array<struct CachedResult, BUFFER_SIZE> faces;

			} detection;

		};
	};
}
