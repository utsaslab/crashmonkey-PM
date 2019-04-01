#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <dirent.h>
#include <cstring>
#include <errno.h>
#include <sys/xattr.h>

#include "../BaseTestCase.h"
#include "../../wrapper/workload.h"
#include "../../wrapper/actions.h"

using tests::DataTestResult;
using wrapper::WriteData;
using wrapper::WriteDataMmap;
using wrapper::Checkpoint;
using std::string;

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

    namespace tests {
        
        
        class testName: public BaseTestCase {
            
            public:
            
            virtual int setup() override {
                
                return 0;
            }
            
            virtual int run( int checkpoint ) override {
                
                return 0;
            }
            
            virtual int check_test( unsigned int last_checkpoint, DataTestResult *test_result) override {
                
                return 0;
            }
                       
            private:
                       
                       
            };
                       
    }  // namespace tests
                       
   extern "C" tests::BaseTestCase *test_case_get_instance() {
       return new tests::testName;
   }
                       
   extern "C" void test_case_delete_instance(tests::BaseTestCase *tc) {
       delete tc;
   }
