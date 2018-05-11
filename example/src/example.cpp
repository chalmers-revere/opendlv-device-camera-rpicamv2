/*
 * Copyright (C) 2018 Ola Benderius
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"

#include "tiny_dnn/tiny_dnn.h"
using namespace tiny_dnn;
using namespace tiny_dnn::layers;
using namespace tiny_dnn::activation;

int32_t main(int32_t argc, char **argv) {
  int32_t retCode{0};
  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if ((0 == commandlineArguments.count("name")) || (0 == commandlineArguments.count("cid"))) {
    std::cerr << argv[0] << " accesses video data using shared memory provided using the command line parameter --name=." << std::endl;
    std::cerr << "Usage:   " << argv[0] << " --cid=<OpenDaVINCI session> --name=<name for the associated shared memory> [--id=<sender stamp>] [--verbose]" << std::endl;
    std::cerr << "         --name:    name of the shared memory to use" << std::endl;
    std::cerr << "         --verbose: when set, a thumbnail of the image contained in the shared memory is sent" << std::endl;
    std::cerr << "Example: " << argv[0] << " --cid=111 --name=cam0" << std::endl;
    retCode = 1;
  } else {
    bool const VERBOSE{commandlineArguments.count("verbose") != 0};
    uint32_t const WIDTH{1280};
    uint32_t const HEIGHT{960};
    uint32_t const BPP{24};
    uint32_t const ID{(commandlineArguments["id"].size() != 0) ? static_cast<uint32_t>(std::stoi(commandlineArguments["id"])) : 0};

    std::string const NAME{(commandlineArguments["name"].size() != 0) ? commandlineArguments["name"] : "/cam0"};
    cluon::OD4Session od4{static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};

    std::unique_ptr<cluon::SharedMemory> sharedMemory(new cluon::SharedMemory{NAME});
    if (sharedMemory && sharedMemory->valid()) {
      std::clog << argv[0] << ": Found shared memory '" << sharedMemory->name() << "' (" << sharedMemory->size() << " bytes)." << std::endl;

      CvSize size;
      size.width = WIDTH;
      size.height = HEIGHT;

      IplImage *image = cvCreateImageHeader(size, IPL_DEPTH_8U, BPP/8);
      sharedMemory->lock();
      image->imageData = sharedMemory->data();
      image->imageDataOrigin = image->imageData;
      sharedMemory->unlock();
      int32_t i = 0;
      while (od4.isRunning()) {
        sharedMemory->wait();

        // Make a scaled copy of the original image.
        int32_t const width = 256;
        int32_t const height = 196;
        cv::Mat scaledImage;
        {
          sharedMemory->lock();
          cv::Mat sourceImage = cv::cvarrToMat(image, false);
          cv::resize(sourceImage, scaledImage, cv::Size(width, height), 0, 0, cv::INTER_NEAREST);
          sharedMemory->unlock();
        }


        
        // Start working with the image

        // For example use tinyDNN.
        /*
        network<sequential> net;
        net << conv(32, 32, 5, 1, 6, padding::same) << tanh()  // in:32x32x1, 5x5conv, 6fmaps
          << max_pool(32, 32, 6, 2) << tanh()                // in:32x32x6, 2x2pooling
          << conv(16, 16, 5, 6, 16, padding::same) << tanh() // in:16x16x6, 5x5conv, 16fmaps
          << max_pool(16, 16, 16, 2) << tanh()               // in:16x16x16, 2x2pooling
          << fc(8*8*16, 100) << tanh()                       // in:8x8x16, out:100
          << fc(100, 10) << softmax();                       // in:100 out:10
        */

        // Make an estimation.
        float estimatedDetectionAngle = 0.0f;
        float estimatedDetectionDistance = 0.0f;
        if (VERBOSE) {
          std::string const FILENAME = std::to_string(i) + ".jpg";
          cv::imwrite(FILENAME, scaledImage);
          i++;
          std::this_thread::sleep_for(std::chrono::seconds(1));
          std::cout << "The target was found at angle " << estimatedDetectionAngle 
            << " at distance " << estimatedDetectionDistance << std::endl;
        }

        // In the end, send a message that is received by the control logic.
        opendlv::logic::sensation::Point detection;
        detection.azimuthAngle(estimatedDetectionAngle);
        detection.distance(estimatedDetectionDistance);

        od4.send(detection, cluon::time::now(), ID);
      }

      cvReleaseImageHeader(&image);
    } else {
      std::cerr << argv[0] << ": Failed to access shared memory '" << NAME << "'." << std::endl;
    }
  }
  return retCode;
}

