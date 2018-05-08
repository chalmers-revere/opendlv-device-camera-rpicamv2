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

#include <chrono>
#include <iostream>
#include <thread>

#include <raspicam/raspicam.h>

#include "cluon-complete.hpp"


int32_t main(int32_t argc, char **argv) {
  int32_t retCode{0};
  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if ((0 == commandlineArguments.count("cid"))) {
    std::cerr << argv[0] << " interfaces with the Raspberry Pi camera module V2 and publishes the video stream to a running OD4 session." << std::endl;
    std::cerr << "Usage:   " << argv[0] << " --cid=<OpenDaVINCI session> [--name=<unique name for the associated shared memory>]" << std::endl;
    std::cerr << "         --name:    when omitted, '/cam0' is chosen" << std::endl;
    std::cerr << "Example: " << argv[0] << " --cid=111 --name=cam0" << std::endl;
    retCode = 1;
  } else {
    std::string const NAME{(commandlineArguments["name"].size() != 0) ? commandlineArguments["name"] : "/cam0"};
        
    raspicam::RaspiCam camera;

    if (!camera.open()) {
      std::cerr << "Could not open camera." << std::endl;
      return retCode = 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    cluon::OD4Session od4{static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};

    uint32_t const SIZE = camera.getImageTypeSize(raspicam::RASPICAM_FORMAT_BGR);
    std::unique_ptr<cluon::SharedMemory> sharedMemory(new cluon::SharedMemory{NAME, SIZE});
    if (sharedMemory && sharedMemory->valid()) {
      std::clog << argv[0] << ": Data from camera available in shared memory '" << sharedMemory->name() << "' (" << sharedMemory->size() << ")." << std::endl;

      while (od4.isRunning()) {
        camera.grab();

        sharedMemory->lock();
        camera.retrieve(reinterpret_cast<unsigned char*>(sharedMemory->data()), raspicam::RASPICAM_FORMAT_IGNORE);
        sharedMemory->unlock();
        sharedMemory->notifyAll();
      }
    } else {
      std::cerr << argv[0] << ": Failed to create shared memory '" << NAME << "'." << std::endl;
    }
    return retCode;
  }
}

