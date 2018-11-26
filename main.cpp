#include <unistd.h>
#include <string>
#include <iostream>
#include <getopt.h>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <fstream>
#include <sys/mount.h>
#include <cassert>

#include "communication/ClientSocket.h"
#include "FSCommands.h"

#define OPTIONS_LIST "b:d:e:f:i:p:v"

// By default the writetracker plugin will trace from 1GB - 1GB+128MB
#define DEFAULT_START_ADDR "0x40000000"
#define DEFAULT_END_ADDR "0x48000000"
#define DEFAULT_REPLAY_START_ADDR "0x48000000"
#define DEFAULT_FS "NOVA"
#define DEFAULT_DEV "/dev/pmem0"

using std::string;
using std::cout;
using std::endl;
using std::cerr;
using std::left;
using std::setw;
using std::time;
using std::localtime;
using std::put_time;
using std::ostringstream;
using std::ofstream;
using std::to_string;

using namespace communication;

static bool verbose_flag;

static struct option harness_options[] = {
	{ "begin-trace-addr", required_argument, NULL, 'b' },
	{ "device-path-record", required_argument, NULL, 'd' },
	{ "end-trace-addr", required_argument, NULL, 'e' },
	{ "file-system", required_argument, NULL, 'f' },
	{ "ip", required_argument, NULL, 'i' },
	{ "port", required_argument, NULL, 'p' },
	{ "verbose", no_argument, NULL, 'v' },
	{ 0, 0, 0, 0 },
};

int main(int argc, char** argv) {

	// Let's set some default values if the user doesn't provide args
	string remote_ip("192.168.122.1");
	//string remote_ip("127.0.0.1");
	unsigned int remote_port = 4444;
	string begin_trace_addr(DEFAULT_START_ADDR);
	string end_trace_addr(DEFAULT_END_ADDR);
	string begin_replay_addr(DEFAULT_REPLAY_START_ADDR);
	string fs(DEFAULT_FS);
	string record_device_path(DEFAULT_DEV);
	int end_size = std::stoi(DEFAULT_END_ADDR,nullptr,0);
	int start_size = std::stoi(DEFAULT_START_ADDR, nullptr,0);
	int record_size = (end_size-start_size)/1024/1024;

	FSCommands *fs_commands_ = NULL;

	// Parse inputs
	int option_index = 0;
	while (1) {
		int ret = getopt_long(argc, argv, OPTIONS_LIST, harness_options, &option_index);
		
		// if all command line options are parsed, exit the loop
		if ( ret == -1 ) {
			break;
		}

		switch (ret) {
			case 'b':
				begin_trace_addr = string(optarg);
				break;
			case 'd':
				record_device_path = string(optarg);
				break;
			case 'e':
				end_trace_addr = string(optarg);
				break;
			case 'f':
				fs = string(optarg);
				break;
			case 'i':
				remote_ip = string(optarg);
				break;
			case 'p':
				remote_port = atoi(optarg);
				break;
			case 'v':
				verbose_flag = true;
				break;
			case '?':
			case ':':
			default :
				cerr << "Error parsing arguments" << endl;
				return -1;		
		}
	}

	// Print the settings in which CrashMonkey-PM would run
	cout << "\n******** Running CrashMonkey-PM **********\n" << endl;
	cout << setw(30) << left << "Remote IP address" << setw(2) << left << ":"
	     << setw(2) << left << remote_ip << endl; 

	cout << setw(30) << left << "Remote port" << setw(2) << left << ":"
	     << setw(2) << left << remote_port << endl; 
	
	cout << setw(30) << left << "Tracking from memory address" << setw(2) << left << ":"
	     << setw(2) << left << begin_trace_addr << endl; 

	cout << setw(30) << left << "Tracking until memory address" << setw(2) << left << ":"
	     << setw(2) << left << end_trace_addr << endl; 
	
	cout << setw(30) << left << "Testing file system" << setw(2) << left << ":"
	     << setw(2) << left << fs << endl; 

	cout << "\n******************************************\n" << endl;
	
	// create a log file
    	auto t = time(nullptr);
    	auto tm = *localtime(&t);
	ostringstream os;
	os << put_time(&tm, "%d%b%y_%T");
	string s = "CM_" + os.str() + ".log";

	ofstream log_file(s);

	//Get the FS specific commands handle
	fs_commands_ = GetFSCommands(fs);
	assert(fs_commands_ != NULL);

	/***********************************************************
	* 1. Connect to the Qemu Monitor
	************************************************************/

	ClientSocket* vm = NULL;
	vm = new ClientSocket(remote_ip, remote_port);
	
	//initialize and connect to socket 
	if (vm->Init() < 0) {
		int err_no = errno;
		cout << "Error starting socket" << endl;
		delete vm;
		return -1;
	}

	cout << "Connected to socket" << endl;
	log_file << "Connected to socket" << endl;


	
	cout << "----Testing ----" << endl;
	cout << fs_commands_->GetFSName() << endl;
	string mnt = "/mnt/pmem0";
	cout << fs_commands_->GetMkfsCommand(record_device_path, mnt) << endl;
	cout << fs_commands_->GetMountCommand(record_device_path, mnt) << endl;
	cout << "----End testing----" << endl;	


	system((fs_commands_->GetMkfsCommand(record_device_path, mnt)).c_str());
	system("umount /mnt/pmem0");

	string cmd = "src/take_snapshot.sh " + to_string(record_size);
	system(cmd.c_str());
	cmd = "src/apply_snapshot.sh " + to_string(record_size);
	system(cmd.c_str());

	system((fs_commands_->GetMountCommand(record_device_path, mnt)).c_str());
	cout << "Mounted file system" << endl;
	system("mount | grep pmem");


	//TODO: Not including the mount traffic, results in journal recovery failure
	// in ext4. But if included, NOVA results in corruption.

	/***********************************************************
	* 2. Load the writetracker plugin
	* Build the command to be sent over socket
	* Send the command
	*	Now this enables mem tracing within the ranges 
	*	specified.
	* (TODO): Is it worth waiting for the reply, which is simply 
	* 	an echo of the command we sent? Probably grep the 
	* 	reply string for error messages? But I need to
	* 	insert a sec of sleep to read the contents  
	************************************************************/


	SockMessage *msg = new SockMessage();
	vm->BuildLoadPluginMsg(msg, pWritetracker, begin_trace_addr, end_trace_addr);
	
	if (vm->SendCommand(msg) != eNone ) {
		int err_no = errno;
		cout << "Error sending message" << endl;
		delete vm;
		return -1;
	}
	vm->ReceiveReply(msg);

	/***********************************************************
	* 3. Execute the workload
	************************************************************/
	//TODO
	//dummy workload for now
	//system("./workload seq 4K 4K overwrite 1");
	//system("./workload seq 1 4K");
	system("./workload");


	/***********************************************************
	* 4. UnLoad the writetracker plugin
	* Build the command to be sent over socket
	* Send the command
	* 	This will stop tracing and the results of the output
	* 	will be in a file named wt.out on the remote host.  
	************************************************************/

	msg = new SockMessage();
	vm->BuildUnloadPluginMsg(msg, 0);
	
	if (vm->SendCommand(msg) != eNone ) {
		int err_no = errno;
		cout << "Error sending message" << endl;
		delete vm;
		return -1;
	}
	//sleep(1);
	vm->ReceiveReply(msg);


	system("ls /mnt/pmem0");
	system("mount | grep pmem0");
	if (umount("/mnt/pmem0") < 0) {
		cout << "Error unmounting" << endl;
		return -1;
	}


	/***********************************************************
	* 5. Load the replay plugin
	* 	This plugin should replay the serialized 
	* 	traces in the wt.out file at the host
	*	starting at memory addr denoted by <end>
	************************************************************/
        msg = new SockMessage();
        vm->BuildLoadPluginMsg(msg, pReplay, begin_replay_addr, end_trace_addr);

        if (vm->SendCommand(msg) != eNone ) {
                int err_no = errno;
                cout << "Error sending message" << endl;
                delete vm;
                return -1;
        }
        vm->ReceiveReply(msg);


	/***********************************************************
	* 6. Unload the replay plugin
	************************************************************/

        msg = new SockMessage();
        vm->BuildUnloadPluginMsg(msg, 0);

        if (vm->SendCommand(msg) != eNone ) {
                int err_no = errno;
                cout << "Error sending message" << endl;
                delete vm;
                return -1;
        }
        vm->ReceiveReply(msg);


	/***********************************************************
	* 7. Perform consistency tests
	************************************************************/
	//system("src/compare_devices.sh");

	/***********************************************************
	* 8. Cleanup and exit
	************************************************************/

	delete msg;
	delete fs_commands_;
	
	log_file.close();
	delete vm;
	return 0;


}