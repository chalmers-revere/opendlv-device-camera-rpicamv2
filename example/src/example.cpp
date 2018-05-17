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

int32_t main(int32_t argc, char **argv) {
  int32_t retCode{0};
  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if ((0 == commandlineArguments.count("name")) || (0 == commandlineArguments.count("cid")) || (0 == commandlineArguments.count("traincnn"))) {
    std::cerr << argv[0] << " accesses video data using shared memory provided using the command line parameter --name=." << std::endl;
    std::cerr << "Usage:   " << argv[0] << " --cid=<OpenDaVINCI session> --name=<name for the associated shared memory> [--id=<sender stamp>] [--verbose]" << std::endl;
    std::cerr << "         --name:    name of the shared memory to use" << std::endl;
    std::cerr << "         --traincnn: set 1 or 0 for training the tiny dnn example and saving a net binary" << std::endl;
    std::cerr << "         --verbose: when set, a thumbnail of the image contained in the shared memory is sent" << std::endl;
    std::cerr << "Example: " << argv[0] << " --cid=111 --name=cam0 --traincnn=1" << std::endl;
    retCode = 1;
  } else {
    bool const VERBOSE{commandlineArguments.count("verbose") != 0};
    bool const TRAINCNN{std::stoi(commandlineArguments["cid"]) == 1};
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


      // TINYDNN EXAMPLE: https://github.com/tiny-dnn/tiny-dnn/blob/master/examples/sinus_fit/sinus_fit.cpp

      if (TRAINCNN) {
        tiny_dnn::network<tiny_dnn::sequential> net;
        net << tiny_dnn::fully_connected_layer(1, 10);
        net << tiny_dnn::tanh_layer();
        net << tiny_dnn::fully_connected_layer(10, 10);
        net << tiny_dnn::tanh_layer();
        net << tiny_dnn::fully_connected_layer(10, 1);

        // create input and desired output on a period
        std::vector<tiny_dnn::vec_t> X;
        std::vector<tiny_dnn::vec_t> sinusX;
        for (float x = -3.1416f; x < 3.1416f; x += 0.2f) {
          tiny_dnn::vec_t vx    = {x};
          tiny_dnn::vec_t vsinx = {sinf(x)};

          X.push_back(vx);
          sinusX.push_back(vsinx);
        }

        // set learning parameters
        size_t batch_size = 16;    // 16 samples for each network weight update
        int epochs        = 2000;  // 2000 presentation of all samples
        tiny_dnn::adamax opt;

        // this lambda function will be called after each epoch
        int iEpoch              = 0;
        auto on_enumerate_epoch = [&]() {
          // compute loss and disp 1/100 of the time
          iEpoch++;
          if (iEpoch % 100) return;

          double loss = net.get_loss<tiny_dnn::mse>(X, sinusX);
          std::cout << "epoch=" << iEpoch << "/" << epochs << " loss=" << loss
                    << std::endl;
        };

        // learn
        std::cout << "learning the sinus function with 2000 epochs:" << std::endl;
        net.fit<tiny_dnn::mse>(opt, X, sinusX, batch_size, epochs, []() {},
                               on_enumerate_epoch);

        std::cout << std::endl
                  << "Training finished, now computing prediction results:"
                  << std::endl;
        net.save("net");


      }
      tiny_dnn::network<tiny_dnn::sequential> net2;
      net2.load("net");
      // compare prediction and desired output
      float fMaxError = 0.f;
      for (float x = -3.1416f; x < 3.1416f; x += 0.2f) {
        tiny_dnn::vec_t xv = {x};
        float fPredicted   = net2.predict(xv)[0];
        float fDesired     = sinf(x);

        std::cout << "x=" << x << " sinX=" << fDesired
                  << " predicted=" << fPredicted << std::endl;

        // update max error
        float fError = std::fabs(fPredicted - fDesired);

        if (fMaxError < fError) fMaxError = fError;
      }

      std::cout << std::endl << "max_error=" << fMaxError << std::endl;



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

