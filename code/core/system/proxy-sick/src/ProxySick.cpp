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

#include <stdint.h>

#include <iostream>
#include <string>
#include <vector>

#include <opendavinci/odcore/base/KeyValueConfiguration.h>
#include <opendavinci/odcore/data/TimeStamp.h>
#include <opendavinci/odcore/wrapper/SerialPort.h>
#include <opendavinci/odcore/wrapper/SerialPortFactory.h>
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
  m_sickStringDecoder = unique_ptr<SickStringDecoder>(new SickStringDecoder(getConference(), x, y, z));

  // Connection configuration.
  const string SERIAL_PORT = kv.getValue<string>("proxy-sick.port");
  const uint32_t BAUD_RATE = 9600; // Fixed baud rate.
  try {
    m_sick = shared_ptr<odcore::wrapper::SerialPort>(odcore::wrapper::SerialPortFactory::createSerialPort(SERIAL_PORT, BAUD_RATE));
    m_sick->setStringListener(m_sickStringDecoder.get());
    m_sick->start();

    stringstream sstrInfo;
    sstrInfo << "[" << getName() << "] Connected to SICK, waiting for configuration (takes approx. 30s)..." << endl;
    toLogger(odcore::data::LogMessage::LogLevel::INFO, sstrInfo.str());
  }
  catch (string &exception) {
    stringstream sstrWarning;
    sstrWarning << "[" << getName() << "] Could not connect to SICK: " << exception << endl;
    toLogger(odcore::data::LogMessage::LogLevel::WARN, sstrWarning.str());
  }
}

void ProxySick::tearDown()
{
  stopScan();
  m_sick->stop();
  m_sick->setStringListener(NULL);
}

// This method will do the main data processing job.
odcore::data::dmcp::ModuleExitCodeMessage::ModuleExitCode ProxySick::body()
{
  // Initialization sequence.
  uint32_t counter = 0;
  while (getModuleStateAndWaitForRemainingTimeInTimeslice() == odcore::data::dmcp::ModuleStateMessage::RUNNING) {
    counter++;
    if (counter == 30) {
      cout << "Sending stop scan" << endl;
      stopScan();
    }
    if (counter == 32) {
      cout << "Sending status request" << endl;
      status();
    }
    if (counter == 34) {
      cout << "Sending settings mode" << endl;
      settingsMode();
    }
    if (counter == 38) {
      cout << "Sending centimeter mode" << endl;
      setCentimeterMode();
    }
    if (counter == 40) {
      cout << "Start scanning" << endl;
      startScan();
      break;
    }
  }

  // "Do nothing" sequence in general.
  while (getModuleStateAndWaitForRemainingTimeInTimeslice() == odcore::data::dmcp::ModuleStateMessage::RUNNING) {
    // Do nothing.
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
  const unsigned char streamStart[] = {0x02, 0x00, 0x02, 0x00, 0x20, 0x24, 0x34, 0x08};
  const string startString(reinterpret_cast<char const *>(streamStart), 8);
  m_sick->send(startString);
}

void ProxySick::stopScan()
{
  const unsigned char streamStop[] = {0x02, 0x00, 0x02, 0x00, 0x20, 0x25, 0x35, 0x08};
  const string stopString(reinterpret_cast<char const *>(streamStop), 8);
  m_sick->send(stopString);
}

void ProxySick::settingsMode()
{
  const unsigned char settingsMode[] = {0x02, 0x00, 0x0A, 0x00, 0x20, 0x00, 0x53, 0x49, 0x43, 0x4B, 0x5F, 0x4C, 0x4D, 0x53, 0xBE, 0xC5};
  const string settingString(reinterpret_cast<char const *>(settingsMode), 16);
  m_sick->send(settingString);
}

void ProxySick::setCentimeterMode()
{
  const unsigned char centimeterMode[] = {0x02, 0x00, 0x21, 0x00, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0xCB};
  const string centimeterString(reinterpret_cast<char const *>(centimeterMode), 39);
  m_sick->send(centimeterString);
}

}
}
}
} // opendlv::core::system::proxy
