/**
 * proxy-sick - Interface to Sick.
 * Copyright (C) 2016 Chalmers REVERE
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


#include <iostream>
#include <string>
#include <vector>

#include <opendavinci/odcore/base/KeyValueConfiguration.h>
#include <opendavinci/odcore/base/Lock.h>
#include <opendavinci/odcore/data/TimeStamp.h>
#include <opendavinci/odcore/wrapper/SerialPort.h>
#include <opendavinci/odcore/wrapper/SerialPortFactory.h>
#include <opendavinci/odcore/wrapper/SharedMemoryFactory.h>
#include <opendavinci/odcore/strings/StringToolbox.h>
#include <opendavinci/odtools/recorder/Recorder.h>

#include "ProxySick.h"

namespace opendlv {
namespace core {
namespace system {
namespace proxy {

using namespace std;
using namespace odcore::base;
using namespace odcore::base::module;

ProxySick::ProxySick(const int &argc, char **argv)
    : TimeTriggeredConferenceClientModule(argc, argv, "proxy-sick")
    , m_sick()
    , m_sickStringDecoder()
    , m_serialPort()
    , m_segment(NULL)
    , m_initialized(false)
    , m_hasAttachedToSharedImageMemory(false)
    , m_spcSharedMemory(NULL)
    , m_spc()
    , m_debug()
{
}

ProxySick::~ProxySick()
{
}

void ProxySick::setUp()
{
  KeyValueConfiguration kv = getKeyValueConfiguration();

  // Mounting configuration.
  const double x = kv.getValue<float>("proxy-sick.mount.x");
  const double y = kv.getValue<float>("proxy-sick.mount.y");
  const double z = kv.getValue<float>("proxy-sick.mount.z");


  const std::string NAME = 
      kv.getValue<std::string>("proxy-sick.type");
  const uint32_t MEMORY_SIZE = 
      kv.getValue<uint32_t>("proxy-sick.sharedMemory-size");
  std::shared_ptr<odcore::wrapper::SharedMemory> sickSharedMemory = 
      odcore::wrapper::SharedMemoryFactory::createSharedMemory(
          NAME, MEMORY_SIZE);
  odcore::data::SharedPointCloud sharedPointCloud;
  

  sharedPointCloud.setName(NAME);
  sharedPointCloud.setSize(MEMORY_SIZE);
  sharedPointCloud.setHeight(1); // Why should this be 1?
  sharedPointCloud.setWidth(361);
  sharedPointCloud.setNumberOfComponentsPerPoint(4);
  sharedPointCloud.setComponentDataType(
      odcore::data::SharedPointCloud::FLOAT_T); // Data type per component.
  sharedPointCloud.setUserInfo(odcore::data::SharedPointCloud::XYZ_INTENSITY);

  //Create memory for temporary storage of point cloud data for each frame
  m_segment = (float *)malloc(MEMORY_SIZE);

  m_sickStringDecoder = unique_ptr<SickStringDecoder>(
      new SickStringDecoder(getConference(), sickSharedMemory, 
          sharedPointCloud, m_segment, x, y, z));

  // Connection configuration.
  m_serialPort = kv.getValue<string>("proxy-sick.serial-port");
  const uint32_t INIT_BAUD_RATE = 9600; // Fixed baud rate.
  openSerialPort(m_serialPort, INIT_BAUD_RATE);
  m_debug = (kv.getValue<int32_t>("proxy-sick.debug") == 1);
}

void ProxySick::tearDown()
{
  free(m_segment);
  stopScan();
  m_sick->stop();
  m_sick->setStringListener(NULL);
}

void ProxySick::nextContainer(odcore::data::Container &a_c)
{
  if (!m_initialized) {
    return;
  }
  if (m_debug && a_c.getDataType() == odcore::data::SharedPointCloud::ID()){
    // m_SPCReceived = true;
    odcore::data::SharedPointCloud spc = 
        a_c.getData<odcore::data::SharedPointCloud>();
    if (!m_hasAttachedToSharedImageMemory) {
      m_spc = spc;
      m_spcSharedMemory = 
          odcore::wrapper::SharedMemoryFactory::attachToSharedMemory(
              spc.getName()); 
      m_hasAttachedToSharedImageMemory = true;
      std::cout << "Attached to shared point cloud memory." << std::endl;
    }  
  }
}

// This method will do the main data processing job.
odcore::data::dmcp::ModuleExitCodeMessage::ModuleExitCode ProxySick::body()
{
  // Initialization sequence.
  uint32_t counter = 0;
  while (getModuleStateAndWaitForRemainingTimeInTimeslice() == 
      odcore::data::dmcp::ModuleStateMessage::RUNNING) {
    counter++;
    if (counter == 1) {
      cout << "Sending stop scan" << endl;
      stopScan();
    }
    if (counter == 3) {
      cout << "Sending status request" << endl;
      status();
    }
    if (counter == 14) {
      cout << "Changing baudrate to 38400" << endl;
      setBaudrate38400();
    }
    if (counter == 15) {
      cout << "Reconnecting with new baudrate" << endl;
      m_sick->stop();
      m_sick->setStringListener(NULL);
      openSerialPort(m_serialPort, 38400);
    }
    if (counter == 17) {
      cout << "Sending settings mode" << endl;
      settingsMode();
    }
    if (counter == 21) {
      cout << "Sending centimeter mode" << endl;
      setCentimeterMode();
    }
    if (counter == 31) {
      cout << "Start scanning" << endl;
      startScan();
      m_initialized = true;
      break;
    }
  }

  while (getModuleStateAndWaitForRemainingTimeInTimeslice() == 
      odcore::data::dmcp::ModuleStateMessage::RUNNING) {
    if (m_debug && m_hasAttachedToSharedImageMemory  
        && m_spcSharedMemory != NULL && m_spcSharedMemory->isValid()) {
      odcore::base::Lock l(m_spcSharedMemory);
      float *sickRawData = 
          static_cast<float*>(m_spcSharedMemory->getSharedMemory());
      uint32_t startID = 0;
      for (uint32_t i = 0; i < m_spc.getWidth(); i++) {
        std::cout << "(x,y,z,intensityLevel): " 
        << std::to_string(sickRawData[startID]) 
        << "," << std::to_string(sickRawData[startID + 1]) 
        << "," << std::to_string(sickRawData[startID + 2]) 
        << "," << std::to_string(sickRawData[startID + 3])
        << std::endl; 
        startID += m_spc.getNumberOfComponentsPerPoint();
      }
    }
  }
  return odcore::data::dmcp::ModuleExitCodeMessage::OKAY;
}

void ProxySick::status()
{
  const unsigned char statusCall[] = {0x02, 0x00, 0x01, 0x00, 0x31, 0x15, 0x12};
  const string statusString(reinterpret_cast<char const *>(statusCall), 7);
  m_sick->send(statusString);
}

void ProxySick::startScan()
{
  const unsigned char streamStart[] = 
      {0x02, 0x00, 0x02, 0x00, 0x20, 0x24, 0x34, 0x08};
  const string startString(reinterpret_cast<char const *>(streamStart), 8);
  m_sick->send(startString);
}

void ProxySick::stopScan()
{
  const unsigned char streamStop[] = 
      {0x02, 0x00, 0x02, 0x00, 0x20, 0x25, 0x35, 0x08};
  const string stopString(reinterpret_cast<char const *>(streamStop), 8);
  m_sick->send(stopString);
}

void ProxySick::settingsMode()
{
  const unsigned char settingsModeString[] = 
      {0x02, 0x00, 0x0A, 0x00, 0x20, 0x00, 0x53, 0x49, 0x43, 0x4B, 0x5F, 0x4C, 
          0x4D, 0x53, 0xBE, 0xC5};
  const string settingString(
      reinterpret_cast<char const *>(settingsModeString), 16);
  m_sick->send(settingString);
}

void ProxySick::setCentimeterMode()
{
  const unsigned char centimeterMode[] = 
      {0x02, 0x00, 0x21, 0x00, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 
          0x00, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
          0x00, 0x00, 0x00, 0x20, 0xCB};
  const string centimeterString(
      reinterpret_cast<char const *>(centimeterMode), 39);
  m_sick->send(centimeterString);
}

void ProxySick::setBaudrate38400()
{
  const unsigned char baudrate38400[] = 
      {0x02, 0x00, 0x02, 0x00, 0x20, 0x40, 0x50, 0x08};
  const string baudrate38400String(
      reinterpret_cast<char const *>(baudrate38400), 8);
  m_sick->send(baudrate38400String); 
}

void ProxySick::openSerialPort(std::string a_serialPort, uint32_t a_baudRate)
{
  try {
    m_sick = 
        shared_ptr<odcore::wrapper::SerialPort>(
            odcore::wrapper::SerialPortFactory::createSerialPort(
                a_serialPort, a_baudRate));
    m_sick->setStringListener(m_sickStringDecoder.get());
    m_sick->start();

    cout << "[" << getName() 
    << "] Connected to SICK, waiting for configuration (takes approx. 30s)..." 
    << endl;
    // toLogger(odcore::data::LogMessage::LogLevel::INFO, sstrInfo.str());
  }
  catch (string &exception) {
    cout << "[" << getName() 
    << "] Could not connect to SICK: " << exception 
    << endl;
  }
}

}
}
}
} // opendlv::core::system::proxy
