/**
 * camera-projection - Tool to find projection matrix of camera.
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

#include <ctype.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>
#include <unistd.h>

#include "opendavinci/GeneratedHeaders_OpenDaVINCI.h"
#include "opendavinci/odcore/data/Container.h"
#include "opendavinci/odcore/base/KeyValueConfiguration.h"
#include "opendavinci/odcore/wrapper/SharedMemoryFactory.h"
#include "opendavinci/odcore/wrapper/SharedMemory.h"

#include "cameraprojection.hpp"

namespace opendlv {
namespace core {
namespace tool {

void LogMouseClicks(int32_t a_event, int32_t a_x, int32_t a_y, int32_t,
    void* a_userdata)
{  

  MouseParams* click = (MouseParams*) a_userdata;
  if (a_event == cv::EVENT_LBUTTONDOWN && click->iterator < 4)
  {

    std::cout << "(" << a_x << ", " << a_y << ")" << std::endl;
    click->points(0,click->iterator) = a_x;
    click->points(1,click->iterator) = a_y;
    click->iterator = click->iterator +1;
    std::cout << "Points: " + std::to_string(click->iterator) << std::endl;
    std::cout << (click->points) << std::endl;
  }
  if (a_event == cv::EVENT_RBUTTONDOWN)
  {
    click->iterator = 0;
  }
}

void ProjectMouseClicks(int32_t a_event, int32_t a_x, int32_t a_y, int32_t,
    void* a_userdata)
{
  Eigen::Vector3d v;
  Eigen::MatrixXd* m = (Eigen::MatrixXd*) a_userdata;
  
  if(a_event == cv::EVENT_LBUTTONDOWN){
    v << a_x,a_y,1;
    // std::cout << *m << std::endl;
    // std::cout << v << std::endl;
    v = *m*v;
    v = v / v(2);


    std::cout << v << std::endl;

  } 
}

MouseParams::MouseParams() : 
    points(),
    iterator() 
{
  points = Eigen::MatrixXd(2,4);
  cv::namedWindow("Calibration", 1 );
}

MouseParams::~MouseParams()
{
}


CameraProjection::CameraProjection(int32_t const &a_argc, char **a_argv)
    : odcore::base::module::TimeTriggeredConferenceClientModule(
      a_argc, a_argv, "core-tool-camera-projection"),
      m_recHeight(),
      m_recWidth(),
      m_recPosX(),
      m_recPosY(),
      m_aMatrix(),
      m_bMatrix(),
      m_projectionMatrix(),
      m_point(),
      m_cameraName(),
      m_transformationMatrixFileName(),
      m_initialized(false)
{
  m_aMatrix = Eigen::MatrixXd(3,3);
  m_bMatrix = Eigen::MatrixXd(3,3);
  m_projectionMatrix = Eigen::MatrixXd(3,3);

}

CameraProjection::~CameraProjection()
{
}

void CameraProjection::setUp()
{
  odcore::base::KeyValueConfiguration kv = getKeyValueConfiguration();

  m_cameraName = kv.getValue<std::string>("core-tool-camera-projection.cameraname");
  m_transformationMatrixFileName = m_cameraName + "-pixel2word-matrix.csv";

  cv::namedWindow("Calibration", 1 );

  m_initialized = true;
}

void CameraProjection::tearDown()
{
}


void CameraProjection::nextContainer(odcore::data::Container &a_c)
{
  if(!m_initialized){
    return;
  }
  if(a_c.getDataType() == odcore::data::image::SharedImage::ID()){
    odcore::data::image::SharedImage mySharedImg =
        a_c.getData<odcore::data::image::SharedImage>();
    // std::cout<<mySharedImg.getName()<<std::endl;
    if(mySharedImg.getName().compare(m_cameraName)){
      return;
    }
    std::shared_ptr<odcore::wrapper::SharedMemory> sharedMem(
        odcore::wrapper::SharedMemoryFactory::attachToSharedMemory(
            mySharedImg.getName()));
    const uint32_t nrChannels = mySharedImg.getBytesPerPixel();
    const uint32_t imgWidth = mySharedImg.getWidth();
    const uint32_t imgHeight = mySharedImg.getHeight();

    // std::cout << imgWidth << "    "<< imgHeight << std::endl;

    IplImage* myIplImage;
    
    myIplImage = cvCreateImage(cvSize(imgWidth, imgHeight), IPL_DEPTH_8U,
        nrChannels);
    cv::Mat feed(myIplImage);

    if(!sharedMem->isValid()){
      return;
    }

    sharedMem->lock();
    {
      memcpy(feed.data, sharedMem->getSharedMemory(),
          imgWidth*imgHeight*nrChannels);
    }
    sharedMem->unlock();
    // const int32_t windowWidth = 640;
    // const int32_t windowHeight = 480;
    // cv::Mat display;
    // cv::resize(m_feed, display, cv::Size(windowWidth, windowHeight), 0, 0,
    //   cv::INTER_CUBIC);

    putText(feed, "Rectangle width: " + std::to_string(m_recWidth),
        cvPoint(30,30), 1, 0.8, cvScalar(0,0,254), 1, CV_AA);
    putText(feed, "Rectangle height: " + std::to_string(m_recHeight),
        cvPoint(30,40), 1, 0.8, cvScalar(0,0,254), 1, CV_AA);
    putText(feed, "Position (x,y): (" + std::to_string(m_recPosX) + "," 
        + std::to_string(m_recPosY) + ")" , cvPoint(30,50), 1, 0.8,
        cvScalar(0,0,254), 1, CV_AA);

    cv::imshow("Calibration", feed);

    cvReleaseImage(&myIplImage);
    return;
  }
}

odcore::data::dmcp::ModuleExitCodeMessage::ModuleExitCode CameraProjection::body(){
  while (getModuleStateAndWaitForRemainingTimeInTimeslice() ==
  odcore::data::dmcp::ModuleStateMessage::RUNNING){
    char key = (char) cv::waitKey(1);
    switch(key){
      case 'c':
        std::cout << "Enter Calibration" << std::endl;
        Calibrate();
        break;
      case'r':
        std::cout << "Enter Configuration" << std::endl;
        Config();
        break;
      case 'e':
        std::cout << "Read file" << std::endl;
        ReadMatrix();
        break;
      case's':
        std::cout << "Calculating projection matrix and saving to file" 
            << std::endl;
        Save();
        break;
      case 'p':
        std::cout << "Projecting points" << std::endl;
        Project();
        break;
      case 'q':
      case 27:
        return odcore::data::dmcp::ModuleExitCodeMessage::OKAY;
      default:
        break;
    }
  }
  return odcore::data::dmcp::ModuleExitCodeMessage::OKAY;
}


void CameraProjection::ReadMatrix()
{
  Eigen::MatrixXd m(3,3);
  std::ifstream indata(m_transformationMatrixFileName, std::ifstream::in);
  if(indata.is_open()){
    for(uint8_t i = 0; i < 3; ++i){
      for(uint8_t j = 0; j < 3; ++j){
        double item;
        indata >> item;
        // std::cout<<item<<", ";
        m(i,j) = item;
      }
    }
    std::cout<< m << std::endl;
  }
  else{
    std::cout<< "File not found." << std::endl;
  }
  indata.close();
}

void CameraProjection::Config()
{
  std::cout << "rectangle width: ";
  std::cin >> m_recWidth;
  std::cout << "rectangle height: ";
  std::cin >> m_recHeight;
  std::cout << "x position: ";
  std::cin >> m_recPosX;
  std::cout << "y position: ";
  std::cin >> m_recPosY;
  Eigen::MatrixXd q(3,3), w(3,1);
  q <<  m_recPosX, m_recPosX, m_recPosX+m_recHeight,
        m_recPosY + m_recWidth, m_recPosY, m_recPosY,
        1,1,1;
  std::cout<<q<<std::endl;
  w <<  m_recPosX + m_recHeight,
        m_recPosY + m_recWidth,
        1;
  std::cout << w << std::endl;

  Eigen::Vector3d scale = q.colPivHouseholderQr().solve(w);
  std::cout<<scale<<std::endl;

  m_aMatrix << scale(0)*q.col(0) ,scale(1)*q.col(1), scale(2)*q.col(2);
  std::cout<< m_aMatrix << std::endl;

  std::cout<<"Configuration done." << std::endl;

}

void CameraProjection::Calibrate()
{

  MouseParams mouseClick;
  mouseClick.iterator = 0;
  std::string mode("Calibration");
  cv::setMouseCallback(mode, LogMouseClicks, (void *) &mouseClick);
  cv::waitKey(0);
  cv::setMouseCallback(mode, NULL, NULL);
  if(mouseClick.iterator > 3){
    Eigen::MatrixXd q(3,3), w(3,1);
    q << mouseClick.points(0,0),mouseClick.points(0,1),mouseClick.points(0,2),
        mouseClick.points(1,0),mouseClick.points(1,1),mouseClick.points(1,2),
        1,1,1;
    w << mouseClick.points(0,3), mouseClick.points(1,3), 1;
    // std::cout<<q<< std::endl;
    // std::cout<<w<< std::endl;

    Eigen::Vector3d scale = q.colPivHouseholderQr().solve(w);
    // std::cout<<scale<<std::endl;

    m_bMatrix << scale(0)*q.col(0),scale(1)*q.col(1),scale(2)*q.col(2);
    std::cout<< m_bMatrix << std::endl;

    std::cout << "Calibration done." << std::endl;
  }
  else{
    std::cout << "Calibration cancelled." << std::endl;
  }
}

void CameraProjection::Save()
{
  m_projectionMatrix =  m_aMatrix * m_bMatrix.inverse();

  std::cout << m_projectionMatrix << std::endl;
  // const static Eigen::IOFormat CSVFormat(Eigen::StreamPrecision,
  //     Eigen::DontAlignCols, ", ", "\n");
  const static Eigen::IOFormat saveFormat(Eigen::StreamPrecision,
      Eigen::DontAlignCols, " ", " ", "", "", "", "");
  

  std::ofstream file(m_transformationMatrixFileName);
  if(file.is_open()){
    file << m_projectionMatrix.format(saveFormat);
  }
  file.close();
  std::cout<<"Saved matrix as " + m_transformationMatrixFileName << std::endl;

}


void CameraProjection::Project()
{
  std::string mode("Calibration");
  cv::setMouseCallback(mode, ProjectMouseClicks, 
      (void *) &m_projectionMatrix);
  cv::waitKey(0);
  cv::setMouseCallback(mode, NULL, NULL);
  std::cout << "Exit point projection" << std::endl;
}

} // tool
} // core
} // opendlv
