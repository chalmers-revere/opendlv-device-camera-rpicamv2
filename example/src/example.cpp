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

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

int32_t main(int32_t argc, char **argv) {
  int32_t retCode{0};
  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if ((0 == commandlineArguments.count("name")) || (0 == commandlineArguments.count("cid") || (0 == commandlineArguments.count("verbose")))) {
    std::cerr << argv[0] << " accesses video data using shared memory provided using the command line parameter --name=." << std::endl;
    std::cerr << "Usage:   " << argv[0] << " --cid=<OpenDaVINCI session> --name=<name for the associated shared memory> [--verbose]" << std::endl;
    std::cerr << "         --name:    name of the shared memory to use" << std::endl;
    std::cerr << "         --verbose: when set, the image contained in the shared memory is displayed" << std::endl;
    std::cerr << "Example: " << argv[0] << " --cid=111 --name=cam0" << std::endl;
    retCode = 1;
  } else {
    int32_t VERBOSE{commandlineArguments.count("verbose") != 0};
    if (VERBOSE) {
      VERBOSE = std::stoi(commandlineArguments["verbose"]);
    }


    uint32_t const WIDTH = 848;
    uint32_t const HEIGHT = 480;
    uint32_t const BPP = 24;

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
      if (VERBOSE == 2) {
        cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE);
      }
      uint32_t i = 0;
      while (od4.isRunning()) {
        // Some bug makes us freeze, and the producer freezes as well..
        sharedMemory->wait();
	      
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));
        sharedMemory->lock();
        if (VERBOSE == 1) {
          cv::Mat mat = cv::cvarrToMat(image);
          cv::cvtColor(mat, mat, CV_RGB2BGR);
          std::string const FILENAME = std::to_string(i) + ".jpg";
          cv::imwrite(FILENAME, mat);
          std::this_thread::sleep_for(std::chrono::seconds(1));
          i++;
          
        } else if (VERBOSE == 2) {
          cv::Mat mat = cv::cvarrToMat(image);
          cv::cvtColor(mat, mat, CV_RGB2BGR);
          cv::imshow("Dispay window", mat);
          cv::waitKey(1);
        }

        sharedMemory->unlock();
      }

      cvReleaseImageHeader(&image);
    } else {
      std::cerr << argv[0] << ": Failed to access shared memory '" << NAME << "'." << std::endl;
    }
  }
  return retCode;
}

