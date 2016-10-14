/**
 * velodyne64Decoder is used to decode Velodyne HDL-64E data realized with OpenDaVINCI
 * Copyright (C) 2016 Hang Yin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef VELODYNE64DECODER_H_
#define VELODYNE64DECODER_H_

#include <memory>

#include "opendavinci/generated/odcore/data/SharedPointCloud.h"
#include "opendavinci/odcore/data/Container.h"
#include "opendavinci/odcore/wrapper/SharedMemory.h"
#include <opendavinci/odcore/io/StringListener.h>

namespace opendlv {
namespace core {
namespace system {
namespace proxy {

using namespace std;
using namespace odcore::wrapper;

// This class will handle bytes received via a UDP socket.
class velodyne64Decoder : public odcore::io::StringListener {
   private:
    /**
                 * "Forbidden" copy constructor. Goal: The compiler should warn
                 * already at compile time for unwanted bugs caused by any misuse
                 * of the copy constructor.
                 */
    velodyne64Decoder(const velodyne64Decoder &);

    /**
                 * "Forbidden" assignment operator. Goal: The compiler should warn
                 * already at compile time for unwanted bugs caused by any misuse
                 * of the assignment operator.
                 */
    velodyne64Decoder &operator=(const velodyne64Decoder &);


   public:
    velodyne64Decoder(const std::shared_ptr< SharedMemory >, odcore::io::conference::ContainerConference &, const string &);

    virtual ~velodyne64Decoder();

    void sendSPC(const float &oldAzimuth, const float &newAzimuth);

    virtual void nextString(const std::string &s);

   private:
    const uint32_t MAX_POINT_SIZE = 101000; //the maximum number of points per frame. This upper bound should be set as low as possible, as it affects the shared memory size and thus the frame updating speed. speed.
    const uint32_t SIZE_PER_COMPONENT = sizeof(float);
    const uint8_t NUMBER_OF_COMPONENTS_PER_POINT = 4;                                           // How many components do we have per vector?
    const uint32_t SIZE = MAX_POINT_SIZE * NUMBER_OF_COMPONENTS_PER_POINT * SIZE_PER_COMPONENT; // What is the total size of the shared memory?
    const float PI = 3.14159;

    uint32_t pointIndex;
    uint32_t startID;
    float previousAzimuth;
    bool upperBlock;
    float distance;
    std::shared_ptr< SharedMemory > VelodyneSharedMemory; //shared memory for the shared point cloud
    float *segment;                                       //temporary memory for transferring data of each frame to the shared memory
    odcore::io::conference::ContainerConference &velodyneFrame;
    odcore::data::SharedPointCloud spc;
    float rotCorrection[64];
    float vertCorrection[64];
    float distCorrection[64];
    float vertOffsetCorrection[64];
    float horizOffsetCorrection[64];
    string calibration;
};
}
}
}
} // opendlv::core::system::proxy

#endif /*VELODYNE64DECODER_H_*/
