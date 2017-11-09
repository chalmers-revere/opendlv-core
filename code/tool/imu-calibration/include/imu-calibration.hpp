/**
 * imu-calibration - Tool to find calibration matrix of IMU.
 * Copyright (C) 2016 Chalmers Revere
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

#ifndef CORE_TOOL_IMUCALIBRATION_HPP_
#define CORE_TOOL_IMUCALIBRATION_HPP_

#include <map>
#include <memory>


#include <opendavinci/odcore/base/module/TimeTriggeredConferenceClientModule.h>
#include <opendavinci/odcore/data/Container.h>


#include "imu-calibration.hpp"

namespace opendlv {
namespace core {
namespace tool {

class IMUCalibration
: public odcore::base::module::TimeTriggeredConferenceClientModule{
 public:
  IMUCalibration(int32_t const &, char **);
  IMUCalibration(MoveSteppermotor const &) = delete;
  IMUCalibration &operator=(MoveSteppermotor const &) = delete;
  virtual ~IMUCalibration();

 private:
  void setUp();
  void tearDown();

  odcore::data::dmcp::ModuleExitCodeMessage::ModuleExitCode body();

};

} // tools
} // core
} // opendlv

#endif
