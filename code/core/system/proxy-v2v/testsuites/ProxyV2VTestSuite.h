/**
 * proxy-v2v - Interface to V2V.
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

#ifndef PROXY_PROXYV2V_TESTSUITE_H
#define PROXY_PROXYV2V_TESTSUITE_H

#include "cxxtest/TestSuite.h"

// Include local header files.
#include "../include/ProxyV2V.h"

using namespace std;
using namespace odcore::data;
using namespace opendlv::core::system::proxy;

/**
 * This class derives from SensorBoard to allow access to protected methods.
 */
class ProxyTestling : public ProxyV2V {
   private:
    ProxyTestling();

   public:
    ProxyTestling(const int32_t &argc, char **argv)
        : ProxyV2V(argc, argv) {}

    // Here, you need to add all methods which are protected in ProxyV2V and which are needed for the test cases.
};

/**
 * The actual testsuite starts here.
 */
class ProxyV2VTest : public CxxTest::TestSuite {
   private:
    ProxyTestling *dt;

   public:
    void setUp() {
        // Prepare the data that would be available from commandline.
        string argv0("proxy-v2v");
        string argv1("--cid=100");
        int32_t argc = 2;
        char **argv;
        argv = new char *[2];
        argv[0] = const_cast< char * >(argv0.c_str());
        argv[1] = const_cast< char * >(argv1.c_str());

        // Create an instance of sensorboard through SensorBoardTestling which will be deleted in tearDown().
        dt = new ProxyTestling(argc, argv);
    }

    void tearDown() {
        delete dt;
        dt = NULL;
    }

    void testApplication() {
        //TS_ASSERT(true);
        TS_ASSERT(dt != NULL);
    }

    ////////////////////////////////////////////////////////////////////////////////////
    // Below this line the necessary constructor for initializing the pointer variables,
    // and the forbidden copy constructor and assignment operator are declared.
    //
    // These functions are normally not changed.
    ////////////////////////////////////////////////////////////////////////////////////

   public:
    /**
         * This constructor is only necessary to initialize the pointer variable.
         */
    ProxyV2VTest()
        : dt(NULL) {}

   private:
    /**
         * "Forbidden" copy constructor. Goal: The compiler should warn
         * already at compile time for unwanted bugs caused by any misuse
         * of the copy constructor.
         *
         * @param obj Reference to an object of this class.
         */
    ProxyV2VTest(const ProxyV2VTest & /*obj*/);

    /**
         * "Forbidden" assignment operator. Goal: The compiler should warn
         * already at compile time for unwanted bugs caused by any misuse
         * of the assignment operator.
         *
         * @param obj Reference to an object of this class.
         * @return Reference to this instance.
         */
    ProxyV2VTest &operator=(const ProxyV2VTest & /*obj*/);
};

#endif /*PROXY_PROXYV2V_TESTSUITE_H*/
