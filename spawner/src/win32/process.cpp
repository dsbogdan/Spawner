#include <process.h>
#include <iostream>

HANDLE stdout_read_mutex = NULL;

//TODO: implement setters and getters

CProcess::CProcess(/*arguments*/):stdinput(STD_INPUT), stdoutput(STD_OUTPUT), stderror(STD_ERROR)
{
	//getting arguments from list
	//working dir, etc
    for (int i = 0; i < RESTRICTION_MAX; i++)
        restrictions[i] = RESTRICTION_NO_LIMIT;
}

void CProcess::SetArguments()
{
	//is this required?..
	//after-constructor argument changing
}

int CProcess::Run(string file)
{
    application = file;
    setRestrictions();    
    createProcess();
    setupJobObject();

    DWORD w = ResumeThread(process_info.hThread);

    stdoutput.bufferize();
    stderror.bufferize();

    check = CreateThread(NULL, 0, check_limits, this, 0, NULL);
    wait();
    finish();
	return 0;
}

void CProcess::RunAsync()
{
    setRestrictions();
    createProcess();
    setupJobObject();
    DWORD w = ResumeThread(process_info.hThread);

    stdoutput.bufferize();
    stderror.bufferize();

    check = CreateThread(NULL, 0, check_limits, this, 0, NULL);
    // create thread, waiting for completition
//    wait();
//    finish();
}
CProcess::~CProcess()
{
	//kills process if it is running
}
thread_return_t CProcess::process_body(thread_param_t param)
{

    return 0;
}

thread_return_t CProcess::check_limits(thread_param_t param)
{
    CProcess *self = (CProcess *)param;
    DWORD t;
    JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION bai;

    if (self->GetRestrictionValue(RESTRICTION_PROCESSOR_TIME_LIMIT) == RESTRICTION_NO_LIMIT &&
        self->GetRestrictionValue(RESTRICTION_USER_TIME_LIMIT) == RESTRICTION_NO_LIMIT &&
        self->GetRestrictionValue(RESTRICTION_WRITE_LIMIT) == RESTRICTION_NO_LIMIT)
        return 0;

    t = GetTickCount();
    while (1)
    {
        BOOL rs = QueryInformationJobObject(self->hJob, JobObjectBasicAndIoAccountingInformation, &bai, sizeof(bai), NULL);
        if (!rs)
            break;

        if (self->GetRestrictionValue(RESTRICTION_WRITE_LIMIT) != RESTRICTION_NO_LIMIT && 
            bai.IoInfo.WriteTransferCount > (1024 * 1024) * self->GetRestrictionValue(RESTRICTION_WRITE_LIMIT))
        {
            PostQueuedCompletionStatus(self->hIOCP, JOB_OBJECT_MSG_PROCESS_WRITE_LIMIT, COMPLETION_KEY, NULL);
            break;
        }

        if (self->GetRestrictionValue(RESTRICTION_PROCESSOR_TIME_LIMIT) != RESTRICTION_NO_LIMIT && 
            (DOUBLE)bai.BasicInfo.TotalUserTime.QuadPart > SECOND_COEFF * self->GetRestrictionValue(RESTRICTION_PROCESSOR_TIME_LIMIT))
        {
            PostQueuedCompletionStatus(self->hIOCP, JOB_OBJECT_MSG_END_OF_PROCESS_TIME, COMPLETION_KEY, NULL);
            break;
        }
        if (self->GetRestrictionValue(RESTRICTION_USER_TIME_LIMIT) != RESTRICTION_NO_LIMIT && 
            (GetTickCount() - t) > self->GetRestrictionValue(RESTRICTION_USER_TIME_LIMIT))
        {
            PostQueuedCompletionStatus(self->hIOCP, JOB_OBJECT_MSG_END_OF_PROCESS_TIME, COMPLETION_KEY, NULL);//freezed
            break;
        }
        Sleep(1);
  }
  return 0;
}

void CProcess::createProcess()
{
    ZeroMemory(&si, sizeof(si));

    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = stdinput.ReadPipe();
    si.hStdOutput = stdoutput.WritePipe();
    si.hStdError = stderror.WritePipe();

    si.lpDesktop = "";
    // TODO may be create new restriction for error handling
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa, sizeof(sa));
    if ( !CreateProcess(application.c_str(),
        "test.exe 1 2 3 4 5",// replace with argument list construction
        NULL, NULL,
        TRUE,
        PROCESS_CREATION_FLAGS,
        NULL, NULL,
        &si, &process_info) )
    {
        throw("!!!");
    }
}

void CProcess::setRestrictions()
{
    /* implement restriction value check */
    hJob = CreateJobObject(NULL, NULL);
    DWORD le = GetLastError();

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION joeli;
    memset(&joeli, 0, sizeof(joeli));
    joeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;

    if (GetRestrictionValue(RESTRICTION_MEMORY_LIMIT) != RESTRICTION_NO_LIMIT)
    {   
        joeli.JobMemoryLimit = GetRestrictionValue(RESTRICTION_MEMORY_LIMIT);
        joeli.ProcessMemoryLimit = GetRestrictionValue(RESTRICTION_MEMORY_LIMIT);
        joeli.BasicLimitInformation.LimitFlags |=
            JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_JOB_MEMORY;
    }

    SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &joeli, sizeof(joeli));

    if (GetRestrictionValue(RESTRICTION_SECURITY_LIMIT) != RESTRICTION_NO_LIMIT)
    {
        JOBOBJECT_BASIC_UI_RESTRICTIONS buir;
        buir.UIRestrictionsClass = JOB_OBJECT_UILIMIT_ALL;
        SetInformationJobObject(hJob, JobObjectBasicUIRestrictions, &buir, sizeof(buir));
    }

    if (GetRestrictionValue(RESTRICTION_GUI_LIMIT) != RESTRICTION_NO_LIMIT)
    {
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }
}

void CProcess::setupJobObject()
{
    AssignProcessToJobObject(hJob, process_info.hProcess);

    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 1, 1);

    JOBOBJECT_ASSOCIATE_COMPLETION_PORT joacp; 
    joacp.CompletionKey = (PVOID)COMPLETION_KEY; 
    joacp.CompletionPort = hIOCP; 
    SetInformationJobObject(hJob, JobObjectAssociateCompletionPortInformation, &joacp, sizeof(joacp));
}

void CProcess::wait()
{
    DWORD waitTime = INFINITE;
    if (GetRestrictionValue(RESTRICTION_USER_TIME_LIMIT) != RESTRICTION_NO_LIMIT)
        waitTime = GetRestrictionValue(RESTRICTION_USER_TIME_LIMIT);
    WaitForSingleObject(process_info.hProcess, waitTime);
    DWORD dwNumBytes, dwKey;
    LPOVERLAPPED completedOverlapped;  
    static CHAR buf[1024];
    //set msg
    int message = 0;
    do
    {     
        GetQueuedCompletionStatus(hIOCP, &dwNumBytes, &dwKey, &completedOverlapped, INFINITE);

        switch (dwNumBytes)
        {
        case JOB_OBJECT_MSG_NEW_PROCESS:
            break;
        case JOB_OBJECT_MSG_END_OF_PROCESS_TIME:
            message++;
            TerminateJobObject(hJob, 0);
            break;
        case JOB_OBJECT_MSG_PROCESS_WRITE_LIMIT:  
            message++;
            TerminateJobObject(hJob, 0);
            break;
        case JOB_OBJECT_MSG_EXIT_PROCESS:
            message++;
            //*message = TM_EXIT_PROCESS;
            break;
        case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS:
            message++;
            //*message = TM_ABNORMAL_EXIT_PROCESS;
            break;
        case JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT:
            message++;
            //*message = TM_MEMORY_LIMIT_EXCEEDED;
            TerminateJobObject(hJob, 0);
            break;
        };    

    } while (!message);
    WaitForSingleObject(process_info.hProcess, 10000);// TODO
}

void CProcess::finish()
{
    stdoutput.finish();
    stderror.finish();
    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
    CloseHandle(check);
}

