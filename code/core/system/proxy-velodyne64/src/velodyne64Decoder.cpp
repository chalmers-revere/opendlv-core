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

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "opendavinci/GeneratedHeaders_OpenDaVINCI.h"
#include "opendavinci/generated/odcore/data/SharedPointCloud.h"
#include "opendavinci/odcore/base/Lock.h"
#include "opendavinci/odcore/data/Container.h"
#include "opendavinci/odcore/io/conference/ContainerConference.h"
#include "opendavinci/odcore/wrapper/SharedMemory.h"
#include "opendavinci/odcore/wrapper/SharedMemoryFactory.h"

#include "velodyne64Decoder.h"


#define toRadian(x) ((x)*PI / 180.0f)

namespace opendlv {
namespace core {
namespace system {
namespace proxy {

using namespace std;
using namespace odcore::base;
using namespace odcore::data;
using namespace odcore::wrapper;

velodyne64Decoder::velodyne64Decoder(const std::shared_ptr< SharedMemory > m,
odcore::io::conference::ContainerConference &c, const string &s)
    : pointIndex(0)
    , startID(0)
    , previousAzimuth(0.0)
    , upperBlock(true)
    , distance(0.0)
    , VelodyneSharedMemory(m)
    , segment(NULL)
    , velodyneFrame(c)
    , spc()
    , calibration(s) {
    //Initial setup of the shared point cloud
    spc.setName(VelodyneSharedMemory->getName()); // Name of the shared memory segment with the data.
    //spc.setSize(pointIndex* NUMBER_OF_COMPONENTS_PER_POINT * SIZE_PER_COMPONENT); // Size in raw bytes.
    //spc.setWidth(pointIndex); // Number of points.
    spc.setHeight(1); // We have just a sequence of vectors.
    spc.setNumberOfComponentsPerPoint(NUMBER_OF_COMPONENTS_PER_POINT);
    spc.setComponentDataType(SharedPointCloud::FLOAT_T); // Data type per component.
    spc.setUserInfo(SharedPointCloud::XYZ_INTENSITY);

    //Create memory for temporary storage of point cloud data for each frame
    segment = (float *)malloc(SIZE);

    //Load calibration data from the calibration file
    string line;
    ifstream in(calibration);
    int counter[5] = {0, 0, 0, 0, 0}; //corresponds to the index of the five calibration values
    bool found[5] = {false, false, false, false, false};

    while (getline(in, line)) {
        string tmp; // strip whitespaces from the beginning
        for (unsigned int i = 0; i < line.length(); i++) {

            if ((line[i] == '\t' || line[i] == ' ') && tmp.size() == 0) {
            } else {
                if (line[i] == '<') {
                    if (found[0]) {
                        rotCorrection[counter[0]] = atof(tmp.c_str());
                        counter[0]++;
                        found[0] = false;
                        continue;
                    }

                    if (found[1]) {
                        vertCorrection[counter[1]] = atof(tmp.c_str());
                        counter[1]++;
                        found[1] = false;
                        continue;
                    }

                    if (found[2]) {
                        distCorrection[counter[2]] = atof(tmp.c_str());
                        counter[2]++;
                        found[2] = false;
                        continue;
                    }

                    if (found[3]) {
                        vertOffsetCorrection[counter[3]] = atof(tmp.c_str());
                        counter[3]++;
                        found[3] = false;
                        continue;
                    }

                    if (found[4]) {
                        horizOffsetCorrection[counter[4]] = atof(tmp.c_str());
                        counter[4]++;
                        found[4] = false;
                        continue;
                    }
                    tmp += line[i];
                } else {
                    tmp += line[i];
                }
            }

            if (tmp == "<rotCorrection_>") {
                found[0] = true;
                tmp = "";
            } else if (tmp == "<vertCorrection_>") {
                found[1] = true;
                tmp = "";
            } else if (tmp == "<distCorrection_>") {
                found[2] = true;
                tmp = "";
            } else if (tmp == "<vertOffsetCorrection_>") {
                found[3] = true;
                tmp = "";
            } else if (tmp == "<horizOffsetCorrection_>") {
                found[4] = true;
                tmp = "";
            } else {
            }
        }
    }
}

velodyne64Decoder::~velodyne64Decoder() {
    free(segment);
}

//Update the shared point cloud when a complete scan is completed.
void velodyne64Decoder::sendSPC(const float &oldAzimuth, const float &newAzimuth) {
    if (newAzimuth < oldAzimuth) {
        if (VelodyneSharedMemory->isValid()) {
            Lock l(VelodyneSharedMemory);
            memcpy(VelodyneSharedMemory->getSharedMemory(), segment, SIZE);
            //spc.setName(VelodyneSharedMemory->getName()); // Name of the shared memory segment with the data.
            spc.setSize(pointIndex * NUMBER_OF_COMPONENTS_PER_POINT * SIZE_PER_COMPONENT); // Size in raw bytes.
            spc.setWidth(pointIndex);                                                      // Number of points.
            //spc.setHeight(1); // We have just a sequence of vectors.
            //spc.setNumberOfComponentsPerPoint(NUMBER_OF_COMPONENTS_PER_POINT);
            //spc.setComponentDataType(SharedPointCloud::FLOAT_T); // Data type per component.
            //spc.setUserInfo(SharedPointCloud::XYZ_INTENSITY);

            Container imageFrame(spc);
            velodyneFrame.send(imageFrame);
        }
        pointIndex = 0;
        startID = 0;
    }
}

void velodyne64Decoder::nextString(const string &payload) {
    if (payload.length() == 1206) {
        //Decode HDL-64E data
        uint32_t position = 0; //position specifies the starting position to read from the 1206 bytes

        //The payload of a HDL-64E packet consists of 12 blocks with 100 bytes each. Decode each block separately.
        static uint8_t firstByte, secondByte;
        static uint32_t dataValue;
        for (int index = 0; index < 12; index++) {
            //Decode header information: 2 bytes
            firstByte = (uint8_t)(payload.at(position));
            secondByte = (uint8_t)(payload.at(position + 1));
            dataValue = ntohs(firstByte * 256 + secondByte);
            if (dataValue == 0xddff) {
                upperBlock = false; //Lower block
            } else {
                upperBlock = true; //upper block
            }

            //Decode rotational information: 2 bytes
            firstByte = (uint8_t)(payload.at(position + 2));
            secondByte = (uint8_t)(payload.at(position + 3));
            dataValue = ntohs(firstByte * 256 + secondByte);
            float azimuth = static_cast< float >(dataValue / 100.0);
            sendSPC(previousAzimuth, azimuth); //Send a complete scan as one frame
            previousAzimuth = azimuth;
            position += 4;

            if (pointIndex < MAX_POINT_SIZE) {
                //Decode distance information and intensity of each sensor in a block
                for (int counter = 0; counter < 32; counter++) {
                    //Decode distance: 2 bytes
                    static uint8_t sensorID(0);
                    if (upperBlock)
                        sensorID = counter;
                    else
                        sensorID = counter + 32;
                    firstByte = (uint8_t)(payload.at(position));
                    secondByte = (uint8_t)(payload.at(position + 1));
                    dataValue = ntohs(firstByte * 256 + secondByte);
                    distance = static_cast< float >(dataValue * 0.2f / 100.0f) + distCorrection[sensorID] / 100.0f;
                    if (distance > 1.0f) {
                        static float xyDistance, xData, yData, zData, intensity;
                        xyDistance = distance * cos(toRadian(vertCorrection[sensorID]));
                        xData = xyDistance * sin(toRadian(azimuth - rotCorrection[sensorID])) - horizOffsetCorrection[sensorID] / 100.0f * cos(toRadian(azimuth - rotCorrection[sensorID]));
                        yData = xyDistance * cos(toRadian(azimuth - rotCorrection[sensorID])) + horizOffsetCorrection[sensorID] / 100.0f * sin(toRadian(azimuth - rotCorrection[sensorID]));
                        zData = distance * sin(toRadian(vertCorrection[sensorID])) + vertOffsetCorrection[sensorID] / 100.0f;
                        //Decode intensity: 1 byte
                        uint8_t intensityInt = (uint8_t)(payload.at(position + 2));
                        intensity = (float)intensityInt;

                        //Store coordinate information of each point to the malloc memory
                        segment[startID] = xData;
                        segment[startID + 1] = yData;
                        segment[startID + 2] = zData;
                        segment[startID + 3] = intensity;

                        pointIndex++;
                        startID += NUMBER_OF_COMPONENTS_PER_POINT;
                    }
                    position += 3;

                    if (pointIndex >= MAX_POINT_SIZE) {
                        position += 3 * (31 - counter); //Discard the points of the current frame when the preallocated shared memory is full; move the position to be read in the 1206 bytes
                        break;
                    }
                }
            } else {
                position += 96; //32*3
            }
        }
    }
}
}
}
}
} // opendlv::core::system::proxy
