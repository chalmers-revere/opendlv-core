/**
 * proxy-velodyne64 - Interface to Velodyne HDL-64E.
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

#ifndef PROXY_PROXYVELODYNE64_TESTSUITE_H
#define PROXY_PROXYVELODYNE64_TESTSUITE_H

#include "cxxtest/TestSuite.h"

#include <cmath>
#include <memory>
#include <fstream>
#include <stdlib.h>
#include <vector>
#include <iostream>

#include "opendavinci/odcore/io/protocol/PCAPProtocol.h"
#include "opendavinci/generated/odcore/data/pcap/GlobalHeader.h"
#include "opendavinci/generated/odcore/data/pcap/PacketHeader.h"
#include "opendavinci/generated/odcore/data/pcap/Packet.h"
#include "opendavinci/odcore/data/Container.h"
#include "opendavinci/odcore/io/conference/ContainerConference.h"
#include <opendavinci/odcore/io/conference/ContainerConferenceFactory.h>
#include "opendavinci/odcore/io/conference/ContainerListener.h"
#include "opendavinci/odcore/wrapper/CompressionFactory.h"
#include "opendavinci/odcore/wrapper/DecompressedData.h"
#include "opendavinci/odcore/wrapper/SharedMemory.h"
#include "opendavinci/odcore/wrapper/SharedMemoryFactory.h"
#include "opendavinci/generated/odcore/data/SharedPointCloud.h"
#include "opendavinci/odcore/base/Lock.h"

#include "../include/velodyne64Decoder.h"


float* segment=NULL; 
uint32_t mSize=0;

using namespace std;
using namespace odcore::base;
using namespace odcore::data;
using namespace odcore::data;
using namespace odcore::io::protocol;

using namespace opendlv::core::system::proxy;
using namespace odcore::wrapper;

class MyContainerConference : public odcore::io::conference::ContainerConference {
   public:
    MyContainerConference() : ContainerConference(),m_counter(0){}
   
    virtual void send(odcore::data::Container &c) const {
        if(c.getDataType()==odcore::data::SharedPointCloud::ID()){}
        m_counter++;
        if(m_counter==2){
            if(c.getDataType()==odcore::data::SharedPointCloud::ID()){
                //cout<<"Receive Frame 1"<<endl;
                odcore::data::SharedPointCloud velodyneFrame=c.getData<SharedPointCloud>();//Get shared point cloud
                std::shared_ptr<odcore::wrapper::SharedMemory> vsm=SharedMemoryFactory::attachToSharedMemory(velodyneFrame.getName());//Attach the shared point cloud to the shared memory

                if(vsm.get()!=NULL){
                    if(vsm->isValid()){
                        //{odcore::base::Lock l(velodyneSM);
                        if (velodyneFrame.getComponentDataType() == SharedPointCloud::FLOAT_T
                                    && (velodyneFrame.getNumberOfComponentsPerPoint() == 4)
                                    && (velodyneFrame.getUserInfo() == SharedPointCloud::XYZ_INTENSITY)) {
                            mSize=velodyneFrame.getWidth();
                            memcpy(segment,vsm->getSharedMemory(),velodyneFrame.getSize());
                            cout<<"Copy memory"<<endl;
                        }
                    } 
                }
            }
        }
    }
    //private:
        mutable uint8_t m_counter;
};

class packetToByte : public odcore::io::conference::ContainerListener {
    public:
         packetToByte(std::shared_ptr<odcore::wrapper::SharedMemory> m):
         m_velodyneSM(m),
         v64decoder(m_velodyneSM,mcc,"../db.xml")
         {}
        
        ~packetToByte(){}
       
        virtual void nextContainer(odcore::data::Container &c) {
            if (c.getDataType() == odcore::data::pcap::GlobalHeader::ID()) {
                cout<<"Get the global header"<<endl;
            }
            if (c.getDataType() == odcore::data::pcap::PacketHeader::ID()) {
                //cout<<"Received a packet!"<<endl;
            }
            if (c.getDataType() == odcore::data::pcap::Packet::ID()) {
                // Here, we have a valid packet.
                
                //Extract payload from the packet
                pcap::Packet packet = c.getData<pcap::Packet>();
                pcap::PacketHeader packetHeader = packet.getHeader();
                if(packetHeader.getIncl_len()==1248)
                {
                    string payload = packet.getPayload();
                    payload = payload.substr(42,1206); //Remove the 42-byte Ethernet header
                    v64decoder.nextString(payload);  //Let the Velodyne64 decoder decode the 1206-byte payload
                }  
            }

        }
    private:
        std::shared_ptr<odcore::wrapper::SharedMemory> m_velodyneSM;
        MyContainerConference mcc;
        opendlv::core::system::proxy::velodyne64Decoder v64decoder;
};

class ProxyVelodyne64Test : public CxxTest::TestSuite{
    public:  
        ProxyVelodyne64Test():
        mIndex(0),
        compare(0),
        velodyneSM(SharedMemoryFactory::createSharedMemory(NAME, SIZE))
        //segment(NULL),
        //mSize(0)
        {
            segment=(float*)malloc(SIZE);
        }
        
        ~ProxyVelodyne64Test(){
            free(segment);
        }
         
        void readCsvFile(){
            fstream fin("../atwallFrame1New.zip", ios::binary | ios::in);
            std::shared_ptr<odcore::wrapper::DecompressedData> dd = odcore::wrapper::CompressionFactory::getContents(fin);
            fin.close();
            vector<string> entries = dd->getListOfEntries();
            std::shared_ptr<istream> stream = dd->getInputStreamFor(entries.at(0));
            stringstream decompressedData;
            if(stream.get()){
                char c;
                while(stream->get(c)){
                    decompressedData<<c;
                }
            }
            string value;
            getline(decompressedData,value);
            while(getline(decompressedData,value)){
                stringstream lineStream(value);
                string cell;
                getline(lineStream,cell,',');
                xDataV.push_back(stof(cell));
                getline(lineStream,cell,',');
                yDataV.push_back(stof(cell));
                getline(lineStream,cell,',');
                zDataV.push_back(stof(cell));
                getline(lineStream,cell,',');
                intensityV.push_back(stof(cell));
            }
        }
        

        void testVelodyneDecodingFromFile(){
            readCsvFile();
            PCAPProtocol pcap;
            packetToByte p2b(velodyneSM);
            //MyContainerConference mcc;
            //opendlv::core::system::proxy::velodyne64Decoder v64decoder(velodyneSM,mcc);
            pcap.setContainerListener(&p2b);

            fstream lidarStream("../atwallshort.pcap", ios::binary|ios::in);
            char *buffer = new char[BUFFER_SIZE+1];
            while (lidarStream.good()) {
                lidarStream.read(buffer, BUFFER_SIZE * sizeof(char));
                string s(buffer,BUFFER_SIZE);
                pcap.nextString(s);
            }
            lidarStream.close();
            pcap.setContainerListener(NULL);
            
            cout<<"File read complete."<<endl;
            delete [] buffer;
                        
            cout<<"Before comparing:"<<compare<<","<<mIndex<<endl;
            for(uint32_t vCounter=0;vCounter<xDataV.size();vCounter++){
                for(uint32_t mCounter=mIndex;mCounter<mSize*4;mCounter+=4){
                    if((abs(segment[mCounter]-xDataV[vCounter])<0.1f)&&((abs(segment[mCounter+1]-yDataV[vCounter]))<0.1f)&&
                    ((abs(segment[mCounter+2]-zDataV[vCounter]))<0.1f)&&((abs(segment[mCounter+3]-intensityV[vCounter]))<=1.0f)){
                        compare++;
                        mIndex=mCounter+4;
                        break;
                    }      
                }
            }
            
            cout<<"Number of points from VeloView: "<<xDataV.size()<<endl;
            cout<<"Number of points matched from our Velodyne decoder: "<<compare<<endl;
            
            TS_ASSERT(compare==xDataV.size());//All points from VeloView must be included by the data from our Velodyne decoder
        }

private:
    const uint32_t BUFFER_SIZE=4000;
    const std::string NAME = "testVelodyne64SM";
    const uint32_t SIZE = 1616000;
    
    vector<float> xDataV;
    vector<float> yDataV;
    vector<float> zDataV;
    vector<float> intensityV;
    uint32_t mIndex; 
    uint32_t compare;//Number of points matched between VeloView and our Velodyne decoder
    std::shared_ptr<odcore::wrapper::SharedMemory> velodyneSM;
    //float* segment; 
    //uint32_t mSize;

};

#endif /*PROXY_PROXYVELODYNE64_TESTSUITE_H*/
