/****************************************************************************/
/*                                                                          */
/* Original algorithm: http://ivrg.epfl.ch/research/superpixels             */
/* Original OpenCV implementation: http://github.com/PSMM/SLIC-Superpixels  */
/*                                                                          */
/* Paper: "Optimizing Superpixel Clustering for Real-Time                   */
/*         Egocentric-Vision Applications"                                  */
/*        http://www.isip40.it/resources/papers/2015/SPL_Pietro.pdf         */
/*                                                                          */
/****************************************************************************/

#include "SLIC.h"
/* Deletion of unuseful includes: they're in the header SLIC.h*/

using namespace cv;

SLIC::SLIC()
{
	clearSLICData();
	this->pixelCluster.clear();
	this->distanceFromClusterCentre.clear();
	this->pixelReachedByClusters.clear();
}

SLIC::SLIC(const SLIC& otherSLIC)
{
	/* Copy debug data. */
	this->averageError = otherSLIC.averageError;
	this->averageIterations = otherSLIC.averageIterations;
	this->minError = otherSLIC.minError;
	this->minIterations = otherSLIC.minIterations;
	this->maxError = otherSLIC.maxError;
	this->maxIterations = otherSLIC.maxIterations;
	this->averageExecutionTime = otherSLIC.averageExecutionTime;
	this->minExecutionTime = otherSLIC.minExecutionTime;
	this->maxExecutionTime = otherSLIC.maxExecutionTime;

	/* Copy variables. */
	this->iterationIndex = otherSLIC.iterationIndex;
	this->pixelsNumber = otherSLIC.pixelsNumber;
	this->clustersNumber = otherSLIC.clustersNumber;
	this->samplingStep = otherSLIC.samplingStep;
	this->spatialDistanceWeight = otherSLIC.spatialDistanceWeight;
	this->distanceFactor = otherSLIC.distanceFactor;
	this->totalResidualError = otherSLIC.totalResidualError;
	this->errorThreshold = otherSLIC.errorThreshold;
	this->framesNumber = otherSLIC.framesNumber;

	/* Copy matrices. */
	this->pixelCluster.resize(otherSLIC.pixelsNumber);
	this->distanceFromClusterCentre.resize(otherSLIC.pixelsNumber);
	this->clusterCentres.resize(otherSLIC.clusterCentres.size());
	this->previousClusterCentres.resize(otherSLIC.previousClusterCentres.size());
	this->pixelsOfSameCluster.resize(otherSLIC.pixelsOfSameCluster.size());
	this->residualError.resize(otherSLIC.residualError.size());

	for (unsigned n = 0; n < otherSLIC.pixelsNumber; ++n)
	{
		this->pixelCluster[n] = otherSLIC.pixelCluster[n];
		this->pixelReachedByClusters[n] = otherSLIC.pixelReachedByClusters[n];
		this->distanceFromClusterCentre[n] = otherSLIC.distanceFromClusterCentre[n];
	}

	for (unsigned n = 0; n < otherSLIC.clustersNumber; ++n)
	{
		this->pixelsOfSameCluster[n] = otherSLIC.pixelsOfSameCluster[n];
		this->residualError[n] = otherSLIC.residualError[n];
	}

	for (unsigned n = 0; n < 5 * otherSLIC.clustersNumber; ++n)
	{
		this->clusterCentres[n] = otherSLIC.clusterCentres[n];
		this->previousClusterCentres[n] = otherSLIC.previousClusterCentres[n];
	}
}

SLIC::~SLIC()
{
	clearSLICData();
	this->pixelCluster.clear();
	this->pixelReachedByClusters.clear();
	this->distanceFromClusterCentre.clear();
}

void SLIC::clearSLICData()
{
	/* Reset debug data. */
	this->averageError = 0;
	this->averageIterations = 0;
	this->minError = 0;
	this->minIterations = 0;
	this->maxError = 0;
	this->maxIterations = 0;
	this->averageExecutionTime = 0;
	this->minExecutionTime = 0;
	this->maxExecutionTime = 0;

	/* Reset variables. */
	this->iterationIndex = 0;
	this->clustersNumber = 0;
	this->pixelsNumber = 0;
	this->samplingStep = 0;
	this->spatialDistanceWeight = 0;
	this->distanceFactor = 0;
	this->totalResidualError = 0;
	this->errorThreshold = 0;
	this->framesNumber = 0;

	/* Erase all matrices' elements. */
	//this->pixelCluster.clear();
	//this->distanceFromClusterCentre.clear();
	this->clusterCentres.clear();
	this->previousClusterCentres.clear();
	this->pixelsOfSameCluster.clear();
	this->residualError.clear();
}

void SLIC::initializeSLICData(
	const cv::Mat&       image,
	const unsigned       samplingStep,
	const unsigned       spatialDistanceWeight,
	const double         errorThreshold,
	VideoElaborationMode videoMode,
	const unsigned       keyFramesRatio,
	const double         GaussianStdDev,
	const bool           connectedFrames)
{
	/* If centres matrix from previous frame is empty,
	or if frames must be processed independently,
	initialize data from scratch. Otherwise, use
	the data from previous frame as initialization.*/
	if (connectedFrames == false || clusterCentres.size() == 0 ||
		/* Initialize data from scratch when using key frames. */
		(((videoMode == KEY_FRAMES) || (videoMode == KEY_FRAMES_NOISE))
			&& (framesNumber % keyFramesRatio == 0)) ||
		(((videoMode == ADD_SUPERPIXELS) || (videoMode == ADD_SUPERPIXELS_NOISE))
			&& (clustersNumber > 1300)))
	{
		/* Clear previous data before initialization. */
		if (clusterCentres.size() == 0) {
			this->pixelCluster.clear();
			this->distanceFromClusterCentre.clear();
			this->pixelReachedByClusters.clear();
		}
		clearSLICData();

		/* Initialize debug data. */
		this->minError = DBL_MAX;
		this->minIterations = UINT_MAX;
		this->minExecutionTime = UINT_MAX;

		/* Initialize variables. */
		this->pixelsNumber = image.rows * image.cols;
		this->samplingStep = samplingStep;
		this->spatialDistanceWeight = spatialDistanceWeight;
		this->distanceFactor =
			static_cast<double>(1.0 * spatialDistanceWeight * spatialDistanceWeight / (samplingStep * samplingStep));
		this->errorThreshold = errorThreshold;

		/* Initialize the clusters and the distances matrices. */
		pixelCluster.assign(pixelsNumber, -1);
		//pixelReachedByClusters.assign(pixelsNumber, 255);
		distanceFromClusterCentre.assign(pixelsNumber, DBL_MAX);

		/* Initialize the centres matrix by sampling the image
		at a regular step. */
		for (int y = samplingStep; y < image.rows; y += samplingStep)
			for (int x = samplingStep; x < image.cols; x += samplingStep)
			{
				/* Find the pixel with the lowest gradient in a 3x3 surrounding. */
				Point lowestGradientPixel = findLowestGradient(image, Point(x, y));
				Vec3b tempPixelColor = image.at<Vec3b>(lowestGradientPixel.y, lowestGradientPixel.x);

				/* Insert a [L, A, B, x, y] centre in the centres vector. */
				clusterCentres.push_back(tempPixelColor.val[0]);
				clusterCentres.push_back(tempPixelColor.val[1]);
				clusterCentres.push_back(tempPixelColor.val[2]);
				clusterCentres.push_back(lowestGradientPixel.x);
				clusterCentres.push_back(lowestGradientPixel.y);

				/* During initialization, previous cluster centres matrix
				is the same as cluster centres matrix.*/
				previousClusterCentres.push_back(tempPixelColor.val[0]);
				previousClusterCentres.push_back(tempPixelColor.val[1]);
				previousClusterCentres.push_back(tempPixelColor.val[2]);
				previousClusterCentres.push_back(lowestGradientPixel.x);
				previousClusterCentres.push_back(lowestGradientPixel.y);

				/* Initialize "pixel of same cluster" matrix. */
				pixelsOfSameCluster.push_back(0);

				/* Initialize residual error to be zero for each cluster
				centre. */
				residualError.push_back(0);
			}

		/* Total number of clusters. */
		this->clustersNumber = static_cast<unsigned>(pixelsOfSameCluster.size());

		///* Reset orphan pixels */
		//if (videoMode == ADD_SUPERPIXELS || videoMode == ADD_SUPERPIXELS_NOISE)
		//	orphanPixels = Mat(image.rows, image.cols, CV_8UC1, cv::Scalar(255));
	}
	/* Add Gaussian noise if requested. */
	else if ((videoMode == NOISE) || (videoMode == KEY_FRAMES_NOISE) || (videoMode == ADD_SUPERPIXELS_NOISE))
	{
		/* Random noise generator. */
		RandNormal randomGen(0.0, GaussianStdDev);

		/* Add some gaussian noise to position. */
		/* Color should be kept equal: we look for a similar color in the surroundings. */
		for (unsigned n = 0; n < clustersNumber; ++n)
		{
			clusterCentres[5 * n + 3] += randomGen();
			clusterCentres[5 * n + 4] += randomGen();
		}
	}

	/* Initialize total residual error for each frame. */
	this->totalResidualError = DBL_MAX;

	/* Reset orphan pixels */
	if (videoMode == ADD_SUPERPIXELS || videoMode == ADD_SUPERPIXELS_NOISE)
		pixelReachedByClusters.assign(pixelsNumber, 255);

	//if (videoMode == ADD_SUPERPIXELS || videoMode == ADD_SUPERPIXELS_NOISE)
	////	orphanPixels = Mat(image.rows, image.cols, CV_8UC1, cv::Scalar(255));
	//	orphanPixels.setTo(cv::Scalar(255));
}

Point SLIC::findLowestGradient(
	const cv::Mat&   image,
	const cv::Point& centre)
{
	unsigned lowestGradient = UINT_MAX;
	Point lowestGradientPoint = Point(centre.x, centre.y);

	for (int y = centre.y - 1; y <= centre.y + 1 && y < image.rows - 1; ++y)
		for (int x = centre.x - 1; x <= centre.x + 1 && x < image.cols - 1; ++x)
		{
			/* Exclude pixels on borders. */
			if (x < 1 || y < 1)
				continue;

			/* Compute horizontal and vertical gradients and keep track
			of the minimum. */
			unsigned tempGradient =
				(image.at<Vec3b>(y, x + 1).val[0] - image.at<Vec3b>(y, x - 1).val[0]) *
				(image.at<Vec3b>(y, x + 1).val[0] - image.at<Vec3b>(y, x - 1).val[0]) +
				(image.at<Vec3b>(y - 1, x).val[0] - image.at<Vec3b>(y + 1, x).val[0]) *
				(image.at<Vec3b>(y - 1, x).val[0] - image.at<Vec3b>(y + 1, x).val[0]);

			if (tempGradient < lowestGradient)
			{
				lowestGradient = tempGradient;
				lowestGradientPoint.x = x;
				lowestGradientPoint.y = y;
			}
		}

	return lowestGradientPoint;
}

double SLIC::computeDistance(
	const int        centreIndex,
	const cv::Point& pixelPosition,
	const cv::Vec3b& pixelColor)
{
	/* Compute the color distance between two pixels. */
	double colorDistance =
		(clusterCentres[5 * centreIndex] - pixelColor.val[0]) *
		(clusterCentres[5 * centreIndex] - pixelColor.val[0]) +
		(clusterCentres[5 * centreIndex + 1] - pixelColor.val[1]) *
		(clusterCentres[5 * centreIndex + 1] - pixelColor.val[1]) +
		(clusterCentres[5 * centreIndex + 2] - pixelColor.val[2]) *
		(clusterCentres[5 * centreIndex + 2] - pixelColor.val[2]);

	/* Compute the spatial distance between two pixels. */
	double spaceDistance =
		(clusterCentres[5 * centreIndex + 3] - pixelPosition.x) *
		(clusterCentres[5 * centreIndex + 3] - pixelPosition.x) +
		(clusterCentres[5 * centreIndex + 4] - pixelPosition.y) *
		(clusterCentres[5 * centreIndex + 4] - pixelPosition.y);

	/* Compute total distance between two pixels using the formula
	described in the paper. */
	return colorDistance + distanceFactor * spaceDistance;
}

void SLIC::createSuperpixels(
	const cv::Mat&       image,
	const unsigned       samplingStep,
	const unsigned       spatialDistanceWeight,
	const unsigned       iterationNumber,
	const double         errorThreshold,
	SLICElaborationMode  SLICMode,
	VideoElaborationMode videoMode,
	const unsigned       keyFramesRatio,
	const double         GaussianStdDev,
	const bool           connectedFrames)
{
	/* Initialize algorithm data. */
	initializeSLICData(
		image, samplingStep, spatialDistanceWeight, errorThreshold,
		videoMode, keyFramesRatio, GaussianStdDev, connectedFrames);
	iterationIndex = 0;
	bool go = false;
	/* Repeat next steps until error is lower than the threshold or
	until the number of iteration is reached. */
	/*for (iterationIndex = 0; ((totalResidualError > errorThreshold) && (SLICMode == ERROR_THRESHOLD)) ||
	((iterationIndex < iterationNumber) && (SLICMode == FIXED_ITERATIONS)); ++iterationIndex)*/
	do
	{
		/* Reset distance values. */
		distanceFromClusterCentre.assign(pixelsNumber, DBL_MAX);

		tbb::parallel_for<unsigned>(0, clustersNumber, 1, [=](unsigned centreIndex)
		{
			/* For each cluster, look for pixels in a 2 x step by 2 x step region only. */
			for (int y = static_cast<int>(clusterCentres[5 * centreIndex + 4]) - samplingStep - 1;
			y < clusterCentres[5 * centreIndex + 4] + samplingStep + 1; ++y)
				for (int x = static_cast<int>(clusterCentres[5 * centreIndex + 3]) - samplingStep - 1;
			x < clusterCentres[5 * centreIndex + 3] + samplingStep + 1; ++x)
			{
				/* Verify that neighbor pixel is within the image boundaries. */
				if (x >= 0 && x < image.cols && y >= 0 && y < image.rows)
				{
					Vec3b pixelColor = image.at<Vec3b>(y, x);

					double tempDistance =
						computeDistance(centreIndex, Point(x, y), pixelColor);

					/* This pixel has been searched */
					if (videoMode == ADD_SUPERPIXELS || videoMode == ADD_SUPERPIXELS_NOISE)
						pixelReachedByClusters[y * image.cols + x] = 0;
					//orphanPixels.at<uchar>(y, x) = 0;

					/* Update pixel's cluster if this distance is smaller
					than pixel's previous distance. */
					if (tempDistance < distanceFromClusterCentre[y * image.cols + x])
					{
						distanceFromClusterCentre[y * image.cols + x] = tempDistance;
						pixelCluster[y * image.cols + x] = centreIndex;
					}
				}
			}
		});

		/* Reset centres values and the number of pixel
		per cluster to zero.
		/*tbb::parallel_for<unsigned>(0, clustersNumber, 1, [=](unsigned centreIndex)
		{
		clusterCentres[5 * centreIndex] = 0;
		clusterCentres[5 * centreIndex + 1] = 0;
		clusterCentres[5 * centreIndex + 2] = 0;
		clusterCentres[5 * centreIndex + 3] = 0;
		clusterCentres[5 * centreIndex + 4] = 0;

		pixelsOfSameCluster[centreIndex] = 0;
		});*/

		/* Reset centres values and the number of pixel
		per cluster to zero. */
		clusterCentres.assign(clustersNumber * 5, 0);
		pixelsOfSameCluster.assign(clustersNumber, 0);

		/* Compute the new cluster centres. */
		for (int y = 0; y < image.rows; ++y)
			for (int x = 0; x < image.cols; ++x)
			{
				int currentPixelCluster = pixelCluster[y * image.cols + x];

				/* Verify if current pixel belongs to a cluster. */
				if (currentPixelCluster != -1)
				{
					/* Sum the information of pixels of the same
					cluster for future centre recalculation. */
					Vec3b pixelColor = image.at<Vec3b>(y, x);

					clusterCentres[5 * currentPixelCluster] += pixelColor.val[0];
					clusterCentres[5 * currentPixelCluster + 1] += pixelColor.val[1];
					clusterCentres[5 * currentPixelCluster + 2] += pixelColor.val[2];
					clusterCentres[5 * currentPixelCluster + 3] += x;
					clusterCentres[5 * currentPixelCluster + 4] += y;

					pixelsOfSameCluster[currentPixelCluster] += 1;
				}
			}

		/* Normalize the clusters' centres. */
		tbb::parallel_for<unsigned>(0, clustersNumber, 1, [=](unsigned centreIndex)
		{
			/* Avoid empty clusters, if there are any. */
			if (pixelsOfSameCluster[centreIndex] != 0)
			{
				clusterCentres[5 * centreIndex] /= pixelsOfSameCluster[centreIndex];
				clusterCentres[5 * centreIndex + 1] /= pixelsOfSameCluster[centreIndex];
				clusterCentres[5 * centreIndex + 2] /= pixelsOfSameCluster[centreIndex];
				clusterCentres[5 * centreIndex + 3] /= pixelsOfSameCluster[centreIndex];
				clusterCentres[5 * centreIndex + 4] /= pixelsOfSameCluster[centreIndex];
			}
		});

		/* Skip error calculation if this is the first iteration,
		meaning this is a new frame in the video. */
		if (iterationIndex == 0)
			for (unsigned centreIndex = 0; centreIndex < 5 * clustersNumber; ++centreIndex)
			{
				/* Update previous centres matrix. */
				previousClusterCentres[centreIndex] = clusterCentres[centreIndex];
			}
		else
		{
			/* Compute residual error. */
			tbb::parallel_for<unsigned>(0, clustersNumber, 1, [=](unsigned centreIndex)
			{
				/* Calculate residual error for each cluster centre. */
				residualError[centreIndex] = sqrt(
					(clusterCentres[5 * centreIndex + 4] - previousClusterCentres[5 * centreIndex + 4]) *
					(clusterCentres[5 * centreIndex + 4] - previousClusterCentres[5 * centreIndex + 4]) +
					(clusterCentres[5 * centreIndex + 3] - previousClusterCentres[5 * centreIndex + 3]) *
					(clusterCentres[5 * centreIndex + 3] - previousClusterCentres[5 * centreIndex + 3]));

				/* Update previous centres matrix. */
				previousClusterCentres[5 * centreIndex] = clusterCentres[5 * centreIndex];
				previousClusterCentres[5 * centreIndex + 1] = clusterCentres[5 * centreIndex + 1];
				previousClusterCentres[5 * centreIndex + 2] = clusterCentres[5 * centreIndex + 2];
				previousClusterCentres[5 * centreIndex + 3] = clusterCentres[5 * centreIndex + 3];
				previousClusterCentres[5 * centreIndex + 4] = clusterCentres[5 * centreIndex + 4];
			});

			/* Compute total residual error by averaging all clusters' errors. */
			totalResidualError = 0;

			for (unsigned centreIndex = 0; centreIndex < clustersNumber; ++centreIndex)
				totalResidualError += residualError[centreIndex];

			totalResidualError /= clustersNumber;
		}

		/* Blob Detector */
		/* At the last iteration it finds orphan pixels and it creates a new superpixel to fix it */
		if ((videoMode == ADD_SUPERPIXELS || videoMode == ADD_SUPERPIXELS_NOISE)
			&& (((totalResidualError < errorThreshold) && (SLICMode == ERROR_THRESHOLD)) ||
				((iterationIndex >= iterationNumber-1) && (SLICMode == FIXED_ITERATIONS)))
			&& (std::any_of(pixelReachedByClusters.begin(),
				pixelReachedByClusters.end(),
				[](uchar u) {return u == 255; })))
		{
			/* Image containing orphan pixels */
			Mat orphanPixels = Mat(image.rows, image.cols, CV_8UC1, pixelReachedByClusters.data());

			//Mat orphanPixels = Mat(image.rows, image.cols, CV_8UC1);
			///* Fill orphanPixels with data */
			//memcpy(orphanPixels.data, pixelReachedByClusters.data(),
			//	pixelReachedByClusters.size()*sizeof(uchar));

			///* Study use only*/
			///* Convert Grayscale image back to RGB */
			//Mat colouredOrphanPixels;
			//cvtColor(orphanPixels, colouredOrphanPixels, CV_GRAY2RGB);
			//drawClusterContours(colouredOrphanPixels, Vec3b(0, 0, 255));
			//drawClusterCentres(colouredOrphanPixels, Scalar(255, 0, 0));
			///* end of Study use only*/

			Mat workHere;
			orphanPixels.copyTo(workHere);

			///* Thesis writing use only - save image */
			//imwrite("../../ThesisData/Images/" + std::to_string(framesNumber) + " 0 orphanPixels.jpg", orphanPixels);

			/* Apply dilation to make all blobs detectable */
			cv::dilate(workHere, workHere,
				getStructuringElement(MORPH_RECT, Size(10, 10)));

			///* Thesis writing use only - save image */
			//imwrite("../../ThesisData/Images/" + std::to_string(framesNumber) + " 1 dilation.jpg", workHere);

			/* Apply a "frame" to separate the blobs from the borders
			of the image, because otherwise not all the blobs
			would have been detected */
			cv::rectangle(workHere, Point(0, 0),
				Point(workHere.cols - 1, workHere.rows - 1), Scalar(0), 2);

			///* Thesis writing use only - save image */
			//imwrite("../../ThesisData/Images/" + std::to_string(framesNumber) + " 2 framing.jpg", workHere);

			/* Image used to find contours*/
			Mat canny_output = Mat(image.rows, image.cols, CV_8UC1);

			/* Vectors useful to find blobs' centers*/
			std::vector<std::vector<Point>> contours;
			std::vector<Vec4i> hierarchy;

			/* Detect edges using canny */
			Canny(workHere, canny_output, 100, 100 * 2, 3);

			///* Thesis writing use only - save image */
			//imwrite("../../ThesisData/Images/" + std::to_string(framesNumber) + " 3 canny.jpg", canny_output);

			/* Find contours */
			findContours(canny_output, contours, hierarchy,
				RETR_LIST, CHAIN_APPROX_SIMPLE, Point(0, 0));

			///* Thesis writing use only - save image */
			//imwrite("../../ThesisData/Images/" + std::to_string(framesNumber) + " 4 findcontours.jpg", canny_output);

			///* Thesis writing use only - save image */
			//imwrite("../../ThesisData/Images/" + std::to_string(framesNumber) + " 5 oldClusterCentres.jpg", colouredOrphanPixels);

			/* Get the moments and the mass centers
			(the centers of the orphans' pixels) */
			for (size_t i = 0; i < contours.size(); i++) {
				Moments mu = moments(contours[i]);

				if (mu.m00 == 0)
					continue;

				/* Add the new clusterCentre */
				clusterCentres.push_back(0);
				clusterCentres.push_back(0);
				clusterCentres.push_back(0);
				clusterCentres.push_back(static_cast<float>(mu.m10 / mu.m00));
				clusterCentres.push_back(static_cast<float>(mu.m01 / mu.m00));

				previousClusterCentres.push_back(0);
				previousClusterCentres.push_back(0);
				previousClusterCentres.push_back(0);
				previousClusterCentres.push_back(static_cast<float>(mu.m10 / mu.m00));
				previousClusterCentres.push_back(static_cast<float>(mu.m01 / mu.m00));

				/* Add the new cluster */
				pixelsOfSameCluster.push_back(0);
				residualError.push_back(0);

				/* update number of clusters */
				clustersNumber += 1;

				//numberOfCentres += 1;
				//circle(colouredOrphanPixels, Point2f(static_cast<float>(mu.m10 / mu.m00),
				//	static_cast<float>(mu.m01 / mu.m00)), 1, Scalar(255, 255, 0), 2);
			}

			///* Thesis writing use only - save image */
			//imwrite("../../ThesisData/Images/" + std::to_string(framesNumber) + " 6 newClusterCentres.jpg", colouredOrphanPixels);

			orphanPixels.release();
			contours.clear();
			hierarchy.clear();
			workHere.release();
			//colouredOrphanPixels.release();
			canny_output.release();
		}
		else go = true;

		++iterationIndex;

	} while ((((totalResidualError > errorThreshold) && (SLICMode == ERROR_THRESHOLD)) ||
		((iterationIndex < iterationNumber) && (SLICMode == FIXED_ITERATIONS))) && go);

	/* Another frame was processed. */
	++framesNumber;
}

void SLIC::enforceConnectivity(const cv::Mat image)
{
	int adjacentCluster = 0;

	/* Average number of pixels contained in any expected cluster. */
	const unsigned clustersAverageSize = static_cast<unsigned>(pixelsNumber / clustersNumber + 0.5);

	std::vector<int> newPixelCluster;

	/* Initialize the new cluster matrix. */
	for (unsigned n = 0; n < pixelsNumber; ++n)
		newPixelCluster.push_back(-1);

	/* Starting from each pixel in the image, we create paths of pixels belonging to the same cluster.
	If the resulting paths are not long enough, i.e. paths contain less than 1/4 of the average number of
	pixels contained in a cluster, then these paths are merged with a surrounding adjacent cluster. Doing so,
	in the end there won't be any superpixels smaller than 1/4 of the average expected cluster size. */
	for (int y = 0; y < image.rows; ++y)
		for (int x = 0; x < image.cols; ++x)
			if (newPixelCluster[y * image.cols + x] == -1)
			{
				std::vector<Point> pixelSegment;
				pixelSegment.push_back(Point(x, y));

				newPixelCluster[y * image.cols + x] = pixelCluster[y * image.cols + x];

				/* Find an adjacent cluster for possible later use. */
				for (int tempY = pixelSegment[0].y - 1; tempY <= pixelSegment[0].y + 1; ++tempY)
					for (int tempX = pixelSegment[0].x - 1; tempX <= pixelSegment[0].x + 1; ++tempX)
					{
						/* Verify that neighbor pixel is within the image boundaries
						and belongs to a cluster. */
						if (tempX >= 0 && tempX < image.cols && tempY >= 0 && tempY < image.rows &&
							newPixelCluster[tempY * image.cols + tempX] != -1)
						{
							/* Break the loop when finding an adjacent cluster. */
							adjacentCluster = newPixelCluster[tempY * image.cols + tempX];
							break;
						}
					}

				unsigned count = 1;
				for (unsigned c = 0; c < count; ++c)
					for (int tempY = pixelSegment[c].y - 1; tempY <= pixelSegment[c].y + 1; ++tempY)
						for (int tempX = pixelSegment[c].x - 1; tempX <= pixelSegment[c].x + 1; ++tempX)
						{
							/* Verify that neighbor pixel is within the image boundaries
							and belongs to the same cluster of the original pixel. */
							if (tempX >= 0 && tempX < image.cols && tempY >= 0 && tempY < image.rows &&
								newPixelCluster[tempY * image.cols + tempX] == -1 &&
								pixelCluster[tempY * image.cols + tempX] == pixelCluster[y * image.cols + x])
							{
								/* Expand the segment by inserting pixels of the same cluster. */
								pixelSegment.push_back(Point(tempX, tempY));
								newPixelCluster[tempY * image.cols + tempX] = pixelCluster[y * image.cols + x];
								++count;
							}
						}

				/* Enforce connectivity for the segments smaller than 1/4 of
				the average cluster size. */
				if (count <= clustersAverageSize / 4)
					for (unsigned c = 0; c < count; ++c)
						newPixelCluster[pixelSegment[c].y * image.cols + pixelSegment[c].x] = adjacentCluster;
			}

	for (unsigned n = 0; n < pixelsNumber; ++n)
		pixelCluster[n] = newPixelCluster[n];

	/* After enforcing connectivity, cluster centres must be recalculated. */
	/* Reset centres values and the number of pixel
	per cluster to zero. */
	tbb::parallel_for<unsigned>(0, clustersNumber, 1, [=](unsigned centreIndex)
	{
		clusterCentres[5 * centreIndex] = 0;
		clusterCentres[5 * centreIndex + 1] = 0;
		clusterCentres[5 * centreIndex + 2] = 0;
		clusterCentres[5 * centreIndex + 3] = 0;
		clusterCentres[5 * centreIndex + 4] = 0;

		pixelsOfSameCluster[centreIndex] = 0;
	});

	/* Compute the new cluster centres. */
	for (int y = 0; y < image.rows; ++y)
		for (int x = 0; x < image.cols; ++x)
		{
			int currentPixelCluster = pixelCluster[y * image.cols + x];

			/* Verify if current pixel belongs to a cluster. */
			if (currentPixelCluster != -1)
			{
				/* Sum the information of pixels of the same
				cluster for future centre recalculation. */
				Vec3b pixelColor = image.at<Vec3b>(y, x);

				clusterCentres[5 * currentPixelCluster] += pixelColor.val[0];
				clusterCentres[5 * currentPixelCluster + 1] += pixelColor.val[1];
				clusterCentres[5 * currentPixelCluster + 2] += pixelColor.val[2];
				clusterCentres[5 * currentPixelCluster + 3] += x;
				clusterCentres[5 * currentPixelCluster + 4] += y;

				pixelsOfSameCluster[currentPixelCluster] += 1;
			}
		}

	/* Normalize the clusters' centres. */
	tbb::parallel_for<unsigned>(0, clustersNumber, 1, [=](unsigned centreIndex)
	{
		/* Avoid empty clusters, if there are any. */
		if (pixelsOfSameCluster[centreIndex] != 0)
		{
			clusterCentres[5 * centreIndex] /= pixelsOfSameCluster[centreIndex];
			clusterCentres[5 * centreIndex + 1] /= pixelsOfSameCluster[centreIndex];
			clusterCentres[5 * centreIndex + 2] /= pixelsOfSameCluster[centreIndex];
			clusterCentres[5 * centreIndex + 3] /= pixelsOfSameCluster[centreIndex];
			clusterCentres[5 * centreIndex + 4] /= pixelsOfSameCluster[centreIndex];
		}
	});
}

void SLIC::colorSuperpixels(
	cv::Mat&  image,
	cv::Rect& areaToColor)
{
	/* Verify that area to color is within image boundaries,
	otherwise reset area to the entire image. */
	if (areaToColor.x < 0 || areaToColor.x > image.cols)
		areaToColor.x = 0;
	if (areaToColor.y < 0 || areaToColor.y > image.rows)
		areaToColor.y = 0;
	if (areaToColor.width < 0 || areaToColor.x + areaToColor.width > image.cols)
		areaToColor.width = image.cols - areaToColor.x;
	if (areaToColor.height < 0 || areaToColor.y + areaToColor.height > image.rows)
		areaToColor.height = image.rows - areaToColor.y;

	/* Fill in each cluster with its average color (cluster centre color). */
	tbb::parallel_for(areaToColor.y, areaToColor.y + areaToColor.height, 1, [&](int y)
	{
		for (int x = areaToColor.x; x < areaToColor.x + areaToColor.width; ++x)
			if (pixelCluster[y * image.cols + x] >= 0 &&
				static_cast<unsigned>(pixelCluster[y * image.cols + x]) < clustersNumber)
			{
				image.at<Vec3b>(y, x) = Vec3d(
					clusterCentres[5 * pixelCluster[y * image.cols + x]],
					clusterCentres[5 * pixelCluster[y * image.cols + x] + 1],
					clusterCentres[5 * pixelCluster[y * image.cols + x] + 2]);
			}
	});
}

void SLIC::drawClusterContours(
	cv::Mat&         image,
	const cv::Vec3b& contourColor,
	cv::Rect&        areaToDraw)
{
	/* Verify that area to color is within image boundaries,
	otherwise reset area to the entire image. */
	if (areaToDraw.x < 0 || areaToDraw.x > image.cols)
		areaToDraw.x = 0;
	if (areaToDraw.y < 0 || areaToDraw.y > image.rows)
		areaToDraw.y = 0;
	if (areaToDraw.width < 0 || areaToDraw.x + areaToDraw.width > image.cols)
		areaToDraw.width = image.cols - areaToDraw.x;
	if (areaToDraw.height < 0 || areaToDraw.y + areaToDraw.height > image.rows)
		areaToDraw.height = image.rows - areaToDraw.y;

	/* Create a matrix with bool values detailing whether a
	pixel is a contour or not. */
	std::vector<bool> isContour(pixelsNumber);

	/* Scan all the pixels and compare them to neighbor
	pixels to see if they belong to a different cluster. */
	tbb::parallel_for(areaToDraw.y, areaToDraw.y + areaToDraw.height, 1, [&](int y)
	{
		for (int x = areaToDraw.x; x < areaToDraw.x + areaToDraw.width; ++x)
		{
			/* Continue only if the selected pixel
			belongs to a cluster. */
			if (pixelCluster[y * image.cols + x] >= 0)
			{
				/* Compare the pixel to its eight neighbor pixels. */
				for (int tempY = y - 1; tempY <= y + 1; ++tempY)
					for (int tempX = x - 1; tempX <= x + 1; ++tempX)
					{
						/* Verify that neighbor pixel is within the image boundaries. */
						if (tempX >= 0 && tempX < image.cols && tempY >= 0 && tempY < image.rows)
							/* Verify that neighbor pixel belongs to a valid different cluster
							and it's not already a contour pixel. */
							if (pixelCluster[tempY * image.cols + tempX] > -1 &&
								pixelCluster[y * image.cols + x] != pixelCluster[tempY * image.cols + tempX] &&
								isContour[y * image.cols + x] == false)
							{
								isContour[y * image.cols + x] = true;
								/* Color contour pixel. */
								image.at<Vec3b>(y, x) = contourColor;
							}
					}
			}
		}
	});
}

void SLIC::drawClusterCentres(
	cv::Mat&          image,
	const cv::Scalar& centreColor)
{
	tbb::parallel_for<unsigned>(0, clustersNumber, 1, [&](unsigned n)
	{
		/* Draw a circle on the image for each cluster centre. */
		circle(image, Point(static_cast<int>(clusterCentres[5 * n + 3]),
			static_cast<int>(clusterCentres[5 * n + 4])), 2, centreColor, 2);
	});
}

void SLIC::drawInformation(
	cv::Mat&       image,
	const unsigned totalFrames,
	const unsigned executionTimeInMilliseconds)
{
	std::ostringstream stringStream;

	if (totalResidualError < minError)
		minError = totalResidualError;
	if (totalResidualError > maxError)
		maxError = totalResidualError;
	if (iterationIndex < minIterations)
		minIterations = iterationIndex;
	if (iterationIndex > maxIterations)
		maxIterations = iterationIndex;
	if (executionTimeInMilliseconds < minExecutionTime)
		minExecutionTime = executionTimeInMilliseconds;
	if (executionTimeInMilliseconds > maxExecutionTime)
		maxExecutionTime = executionTimeInMilliseconds;

	rectangle(image, Point(0, 0), Point(260, 320), CV_RGB(255, 255, 255), CV_FILLED);

	stringStream << "Frame: " << framesNumber << " (" << totalFrames << " total)";
	putText(image, stringStream.str(), Point(5, 20),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Superpixels: " << clustersNumber;
	putText(image, stringStream.str(), Point(5, 40),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Distance weight: " << spatialDistanceWeight;
	putText(image, stringStream.str(), Point(5, 60),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Exe. time now: " << executionTimeInMilliseconds << " ms";
	putText(image, stringStream.str(), Point(5, 80),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Exe. time max.: " << maxExecutionTime;
	putText(image, stringStream.str(), Point(5, 100),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Exe. time min.: " << minExecutionTime;
	putText(image, stringStream.str(), Point(5, 120),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Exe. time avg.: " << (averageExecutionTime += executionTimeInMilliseconds) / framesNumber << " ms";
	putText(image, stringStream.str(), Point(5, 140),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Iterations now: " << iterationIndex;
	putText(image, stringStream.str(), Point(5, 160),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Iterations max.: " << maxIterations;
	putText(image, stringStream.str(), Point(5, 180),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Iterations min.: " << minIterations;
	putText(image, stringStream.str(), Point(5, 200),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Iterations avg.: " << (averageIterations += iterationIndex) / framesNumber;
	putText(image, stringStream.str(), Point(5, 220),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Error now: " << totalResidualError;
	putText(image, stringStream.str(), Point(5, 240),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Error max.: " << maxError;
	putText(image, stringStream.str(), Point(5, 260),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Error min.: " << minError;
	putText(image, stringStream.str(), Point(5, 280),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);

	stringStream.str("");
	stringStream << "Error avg.: " << (averageError += totalResidualError) / framesNumber;
	putText(image, stringStream.str(), Point(5, 300),
		FONT_HERSHEY_COMPLEX_SMALL, 0.8, CV_RGB(0, 0, 0), 1, CV_AA);
}