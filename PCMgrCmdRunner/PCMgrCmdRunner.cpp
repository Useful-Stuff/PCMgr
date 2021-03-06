// PCMgrCmdRunner.cpp: 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "PCMgrCmdRunnerEntry.h"
#include <string>
#include <vector>

#include "..\TaskMgrCore\cmdhlp.h"
#include "..\TaskMgrCore\StringHlp.h"
#include "..\TaskMgrCore\PathHelper.h"
#include "..\TaskMgrCore\fmhlp.h"
#include "..\TaskMgrCore\mapphlp.h"
#include "..\TaskMgrCore\prochlp.h"
#include "..\TaskMgrCore\ntdef.h"
#include "..\TaskMgrCore\thdhlp.h"
#include "..\TaskMgrCore\suact.h"
#include "..\TaskMgrCore\syshlp.h"
#include "..\TaskMgrCore\loghlp.h"
#include "..\TaskMgrCore\uwphlp.h"
#include "..\TaskMgrCore\kernelhlp.h"

BOOL kernelLodWarnShowed = FALSE;

vector<string> *MAppConsoleInitCommandLine(string * outCmd)
{
	string orgCmd;
	int argc = 0;
	LPWSTR*argsStrs = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argsStrs) {
		if (argc > 1) {
			vector<string> *cmdArray = new vector<string>();
			for (int i = 1; i < argc; i++)
			{
				LPCSTR str = W2A(argsStrs[i]);
				cmdArray->push_back(string(str));
				orgCmd += str;
				delete str;
			}
			if (outCmd)*outCmd = orgCmd;
			return cmdArray;
		}
		LocalFree(argsStrs);
		return nullptr;
	}
	return nullptr;
}

MCmdRunner *runner = NULL;

BOOL cmdThreadCanRun = FALSE;
BOOL cmdThreadRunning = FALSE;
HANDLE hCmdThread = NULL;

HANDLE hEventRunAppCmds = NULL;
vector<string> currentAppCmds;

CMD_CAPI(UINT) MAppCmdGetCount() { return currentAppCmds.size(); }
CMD_CAPI(BOOL) MAppCmdGetAt(UINT i, LPSTR buffer, size_t maxbuf) {
	if (buffer && i >= 0 && i < currentAppCmds.size()) {
		strcpy_s(buffer, maxbuf, currentAppCmds[i].c_str());
		return TRUE;
	}return FALSE;
}

void __cdecl MEnumProcessCallBack(DWORD pid, DWORD parentid, LPWSTR exename, LPWSTR exefullpath, int tp, HANDLE hProcess)
{
	if (tp)
	{
		MPrintMumberWithLen(pid, 5);
		printf(" ");
		MPrintMumberWithLen(parentid, 5);
		printf("        ");//6
		MPrintStrWithLenW(exename, 32);
		wprintf_s(L"   %s\n", exefullpath);
	}
}
void __cdecl MEnumServicesCallBack(LPWSTR dspName, LPWSTR scName,
	DWORD scType, DWORD currentState, DWORD dwProcessId, BOOL sysSc, DWORD dwStartType,
	LPWSTR lpBinaryPathName, LPWSTR lpLoadOrderGroup, BOOL add)
{
	MPrintStrWithLenW(dspName, 32);
	printf(" ");
	MPrintStrWithLenW(scName, 22);
	printf(" ");
	if (dwProcessId == 0)printf("     ");
	else MPrintMumberWithLen(dwProcessId, 5);
	printf(" ");
	wprintf_s(lpBinaryPathName);
	putchar('\n');
}

M_CMD_HANDLER(MRunCmd_Fix)
{
#ifdef _X64_
	if (MFM_FileExist(L"PCMgrUpdate64.exe")) MRunExe((LPWSTR)L"PCMgrUpdate64.exe", (LPWSTR)L"fix", FALSE, NULL);
#else
	if (MFM_FileExist(L"PCMgrUpdate.exe")) MRunExe((LPWSTR)L"PCMgrUpdate.exe", (LPWSTR)L"fix", FALSE, NULL);
#endif
}
M_CMD_HANDLER(MRunCmd_TaskList)
{
	wprintf_s(L"PID     ParentPID ProcessName                          FullPath\n");
	wprintf_s(L"===== ========= ================================ ===========\n");
	MEnumProcess((EnumProcessCallBack)MEnumProcessCallBack, NULL);
}
M_CMD_HANDLER(MRunCmd_ScList)
{
	if (!MSCM_Inited())  if (!MSCM_Init()) return;

	wprintf_s(L"Name                             Service Name           PID   Binary Path\n");
	wprintf_s(L"================================ ====================== ===== ========\n");
	MEnumServices(MEnumServicesCallBack);
}
M_CMD_HANDLER(MRunCmd_ScStop)
{
	if (size < 2) { printf("Please enter service name.\n"); return; }

	LPWSTR scName = A2W((*cmds)[2].c_str());

	if (MSCM_ControlSc(scName, SERVICE_STOPPED, SERVICE_CONTROL_STOP, (LPWSTR)L"Can not stop service"))
		MPrintSuccess();

	FreeStringPtr(scName);
}
M_CMD_HANDLER(MRunCmd_ScStart)
{
	if (size < 2) { printf("Please enter service name.\n"); return; }

	LPWSTR scName = A2W((*cmds)[2].c_str());

	if (MSCM_StartSc(scName))
		MPrintSuccess();

	FreeStringPtr(scName);
}
M_CMD_HANDLER(MRunCmd_ScPause)
{
	if (size < 2) { printf("Please enter service name.\n"); return; }

	LPWSTR scName = A2W((*cmds)[2].c_str());

	if (MSCM_ControlSc(scName, SERVICE_PAUSED, SERVICE_CONTROL_PAUSE, (LPWSTR)L"Can not pause service"))
		MPrintSuccess();

	FreeStringPtr(scName);
}
M_CMD_HANDLER(MRunCmd_ScCon)
{
	if (size < 2) { printf("Please enter service name.\n"); return; }

	LPWSTR scName = A2W((*cmds)[2].c_str());

	if (MSCM_ControlSc(scName, SERVICE_RUNNING, SERVICE_CONTROL_CONTINUE, (LPWSTR)L"Can not continue run service"))
		MPrintSuccess();

	FreeStringPtr(scName);
}
M_CMD_HANDLER(MRunCmd_Help) {
	printf("Help : \n");
	printf("    ? or help : Show this help\n");
	printf("    tasklist : list all running process\n");
	printf("    taskkill (pid) [force] [useApc] : kill a running process use process id\n            force : Want to use kernel force kill process\n            useApc : When force kill , should use APC to terminate threads\n");
	printf("    taskkilln (ProcessName) [force] [useApc] : kill a running process use process name\n            force : Want to use kernel force kill process\n            useApc : When force kill , should use APC to terminate threads\n");
	printf("    tasksuspend (pid) [force] : suspend process use process id\n            force : Want to use kernel force suspend process\n");
	printf("    taskresume (pid) [force] : resume process use process id\n            force : Want to use kernel force resume process\n");
	printf("    toadmin : run PCManager as adminstrator\n");
	printf("    threadkill (tid)  [force] [useApc] : kill a running thread use thread id\n            force : Want to use kernel force kill thread\n            useApc : When force kill , should use APC to terminate thread\n");
	printf("    killuwp (fullPackageName) : stop a uwp app running\n");
	printf("    runuwp (fullPackageName) (appName) : Run a uwp app\n");
	printf("    vmodule (pid) : View module information in a process\n");
	printf("    vthread (pid) : View thread information in a process\n");
	printf("    sclist : list all service\n");
	printf("    scstop (serviceName) : Stop service\n");
	printf("    scstart (serviceName) : Start service\n");
	printf("    scpause (serviceName) : Pause service\n");
	printf("    sccon (serviceName) : Continue run service\n");
	printf("    loaddrv (driverPath) : Load a kernel driver, the driverServiceName is the driver file name\n");
	printf("    loaddrv (driverServiceName) (driverPath) : Load a kernel driver use own driver Service Name\n");
	printf("    unloaddrv (driverServiceName) : Unload a driver that loaded by loaddrv command\n");
	printf("    su : Load PCManager kernel mode\n");
	printf("    quit-su : Quit PCManager kernel mode\n");
	printf("    vsign (filePath) : Verify embedded signature about file.\n");
	printf("    vexp (filePath) : Show export table in pe file.\n");
	printf("    vimp (filePath) : Show import table in pe file.\n");
	printf("    cls (filePath) : Clear Console\n");
	printf("    exit (filePath) : Exit Console app\n");
	printf("    =======================\n");
	printf("    fix : Run the repair program, it can try to fix application. \n");
	printf("    app : App debug cmdline. \n");
	printf("    =======================\n");
	printf("    And any command we do not know will be handed by the system() function.\n");
	printf("    =======================\n");
	printf("    gc : (For debug)\n");
	printf("    vstat : (For debug)\n");
	printf("    =======================");
}
M_CMD_HANDLER(MRunCmd_TaskKillName)
{
	if (size < 2) { printf("Please enter PocessName.\n"); return; }

	LPCWSTR procName = A2W((*cmds)[1].c_str());

	UINT endedCount = 0, failedCount = 0;

	PROCESSENTRY32 pe;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	pe.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hSnapshot, &pe)) {
		wprintf(L"No process named %s was found.\n", procName);
		FreeStringPtr((LPVOID)procName);
		return;
	}
	while (1)
	{
		pe.dwSize = sizeof(PROCESSENTRY32);
		if (Process32Next(hSnapshot, &pe) == FALSE)
			break;
		if (StrEqual(pe.szExeFile, procName))
			if (pe.th32ProcessID != GetCurrentProcessId())
			{			
				NTSTATUS status = MTerminateProcessNt(pe.th32ProcessID, NULL);
				if (NT_SUCCESS(status)) { Log(L"Process : %d Terminated.", pe.th32ProcessID); endedCount++; }
				else { wprintf(L"TerminateProcess Process %d Failed %s\n", pe.th32ProcessID, MNtStatusToStr(status)); failedCount++; }
			}
	}
	CloseHandle(hSnapshot);

	wprintf(L"Terminate process named %s %d terminated, %d failed.\n", procName, endedCount, failedCount);

	FreeStringPtr((LPVOID)procName);
}
M_CMD_HANDLER(MRunCmd_TaskKill)
{
	if (size < 2) { printf("Please enter pid.\n"); return; }
	DWORD pid = static_cast<DWORD>(atoll((*cmds)[1].c_str()));
	if (pid > 4) {
		if (size > 2 && (*cmds)[2] == "force")
		{
			BOOL useApc = FALSE;
			if (size > 3 && (*cmds)[3] == "apc")useApc = TRUE;
			NTSTATUS status = 0;
			if (M_SU_TerminateProcessPID(pid, 0, &status, useApc) && NT_SUCCESS(status))
				MPrintSuccess();
			else wprintf(L"TerminateProcess Failed %s\n", MNtStatusToStr(status));
		}
		else
		{
			HANDLE hProcess;
			NTSTATUS status = MOpenProcessNt(pid, &hProcess);
			if (status == STATUS_SUCCESS)
			{
				status = MTerminateProcessNt(0, hProcess);
				if (NT_SUCCESS(status)) printf("Success.\n");
				else wprintf(L"TerminateProcess Failed %s\n", MNtStatusToStr(status));
			}
			else wprintf(L"TerminateProcess Failed %s\n", MNtStatusToStr(status));
		}
	}
	else printf("Invalid pid.\n");
}
M_CMD_HANDLER(MRunCmd_ThreadKill)
{
	if (size < 2) { printf("Please enter tid.\n"); return; }
	DWORD tid = static_cast<DWORD>(atoll((*cmds)[1].c_str()));
	NTSTATUS status = 0;
	if (size > 2 && (*cmds)[2] == "force")
	{
		BOOL useApc = FALSE;
		if (size > 3 && (*cmds)[3] == "apc")useApc = TRUE;
		if (!(M_SU_TerminateThreadTID(tid, 0, &status, useApc) && NT_SUCCESS(status)))
			wprintf(L"TerminateThread Failed %s\n", MNtStatusToStr(status));
	}
	else {
		HANDLE hThread;
		DWORD NtStatus = MOpenThreadNt(tid, &hThread, tid);
		if (NT_SUCCESS(status)) {
			NtStatus = MTerminateThreadNt(hThread);
			if (NtStatus == STATUS_SUCCESS)
				printf("Success.\n");
			else wprintf(L"TerminateThread Failed %s\n", MNtStatusToStr(status));
		}
		else wprintf(L"Failed : OpenThread : %s\n", MNtStatusToStr(status));
	}
}
M_CMD_HANDLER(MRunCmd_TaskSuspend)
{
	if (size < 2) { printf("Please enter pid.\n"); return; }
	DWORD pid = static_cast<DWORD>(atoll((*cmds)[1].c_str()));
	if (pid > 4) {
		NTSTATUS status = MSuspendProcessNt(pid, 0);
		if (status == STATUS_SUCCESS)
			MPrintSuccess();
		else wprintf(L"Failed : SuspendProcess : %s\n", MNtStatusToStr(status));
	}
	else printf("Invalid pid.\n");
}
M_CMD_HANDLER(MRunCmd_TaskResume)
{
	if (size < 2) { printf("Please enter pid.\n"); return; }
	DWORD pid = static_cast<DWORD>(atoll((*cmds)[1].c_str()));
	if (pid > 4) {
		NTSTATUS status = MResumeProcessNt(pid, 0);
		if (status == STATUS_SUCCESS)
			MPrintSuccess();
		else wprintf(L"Failed : SuspendProcess :%s\n", MNtStatusToStr(status));
	}
	else printf("Invalid pid.\n");
}
M_CMD_HANDLER(MRunCmd_RunUWP)
{
	if (size < 2) { printf("Please enter full package name.\n"); return; }
	else if (size < 3) { printf("Please enter the app name that you want to run in package.\n"); return; }

	if (!M_UWP_IsInited()) M_UWP_Init();

	std::string pkgName = (*cmds)[1];
	
	size_t start = pkgName.find_first_of('_');
	size_t end = pkgName.find_last_of('_');
	if (start < end && end>0)
		pkgName = pkgName.replace(start, end - start, "");

	LPWSTR pkgNamew = A2W(pkgName.c_str());
	LPWSTR appName = A2W((*cmds)[2].c_str());
	DWORD processId = FALSE;
	if (M_UWP_RunUWPApp(pkgNamew, &processId))
		printf("Success. The new process id is : %d.", processId);

	FreeStringPtr(pkgNamew);
	FreeStringPtr(appName);
}
M_CMD_HANDLER(MRunCmd_StopUWP)
{
	if (size < 2) { printf("Please enter full package name.\n"); return; }

	LPWSTR pkgName = A2W((*cmds)[1].c_str());

	if (!M_UWP_IsInited()) M_UWP_Init();

	if (M_UWP_KillUWPApplication(pkgName))
		MPrintSuccess();

	FreeStringPtr(pkgName);
}
M_CMD_HANDLER(MRunCmd_Vsign)
{
	if (size < 2) { printf("Please enter full path of a file.\n"); return; }

	LPWSTR pkgName = A2W((*cmds)[1].c_str());
	Path::RemoveQuotes(pkgName, (*cmds)[1].size());

	if (MFM_FileExist(pkgName))
		MVerifyEmbeddedSignature(pkgName);
	else LogErr(L"The input file is not exist.");

	FreeStringPtr(pkgName);
}
M_CMD_HANDLER(MRunCmd_DeatchDebugger)
{
	if (size < 2) { printf("Please enter pid.\n"); return; }
	DWORD pid = static_cast<DWORD>(atoll((*cmds)[1].c_str()));
	if (pid > 4) {
		if (MAppWorkCall3(159, (HWND)(ULONG_PTR)pid, 0))
			MPrintSuccess();
	}
	else printf("Invalid pid.\n");
}
M_CMD_HANDLER(MRunCmd_CreateMiniDump)
{
	if (size < 2) { printf("Please enter pid.\n"); return; }
	DWORD pid = static_cast<DWORD>(atoll((*cmds)[1].c_str()));
	if (pid > 4) {
		if (MAppWorkCall3(158, (HWND)(ULONG_PTR)pid, 0))
			MPrintSuccess();
	}
	else printf("Invalid pid.\n");
}
M_CMD_HANDLER(MRunCmd_VModule)
{
	if (size < 2) { printf("Please enter pid.\n"); return; }
	DWORD pid = static_cast<DWORD>(atoll((*cmds)[1].c_str()));
	if (pid > 4) {

		HANDLE hProcess;
		NTSTATUS status = MOpenProcessNt(pid, &hProcess);
		if (status == STATUS_INVALID_HANDLE || status == STATUS_INVALID_CID) {
			LogErr(L"Invalid process");
			return;
		}
		else if (status == STATUS_ACCESS_DENIED) {
			LogErr(L"Aaccess denied");
			return;
		}
		else if (NT_SUCCESS(status) && hProcess) {

			BOOL bRet = FALSE;

#ifndef _AMD64_
			BOOL bWow64Proc = FALSE;
			IsWow64Process(hProcess, &bWow64Proc);
			if (!bWow64Proc)
			{
				printf("Please use 64 bit PCMgr to enumerate 64 bit process modules.\n"); 
				return;
			}
#endif

			PPEB_LDR_DATA pLdr = NULL;
			PPEB pPeb = 0;

			status = MGetProcessPeb(hProcess, &pPeb);

			if (NT_SUCCESS(status) && pPeb != 0)
			{
				status = MReadVirtualMemory(hProcess, PTR_ADD_OFFSET(pPeb, FIELD_OFFSET(PEB, Ldr)), &pLdr, sizeof(PPEB_LDR_DATA), NULL);
				if (!NT_SUCCESS(status)) {
					LogErr(L"View Modules Failed in MReadVirtualMemory : %s", MNtStatusToStr(status));
					return;
				}

				PLIST_ENTRY list_head = (PLIST_ENTRY)PTR_ADD_OFFSET(pLdr, FIELD_OFFSET(PEB_LDR_DATA, InMemoryOrderModuleList));
				PLIST_ENTRY p = list_head;
				LIST_ENTRY thisListEntry;
				status = MReadVirtualMemory(hProcess, list_head, &thisListEntry, sizeof(LIST_ENTRY), NULL);

				printf("Module information about process %d\n", pid);
				printf("Module name                      BaseAddress         SizeOfImage FullPath");
				printf("=============================== ================== ==========  =======");

				int i = 0;
				WCHAR thisName[MAX_PATH];
				for (p = thisListEntry.Flink; p != list_head; p = thisListEntry.Flink)
				{
					LDR_MODULE thisModule;
					status = MReadVirtualMemory(hProcess, CONTAINING_RECORD(p, LDR_MODULE, InMemoryOrderModuleList), &thisModule, sizeof(LDR_MODULE), NULL);
					status = MReadVirtualMemory(hProcess, p, &thisListEntry, sizeof(LIST_ENTRY), NULL);

					if (thisModule.BaseDllName.Buffer != NULL) {
						WCHAR baseDllNameBuffer[MAX_PATH];
						status = MReadVirtualMemory(hProcess, thisModule.BaseDllName.Buffer, &baseDllNameBuffer, sizeof(baseDllNameBuffer), NULL);
						wcscpy_s(thisName, baseDllNameBuffer);
						MPrintStrWithLenW(baseDllNameBuffer, 32);
						printf(" ");

						WCHAR fullDllNameBuffer[MAX_PATH];
						status = MReadVirtualMemory(hProcess, thisModule.FullDllName.Buffer, &fullDllNameBuffer, sizeof(fullDllNameBuffer), NULL);
						MPrintStrWithLenW(fullDllNameBuffer, 32);
						printf(" ");
				
						TCHAR addr[20];
#ifdef _AMD64_
						swprintf_s(addr, L"0x%I64X", (ULONG_PTR)thisModule.BaseAddress);
#else
						swprintf_s(addr, L"0x%08X", (ULONG_PTR)thisModule.BaseAddress);
#endif
						MPrintStrWithLenW(addr, 18);
						printf(" ");

						swprintf_s(addr, L"0x%08X", (ULONG)thisModule.SizeOfImage);
						MPrintStrWithLenW(addr, 10);
						printf(" \n");

						i++;
						if (i > 512)//Too big
							break;
					}
				}
			}
			else LogErr(L"Read peb of process failed.\nMGetProcessPeb  : %s", MNtStatusToStr(status));
			MCloseHandle(hProcess);
		}
		else LogErr(L"View Modules Failed in OpenProcess : %s", MNtStatusToStr(status));
	}
	else printf("Invalid pid.\n");
}
M_CMD_HANDLER(MRunCmd_VThread)
{
	if (size < 2) { printf("Please enter pid.\n"); return; }
	DWORD pid = static_cast<DWORD>(atoll((*cmds)[1].c_str()));
	if (pid > 0) 
	{
		THREADENTRY32 te32 = { sizeof(THREADENTRY32) };
		HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		if (hThreadSnap == INVALID_HANDLE_VALUE)
		{
			printf("CreateToolhelp32Snapshot failed , last error : %d.\n", GetLastError());
			return;
		}

		printf("Thread information about process %d\n", pid);
		printf("ThreadID OwnerProcessID Usage  Delta Priority Base Priority");
		printf("=====     =====          ====== ======         ======");

		if (Thread32First(hThreadSnap, &te32))
		{
			do {

				MPrintMumberWithLen(te32.th32ThreadID, 5);
				printf(" ");
				MPrintMumberWithLen(te32.th32OwnerProcessID, 5);
				printf(" ");
				MPrintMumberWithLen(te32.cntUsage, 5);
				printf(" ");
				MPrintMumberWithLen(te32.tpDeltaPri, 5);
				printf(" ");
				MPrintMumberWithLen(te32.tpBasePri, 5);
				printf(" \n");

			} while (Thread32Next(hThreadSnap, &te32));
		}

		CloseHandle(hThreadSnap);
	}
	else printf("Invalid pid.\n");
}
M_CMD_HANDLER(MRunCmd_LoadDrv) 
{
	if (size < 2) { printf("Please enter the driver file path.\n"); return; }

	LPWSTR drvName = A2W((*cmds)[1].c_str());
	WCHAR drvPath[MAX_PATH];
	WCHAR drvName2[MAX_PATH];

	if (size == 3) {
		LPWSTR drvP = A2W((*cmds)[1].c_str());
		wcscpy_s(drvPath, drvP);
		wcscpy_s(drvName2, drvName);		
		FreeStringPtr(drvP);
	}
	else {
		wcscpy_s(drvPath, drvName);
		wcscpy_s(drvName2, drvName);
		std::wstring *fname = Path::GetFileNameWithoutExtension(drvName2);
		wcscpy_s(drvName2, fname->c_str());
		FreeStringPtr(fname);
	}
	
	Path::RemoveQuotes(drvPath, wcslen(drvPath) + 1);

	if (MFM_FileExist(drvPath))
	{
		if (MLoadKernelDriver(drvName2, drvPath, NULL))
			MPrintSuccess();
	}
	else LogErr(L"The input driver path : %s is not exist", drvPath);

	FreeStringPtr(drvName);
}
M_CMD_HANDLER(MRunCmd_UnLoadDrv) 
{
	if (size < 2) { printf("Please enter the driver name.\n"); return; }

	LPWSTR drvName = A2W((*cmds)[1].c_str());
	if (MUnLoadKernelDriver(drvName))
		MPrintSuccess();
	FreeStringPtr(drvName);
}
M_CMD_HANDLER(MRunCmd_Su)
{
	if (MCanUseKernel())
		LogInfo(L"PCMgr are aleday in kernel modul.");
	else {
		if (!kernelLodWarnShowed)
		{
			LogInfo(L"The kernel management function is only partially completed, and the function has not been developed yet. \
There may be BUG. If you load the driver, it is likely to cause a blue screen problem because of incompatibility.\
\nTherefore, we do not recommend that you use this option.");
			LogWarn(L"enter \"su\" again to continue this operation. ");

			kernelLodWarnShowed = TRUE;
			return;
		}

		if (MInitKernel())
			MPrintSuccess();
	}
}
M_CMD_HANDLER(MRunCmd_QSu)
{
	if (MCanUseKernel())
	{
		if (MUninitKernel()) MPrintSuccess();
	}
	else LogWarn(L"PCMgr are not in kernel modul.");
}
M_CMD_HANDLER(MRunCmd_Test)
{
	if (size >= 2)
	{
		DWORD tid = static_cast<DWORD>(atoll((*cmds)[1].c_str()));
		switch (tid)
		{
		case 1: {
			wprintf_s(L"这是中文字符And Englist charters\n");
			break;
		}		
		case 2: {
			break;
		}
		default:
			printf("test agr : %d\n", tid);
			break;
		}
	}
}
M_CMD_HANDLER(MRunCmd_App)
{
	if (size >= 2)
	{
		string cmd2 = (*cmds)[1];
		if (cmd2 == "exit") MAppMainThreadCall(M_MTMSG_MAIN_EXIT, NULL);
		else if (cmd2 == "reboot") MAppMainThreadCall(M_MTMSG_MAIN_REBOOT, NULL);
		else if (cmd2 == "stat") MAppMainThreadCall(M_MTMSG_MAIN_SHOW_STAT, NULL);
		else {
			for (int i = 1; i < size; i++)
				currentAppCmds.push_back(string((*cmds)[i]));
			if(hEventRunAppCmds == NULL) hEventRunAppCmds = CreateEvent(NULL, TRUE, FALSE, L"EventRunAppCmds");
			else ResetEvent(hEventRunAppCmds);
			MAppMainThreadCall(M_MTMSG_MAIN_RUN_APP_CMD, (LPARAM)hEventRunAppCmds);
			WaitForSingleObject(hEventRunAppCmds, INFINITE);
			ResetEvent(hEventRunAppCmds);
			currentAppCmds.clear();
		}
	}
}

CMD_CAPI(VOID) MInitAllCmd()
{
	runner = MGetStaticCmdRunner();

	runner->RegisterCommandNoReturn("?", MRunCmd_Help);
	runner->RegisterCommandNoReturn("help", MRunCmd_Help);
	runner->RegisterCommandNoReturn("tasklist", MRunCmd_TaskList);
	runner->RegisterCommandNoReturn("taskkill", MRunCmd_TaskKill);
	runner->RegisterCommandNoReturn("taskkilln", MRunCmd_TaskKillName);
	runner->RegisterCommandNoReturn("tasksusp", MRunCmd_TaskSuspend);
	runner->RegisterCommandNoReturn("tasksuspend", MRunCmd_TaskResume);
	runner->RegisterCommandNoReturn("taskresume", MRunCmd_TaskResume);
	runner->RegisterCommandNoReturn("threadkill", MRunCmd_ThreadKill);
	runner->RegisterCommandNoReturn("runuwp", MRunCmd_RunUWP);
	runner->RegisterCommandNoReturn("killuwp", MRunCmd_StopUWP);
	runner->RegisterCommandNoReturn("sclist", MRunCmd_ScList);
	runner->RegisterCommandNoReturn("scstart", MRunCmd_ScStart);
	runner->RegisterCommandNoReturn("scstop", MRunCmd_ScStop);
	runner->RegisterCommandNoReturn("scpause", MRunCmd_ScPause);
	runner->RegisterCommandNoReturn("sccon", MRunCmd_ScCon);
	runner->RegisterCommandNoReturn("vsign", MRunCmd_Vsign);
	runner->RegisterCommandNoReturn("deattach-debugger", MRunCmd_DeatchDebugger);
	runner->RegisterCommandNoReturn("create-minidump", MRunCmd_CreateMiniDump);
	runner->RegisterCommandNoReturn("vmodule", MRunCmd_VModule);
	runner->RegisterCommandNoReturn("vthread", MRunCmd_VThread);
	runner->RegisterCommandNoReturn("loaddrv", MRunCmd_LoadDrv);
	runner->RegisterCommandNoReturn("unloaddrv", MRunCmd_UnLoadDrv);
	runner->RegisterCommandNoReturn("su", MRunCmd_Su);
	runner->RegisterCommandNoReturn("quitsu", MRunCmd_QSu);
	runner->RegisterCommandNoReturn("test", MRunCmd_Test);
	runner->RegisterCommandNoReturn("fix", MRunCmd_Fix);
	runner->RegisterCommandNoReturn("app", MRunCmd_App);
}

CMD_CAPI(BOOL) MAppCmdCanRun() {	return cmdThreadRunning; }
CMD_CAPI(BOOL) MAppCmdRunOne(BOOL isMain, char* cmd) {
	if (runner->MRunCmdWithString(cmd))
	{
		if (isMain)  MAppMainThreadCall(M_MTMSG_COSCLOSE, 0);
		return TRUE;
	}		
	return FALSE;
}
CMD_CAPI(VOID) MAppCmdOnExit() {
	if (hEventRunAppCmds) CloseHandle(hEventRunAppCmds);
	if (currentAppCmds.size() > 0)currentAppCmds.clear();
}

int MAppCmdStart()
{
	setlocale(LC_ALL, "chs");
	cmdThreadCanRun = TRUE;
	M_LOG_Init_InConsole();
	M_LOG_SetLogLevel(LogLevDebug);

	MAppWorkCall3(157, GetConsoleWindow(), NULL);

	printf_s("PCMgr Command Line Tool\n");
	printf_s("Version : %ls\n%ls\n", MAppGetVersion(), MAppGetBulidDate());

	MGetWindowsBulidVersion();
	printf_s("\n");

	string orgCmd;
	vector<string> * cmds = MAppConsoleInitCommandLine(&orgCmd);
	if (cmds) {
		if (cmds->size() > 1) runner->MRunCmd(cmds, orgCmd.c_str());
		delete(cmds);
	}

	cmdThreadRunning = TRUE;
	int rs = MAppCmdRunner(FALSE, NULL);
	M_LOG_Close_InConsole();
	ExitProcess(rs);
	return rs;
}

DWORD WINAPI MConsoleThread(LPVOID lpParameter)
{
	Sleep(4000);
	return MAppCmdRunner(TRUE, (COS_FOR_ALLOC_EXIT_CALLBACK)lpParameter);
}

CMD_CAPI(BOOL) MStartRunCmdThread(COS_FOR_ALLOC_EXIT_CALLBACK callback)
{
	if (!cmdThreadRunning)
	{
		cmdThreadCanRun = TRUE;
		hCmdThread = CreateThread(NULL, NULL, MConsoleThread, callback, NULL, NULL);
		cmdThreadRunning = TRUE;
		return cmdThreadRunning;
	}
	return FALSE;
}
CMD_CAPI(BOOL) MStopRunCmdThread()
{
	if (cmdThreadRunning)
	{
		cmdThreadCanRun = FALSE;
		if (hCmdThread)
		{
			DWORD dw = WaitForSingleObject(hCmdThread, 100);
			if (dw == WAIT_TIMEOUT) {
				if (NT_SUCCESS(MTerminateThreadNt(hCmdThread)))
					LogInfo(L"RunCmdThread Terminated.");
				else LogWarn(L"RunCmdThread Terminate failed!");
			}
			if (hCmdThread) { CloseHandle(hCmdThread); hCmdThread = 0; }
			cmdThreadRunning = FALSE;
			return 1;
		}
		cmdThreadRunning = FALSE;
	}
	return FALSE;
}




