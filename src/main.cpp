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
#include  <wait.h>

#include "communication/ClientSocket.h"
#include "utils/Tester.h"
#include <memory>

#define OPTIONS_LIST "b:d:e:f:i:p:r:v"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// By default the writetracker plugin will trace from 1GB - 1GB+128MB
#define DEFAULT_START_ADDR "0x40000000"
#define DEFAULT_END_ADDR "0x48000000"
#define DEFAULT_REPLAY_START_ADDR "0x48000000"
#define DEFAULT_FS "NOVA"
#define DEFAULT_RECORD_DEV "/dev/pmem0"
#define DEFAULT_REPLAY_DEV "/dev/pmem1"
#define MNT_POINT_RECORD "/mnt/pmem0"
#define MNT_POINT_REPLAY "/mnt/pmem1"

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

namespace {
static constexpr char kChangePath[] = "run_changes";
}

using namespace communication;

static bool verbose_flag;

static struct option harness_options[] = {
	{ "begin-trace-addr", required_argument, NULL, 'b' },
	{ "device-path-record", required_argument, NULL, 'd' },
	{ "end-trace-addr", required_argument, NULL, 'e' },
	{ "file-system", required_argument, NULL, 'f' },
	{ "ip", required_argument, NULL, 'i' },
	{ "port", required_argument, NULL, 'p' },
	{ "device-path-replay", required_argument, NULL, 'r' },
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
	string record_device_path(DEFAULT_RECORD_DEV);
	string replay_device_path(DEFAULT_REPLAY_DEV);
	int end_size = std::stoi(DEFAULT_END_ADDR,nullptr,0);
	int start_size = std::stoi(DEFAULT_START_ADDR, nullptr,0);

	// size of the record device in MB. We will assume that the
	// replay device is also of the same size 
	int record_size = (end_size-start_size)/1024/1024;

	if (record_size <= 0) {
		cerr << "Input device size should be non zero" << endl;
		return -1;
	}

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
			case 'r':
				replay_device_path = string(optarg);
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

	cout << setw(30) << left << "Record device" << setw(2) << left << ":"
	     << setw(2) << left << record_device_path << endl; 
	
	cout << setw(30) << left << "Replay device" << setw(2) << left << ":"
	     << setw(2) << left << replay_device_path << endl; 

	cout << "\n******************************************\n" << endl;


	// Get the test case name
	const unsigned int test_case_index = optind;

	if (test_case_index == argc) {
		cerr << "Input test file missing" << endl;
		return -1;
	}
	const string test_case_path = argv[test_case_index];

	// Get the name of the test being run.
	int begin = test_case_path.rfind('/');
	string test_case_name = test_case_path.substr(begin + 1);
	test_case_name = test_case_name.substr(0, test_case_name.length() - 3);

	cout << " Testing : " << test_case_name << endl;

	// create a log file
    	auto t = time(nullptr);
    	auto tm = *localtime(&t);
	ostringstream os;
	os << put_time(&tm, "%d%b%y_%T");
	string s = "PMTester_" + os.str() + ".log";

	ofstream log_file(s);

	// Instantiate the tester
	Tester pm_tester(record_size, verbose_flag);
	pm_tester.StartTestSuite();
	pm_tester.set_fs(fs);
	pm_tester.set_record_device(record_device_path);
	pm_tester.set_replay_device(replay_device_path);

	// Load test class
	cout << "Loading the test" << endl;
	log_file << "Loading the test" << endl;
	if (pm_tester.test_load_class(test_case_path.c_str()) != SUCCESS) {
		pm_tester.cleanup_harness();
		return -1;
	}

	// Init values of dev, FS, mount points
	pm_tester.test_init_values(MNT_POINT_RECORD, record_size);


	/***********************************************************
	* 1. Connect to the Qemu Monitor
	************************************************************/

	std::unique_ptr<ClientSocket> vm(new ClientSocket(remote_ip, remote_port));
	
	//initialize and connect to socket 
	if (vm->Init() < 0) {
		int err_no = errno;
		cout << "Error starting socket" << endl;
		return -1;
	}

	cout << "Connected to QEMU monitor" << endl;
	log_file << "Connected to QEMU monitor" << endl;
	

	/***********************************************************
	* 2. Format and snapshot the initial record device
	************************************************************/
	
	string mnt = "/mnt/pmem0";
	FSCommands *fs_command_ = NULL;
	fs_command_ = GetFSCommands(fs);
	//Format the record device and mount it
	cout << "Formating record device " << record_device_path << endl;
	log_file << "Formating record device " << record_device_path << endl;
	if (pm_tester.format_and_mount_device(record_device_path, mnt) != SUCCESS) {
		cerr << "Error formating the record device" << endl;
		pm_tester.cleanup_harness();
		return -1;
	}
	//system((fs_command_->GetMkfsCommand(record_device_path, mnt)).c_str());


	// FOr some reason, NOVA fails to mount if we dont snapshot
	// after the unmount. FOr now include unmount in the inital snapshot
	// only for NOVA
	// TODO: Ask about this in the NOVA mailing list	
	// umount the device
	
	if (fs.compare("NOVA") == 0) {
		cout << "Unmount the record device" << endl;
		if (pm_tester.umount_device() != SUCCESS) {
			cerr << "Error unmounting device" << endl;
			pm_tester.cleanup_harness();
			return -1;
		}
	}


	// Snapshot the initial FS image
	if (pm_tester.snapshot_device() != SUCCESS) {
		cerr << "Error snapshotting" << endl;
		pm_tester.cleanup_harness();
		return -1;
	}
	//string cmd = "scripts/take_snapshot.sh " + to_string(record_size);
	//system(cmd.c_str());

	// Apply the snapshot on replay device
	if (pm_tester.restore_snapshot() != SUCCESS) {
		cerr << "Error restoring snapshot" << endl;
		pm_tester.cleanup_harness();
		return -1;
	}
	//cmd = "scripts/apply_snapshot.sh " + to_string(record_size);
	//system(cmd.c_str());

	// if the FS is NOVA, we have unmounted it
	// before capturing snapshot. 
	// SO mount it bak again for workload execution
	if (fs.compare("NOVA") == 0) {
		if (pm_tester.mount_device(record_device_path, mnt) != SUCCESS) {
			cerr << "Error mounting the record device" << endl;
			pm_tester.cleanup_harness();
			return -1;
		}
	}
	cout << "Mounted file system. Ready for workload execution" << endl;
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


	SockMessage msg;
	vm->BuildLoadPluginMsg(msg, pWritetracker, begin_trace_addr, end_trace_addr);
	
	if (vm->SendCommand(msg) != eNone ) {
		int err_no = errno;
		cout << "Error sending message" << endl;
		return -1;
	}
	vm->ReceiveReply(msg);

	/***********************************************************
	* 3. Execute the workload
	************************************************************/


	cout << "Running j-lang test profile" << endl;
	bool last_checkpoint = false;
	int checkpoint = 0;

	do {
		const pid_t child = fork();
		if (child < 0) {
			cerr << "Error spinning off test process" << endl;
			pm_tester.cleanup_harness();
			return -1;
		}
		else if (child != 0) {
			// let the parent wait
			pid_t status = -1;
			pid_t wait_res = 0;
			do { 
				wait_res = waitpid(child, &status, WNOHANG);
			} while (wait_res == 0);

			if (WIFEXITED(status) == 0) {
				cerr << "Error terminating test_run process, status: " << status << endl;
				pm_tester.cleanup_harness();
				return -1;
			}

		}
		else {
			int change_fd;
			if (checkpoint == 0) {
				change_fd = open(kChangePath, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
				if (change_fd < 0) {
					return change_fd;
				}
			}
			const int res = pm_tester.test_run(change_fd, checkpoint);
			last_checkpoint = true;
			cout << "DOne executing the workload" << endl;
			if (checkpoint == 0) {
				close(change_fd);
			}
			return res;
		}

		cout << "Getting change data" << endl;
		const int change_fd = open(kChangePath, O_RDONLY);
		if (change_fd < 0) {
			cerr << "Error reading change data" << endl;
			pm_tester.cleanup_harness();
			return -1;
		}

		if (lseek(change_fd, 0, SEEK_SET) < 0) {
			cerr << "Error reading change data" << endl;
			pm_tester.cleanup_harness();
			return -1;
		}

		if (pm_tester.GetChangeData(change_fd) != SUCCESS) {
			pm_tester.cleanup_harness();
			return -1;
		}
		last_checkpoint = true;
		cout << "Is it last checkpoint ? " << last_checkpoint << endl;
	}while(!last_checkpoint);


	/***********************************************************
	* 4. UnLoad the writetracker plugin
	* Build the command to be sent over socket
	* Send the command
	* 	This will stop tracing and the results of the output
	* 	will be in a file named wt.out on the remote host.  
	************************************************************/

	msg = SockMessage();
	vm->BuildUnloadPluginMsg(msg, 0);
	
	if (vm->SendCommand(msg) != eNone ) {
		int err_no = errno;
		cout << "Error sending message" << endl;
		return -1;
	}
	//sleep(1);
	vm->ReceiveReply(msg);


	// umount the record device
	cout << "Unmount the record device" << endl;
	if (pm_tester.umount_device() != SUCCESS) {
		cerr << "Error unmounting device" << endl;
		pm_tester.cleanup_harness();
		return -1;
	}


	/***********************************************************
	* 5. Load the replay plugin
	* 	This plugin should replay the serialized 
	* 	traces in the wt.out file at the host
	*	starting at memory addr denoted by <end>
	*	When we receive the EOF reply from the Qemu Monitor, 
	*	we know this plugin has completed initialization.
	*	All the replay happens in the initialization
	*	method of the plugin, so we can be sure
	*	that the replay is complete, when we receive
	*	EOF
	************************************************************/
        msg = SockMessage();
        vm->BuildLoadPluginMsg(msg, pReplay, begin_replay_addr, end_trace_addr);

        if (vm->SendCommand(msg) != eNone ) {
                int err_no = errno;
                cout << "Error sending message" << endl;
                return -1;
        }
        vm->ReceiveReply(msg);


	/***********************************************************
	* 6. Unload the replay plugin
	************************************************************/

        msg = SockMessage();
        vm->BuildUnloadPluginMsg(msg, 0);

        if (vm->SendCommand(msg) != eNone ) {
                int err_no = errno;
                cout << "Error sending message" << endl;
                return -1;
        }
        vm->ReceiveReply(msg);


	/***********************************************************
	* 7. Perform consistency tests
	************************************************************/
	// At point both record and replay
	// devices are not mounted.
	pm_tester.test_check(replay_device_path, log_file);

	/***********************************************************
	* 8. Cleanup and exit
	************************************************************/
	pm_tester.PrintTestStats(cout);
	log_file.close();

	// This will unmount the record device
	pm_tester.cleanup_harness();
	
	// generalize the umount function in Tester
	system("umount /mnt/pmem1");
	
	return 0;


}
