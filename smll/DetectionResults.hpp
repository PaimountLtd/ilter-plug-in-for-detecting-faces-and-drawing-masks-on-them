/*
* Face Masks for SlOBS
* smll - streamlabs machine learning library
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
#pragma once
#pragma warning( push )
#pragma warning( disable: 4127 )
#pragma warning( disable: 4201 )
#pragma warning( disable: 4456 )
#pragma warning( disable: 4458 )
#pragma warning( disable: 4459 )
#pragma warning( disable: 4505 )
#pragma warning( disable: 4267 )
#pragma warning( disable: 4100 )
#include <dlib/image_processing.h>
#include <dlib/filtering.h>
#include <dlib/matrix.h>
#pragma warning( pop )

#include <vector>
#include <array>

#include "landmarks.hpp"
#include "SingleValueKalman.hpp"
#include "Face.hpp"
#include "../Plugin/utils.h"
#include <opencv2/opencv.hpp>

namespace smll {

	class ThreeDPose{
	public:
		ThreeDPose();

		void SetPose(cv::Mat cvRot, cv::Mat cvTrs);
		cv::Mat GetCVRotation() const;
		cv::Mat GetCVTranslation() const;

		double DistanceTo(const ThreeDPose& r) const;
		void CopyPoseFrom(const ThreeDPose& r);
		void ResetPose();
		bool PoseValid();

		double		translation[3];
		double		rotation[4];

	};

	class ProcessedResults {
	public:
		ProcessedResults();
		void FrameSkipped();
		void DetectionMade();
		void TrackingMade();
		void TrackingFailed();
		void DetectionFailed();
		bool isSkipped();
		std::string to_string();
		std::string titles_to_string();
	private:
		bool skipped;
		bool detection;
		bool tracking;
		bool tracking_failed;
		bool detection_failed;
	};


	typedef sarray<ThreeDPose, MAX_FACES> ThreeDPoses;


	class DetectionResult
	{
	public:
		// face detection/tracking
		dlib::rectangle		bounds;

		// facial landmarks (68 point)
		dlib::point			landmarks68[NUM_FACIAL_LANDMARKS];

		// 3D pose 
		ThreeDPose			pose;
		// Start Pose
		ThreeDPose			startPose;
		bool				initedStartPose;

		DetectionResult();
		DetectionResult(const DetectionResult& r) { *this = r; }
		~DetectionResult();
		DetectionResult& operator=(const DetectionResult& r);
		DetectionResult& operator=(const Face& f);

		void SetPose(const ThreeDPose& p);
		void SetPose(cv::Mat cvRot, cv::Mat cvTrs);
		cv::Mat GetCVRotation() const;
		cv::Mat GetCVTranslation() const;

		void CopyPoseFrom(const DetectionResult& r);
		void InitStartPose();
		void UpdateResultsFrom(const DetectionResult& r);

		double DistanceTo(const DetectionResult& r) const;

		inline dlib::point GetPosition() {
			long x = (int)(bounds.right() + bounds.left()) / 2;
			long y = (int)(bounds.top() + bounds.bottom()) / 2;
			return dlib::point(x, y);
		}

		bool matched;
		int numFramesLost;


	private:
		// Kalman Filter variables
		cv::KalmanFilter kalmanFilter; // Initialize Kalman Filter
		std::array<SingleValueKalman, 2*NUM_FACIAL_LANDMARKS> kalmanFilters;
		int nStates;
		int nMeasurements;
		int nInputs;
		double dt; // 1/FPS. TODO: Get it from current FPS.
		bool kalmanFilterInitialized;

		// Kalman Filter methods
		void InitKalmanFilter();
		void UpdateKalmanFilter(cv::Mat& measurements, cv::Mat& translationEstimated, cv::Mat& eulersEstimated);
	};


	class DetectionResults : public sarray<DetectionResult, MAX_FACES>
	{
	public:
		DetectionResults();
		void CorrelateAndUpdateFrom(DetectionResults& other);
		int findClosest(const smll::DetectionResult& result);
		ProcessedResults processedResults;
		dlib::rectangle motionRect;
	};

}

