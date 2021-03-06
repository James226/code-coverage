#include <string>
#include <mutex>
#include <fstream>
#include "CorProfiler.h"
#include "corhlpr.h"
#include "CComPtr.h"
#include "ILRewriter.h"
#include "profiler_pal.h"

static void STDMETHODCALLTYPE Enter(FunctionID functionId)
{
    CorProfiler::Get()->Enter(functionId);
}

static void STDMETHODCALLTYPE Leave(FunctionID functionId)
{
    CorProfiler::Get()->Leave(functionId);
}

void STDMETHODCALLTYPE CorProfiler::Enter(FunctionID functionId)
{
    ModuleID module;
    mdToken token;
    mdTypeDef type;
    ClassID classId;
    auto functionResult = corProfilerInfo->GetFunctionInfo(functionId, &classId, &module, &token);
    if (FAILED(functionResult)) {
        return;
    }
    
    corProfilerInfo->GetClassIDInfo(classId, &module, &type);
    
    auto mod = this->modules.find(module);
    if(mod == this->modules.end()) return;
    
    auto t = mod->second->types.find(type);
    if(t == mod->second->types.end()) return;
    
    auto func = t->second->functions.find(token);
    if (func == t->second->functions.end()) return;

    //printf("Enter %s\r\n", func->second->name.c_str());
    func->second->invocations++;
}

void STDMETHODCALLTYPE CorProfiler::Leave(FunctionID functionId)
{
    //printf("\r\nLeave %" UINT_PTR_FORMAT "", (UINT64)functionId);
}

std::string UnicodeToAnsi(const WCHAR* str) {
#ifdef _WINDOWS
    std::wstring ws(str);
#else
    std::basic_string<WCHAR> ws(str);
#endif
    return std::string(ws.begin(), ws.end());
}

COR_SIGNATURE enterLeaveMethodSignature             [] = { IMAGE_CEE_CS_CALLCONV_STDCALL, 0x01, ELEMENT_TYPE_VOID, ELEMENT_TYPE_I };

void(STDMETHODCALLTYPE* EnterMethodAddress)(FunctionID) = &Enter;
void(STDMETHODCALLTYPE *LeaveMethodAddress)(FunctionID) = &Leave;

CorProfiler::CorProfiler() : refCount(0), corProfilerInfo(nullptr)
{
}

CorProfiler::~CorProfiler()
{
    if (this->corProfilerInfo != nullptr)
    {
        this->corProfilerInfo->Release();
        this->corProfilerInfo = nullptr;
    }
}

CorProfiler* CorProfiler::_profiler = nullptr;
std::mutex singleton_mutex;

CorProfiler* CorProfiler::Get()
{
    std::lock_guard<std::mutex> guard(singleton_mutex);
    if (_profiler == nullptr)
    {
        _profiler = new CorProfiler();
    }
    return _profiler;
}

HRESULT STDMETHODCALLTYPE CorProfiler::Initialize(IUnknown *pICorProfilerInfoUnk)
{
    HRESULT queryInterfaceResult = pICorProfilerInfoUnk->QueryInterface(__uuidof(ICorProfilerInfo8), reinterpret_cast<void **>(&this->corProfilerInfo));
    
    if (FAILED(queryInterfaceResult))
    {
        return E_FAIL;
    }

    DWORD eventMask = COR_PRF_MONITOR_JIT_COMPILATION                      |
                      COR_PRF_DISABLE_TRANSPARENCY_CHECKS_UNDER_FULL_TRUST | /* helps the case where this profiler is used on Full CLR */
                      COR_PRF_DISABLE_INLINING |
                        COR_PRF_MONITOR_MODULE_LOADS |
                        COR_PRF_MONITOR_ASSEMBLY_LOADS |
                        COR_PRF_MONITOR_GC |
                        COR_PRF_MONITOR_CLASS_LOADS |
                        COR_PRF_MONITOR_THREADS |
                        COR_PRF_MONITOR_EXCEPTIONS;

    auto hr = this->corProfilerInfo->SetEventMask(eventMask);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::Shutdown()
{
    std::ofstream results;
    results.open("coverage.csv");

    for (const auto& [moduleId, module] : this->modules)
    for (const auto& [classId, type] : module->types) 
    for (const auto& [key, value] : type->functions) {
        //if (value->invocations > 0) {
            results << module->name.c_str() << "," << type->name.c_str() << "," << value->name.c_str() << "," << value->invocations << std::endl;
            printf("(%s) %s.%s: %i\r\n", module->name.c_str(), type->name.c_str(), value->name.c_str(), value->invocations);
        //}
        delete value;
    }
    
    results.close();

    if (this->corProfilerInfo != nullptr)
    {
        this->corProfilerInfo->Release();
        this->corProfilerInfo = nullptr;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AppDomainCreationStarted(AppDomainID appDomainId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AppDomainCreationFinished(AppDomainID appDomainId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AppDomainShutdownStarted(AppDomainID appDomainId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AppDomainShutdownFinished(AppDomainID appDomainId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AssemblyLoadStarted(AssemblyID assemblyId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AssemblyLoadFinished(AssemblyID assemblyId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AssemblyUnloadStarted(AssemblyID assemblyId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::AssemblyUnloadFinished(AssemblyID assemblyId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ModuleLoadStarted(ModuleID moduleId)
{
    return S_OK;
}

bool ContainsPath(const std::string dllFilename, const std::string dll)
{
    auto start = 0U;
    auto end = dll.find(",");

    do
    {
        auto allowedName = dll.substr(start, end - start);
        if (dllFilename.compare(allowedName) == 0)
        {
            return true;
        }
        start = end + 1;
        end = dll.find(",", start);
    }
    while (end != std::string::npos);
    
    auto allowedName = dll.substr(start, dll.length() - start);
    if (dllFilename.compare(allowedName) == 0)
    {
        return true;
    }
    
    return false;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ModuleLoadFinished(ModuleID moduleId, HRESULT hrStatus)
{
    HRESULT hr;
    PCCOR_SIGNATURE sig;
    ULONG blobSize, size, attributes;
    WCHAR name[256];
    DWORD flags;
    ULONG codeRva;
    LPCBYTE baseAddress;
    AssemblyID assemblyId;

    hr = this->corProfilerInfo->GetModuleInfo2(moduleId, &baseAddress, 256, &size, name, &assemblyId, &flags);

    const char* dll = std::getenv("CORECLR_PROFILER_DLL");
    if (!dll)
        dll = "CodeCoverage.Example.dll";
    
    
    auto dllPath = UnicodeToAnsi(name);
    auto dllFilename = dllPath.substr(dllPath.find_last_of("/\\") + 1);
    
    if (!ContainsPath(dllFilename, dll)) {
        return S_OK;
    }
    
    auto moduleDetails = new ModuleDetails(dllFilename);
    this->modules[moduleId] = moduleDetails;

    printf("Module loaded: %s (%llx)\r\n", UnicodeToAnsi(name).c_str(), (UINT64)moduleId);

    CComPtr<IMetaDataImport> metadataImport;
    hr = this->corProfilerInfo->GetModuleMetaData(moduleId, ofRead, IID_IMetaDataImport, reinterpret_cast<IUnknown**>(&metadataImport));

    HCORENUM position = nullptr;
    mdTypeDef types[50];
    ULONG numTypes;
    HRESULT typeResult;
    do {
        typeResult = metadataImport->EnumTypeDefs(&position, types, 50, &numTypes);
        
        if (typeResult == S_FALSE) break;
    
        for (ULONG i = 0; i < numTypes; ++i)
        {
            ULONG size;
            WCHAR typeName[256];
            DWORD flags;
            mdToken baseType;
            metadataImport->GetTypeDefProps(types[i], typeName, 256, &size, &flags, &baseType);
            
            if (typeName[0] == '<') {
                continue;
            }
            
            auto ansiTypeName = UnicodeToAnsi(typeName);
    
            printf("Found Type %s (%i)\r\n", ansiTypeName.c_str(), types[i]);
            
            auto typeDetails = new ClassDetails(ansiTypeName);
            moduleDetails->types[types[i]] = typeDetails;
    
            HCORENUM pos2 = nullptr;
            mdMethodDef methodDef[50];
            ULONG tokens;
    
            hr = metadataImport->EnumMethods(&pos2, types[i], methodDef, 50, &tokens);
    
            for (ULONG j = 0; j < tokens; ++j)
            {
                PCCOR_SIGNATURE sig;
                ULONG blobSize, size, attributes;
                WCHAR name[256];
                DWORD flags;
                ULONG codeRva;
                mdTypeDef type;
                metadataImport->GetMethodProps(methodDef[j], &type, name, 256, &size, &attributes, &sig, &blobSize, &codeRva, &flags);
                
                auto functionDetails = new FunctionDetails(UnicodeToAnsi(name));
                typeDetails->functions[methodDef[j]] = functionDetails;
                //printf("Found Method %i %s::%s\r\n", methodDef[j], UnicodeToAnsi(typeName).c_str(), UnicodeToAnsi(name).c_str());
                this->functions[methodDef[j]] = new FunctionDetails(UnicodeToAnsi(typeName) + "::" + UnicodeToAnsi(name));
            }
        }
    } while (typeResult == S_OK);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ModuleUnloadStarted(ModuleID moduleId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ModuleUnloadFinished(ModuleID moduleId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ModuleAttachedToAssembly(ModuleID moduleId, AssemblyID AssemblyId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ClassLoadStarted(ClassID classId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ClassLoadFinished(ClassID classId, HRESULT hrStatus)
{
    return S_OK;

    HRESULT hr;
    ModuleID moduleId;
    mdTypeDef typeDef;
    ClassID parentClassId;

    hr = this->corProfilerInfo->GetClassIDInfo2(classId, &moduleId, &typeDef, &parentClassId, 0, nullptr, nullptr);
    auto typeName = GetTypeName(typeDef, moduleId);
    if (typeName.rfind("CodeCoverage.Example", 0) != 0) {
        return S_OK;
    }

    printf("Class load finished: %s\r\n", typeName.c_str());


    CComPtr<IMetaDataImport> metadataImport;
    hr = this->corProfilerInfo->GetModuleMetaData(moduleId, ofRead, IID_IMetaDataImport, reinterpret_cast<IUnknown**>(&metadataImport));

    HCORENUM position = nullptr;
    mdMethodDef methodDef[50];
    ULONG tokens;

    hr = metadataImport->EnumMethods(&position, typeDef, methodDef, 50, &tokens);

    for (ULONG i = 0; i < tokens; ++i)
    {
        PCCOR_SIGNATURE sig;
        ULONG blobSize, size, attributes;
        WCHAR name[256];
        DWORD flags;
        ULONG codeRva;
        metadataImport->GetMethodProps(methodDef[i], &typeDef, name, 256, &size, &attributes, &sig, &blobSize, &codeRva, &flags);

        printf("Found Method %s::%s\r\n", typeName.c_str(), UnicodeToAnsi(name).c_str());
    }


    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ClassUnloadStarted(ClassID classId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ClassUnloadFinished(ClassID classId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::FunctionUnloadStarted(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::JITCompilationStarted(FunctionID functionId, BOOL fIsSafeToBlock)
{
    HRESULT hr;
    mdToken token;
    ClassID classId;
    ModuleID moduleId;

    IfFailRet(this->corProfilerInfo->GetFunctionInfo(functionId, &classId, &moduleId, &token));
    
    if (classId == 0) {
        printf("Skipping generic JIT\n");
        return S_OK;
    }
    mdTypeDef typeDef;
    ClassID pParentClassId;
    ULONG32 cNumTypeArgs;
    ULONG32 pcNumTypeArgs;
    ClassID typeArgs[50];  
    IfFailRet(this->corProfilerInfo->GetClassIDInfo2(classId, &moduleId, &typeDef, &pParentClassId, 50, &pcNumTypeArgs, typeArgs));
    
    CComPtr<IMetaDataImport> metadataImport;
    IfFailRet(this->corProfilerInfo->GetModuleMetaData(moduleId, ofRead | ofWrite, IID_IMetaDataImport, reinterpret_cast<IUnknown **>(&metadataImport)));

    CComPtr<IMetaDataEmit> metadataEmit;
    IfFailRet(metadataImport->QueryInterface(IID_IMetaDataEmit, reinterpret_cast<void **>(&metadataEmit)));
    
    auto mod = this->modules.find(moduleId);
    if(mod == this->modules.end()) return S_OK;
    
    auto type = mod->second->types.find(typeDef);
    if(type == mod->second->types.end()) return S_OK;
    
    auto func = type->second->functions.find(token);
    if (func == type->second->functions.end()) return S_OK;
    
    //printf("Function JIT Compilation Started. %s (%llx, %i, %i)\r\n", GetMethodName(functionId).c_str(), (UINT64)moduleId, typeDef, token);
    
    mdSignature enterLeaveMethodSignatureToken;
    metadataEmit->GetTokenFromSig(enterLeaveMethodSignature, sizeof(enterLeaveMethodSignature), &enterLeaveMethodSignatureToken);

    return RewriteIL(this->corProfilerInfo, nullptr, moduleId, token, functionId, reinterpret_cast<ULONGLONG>(EnterMethodAddress), reinterpret_cast<ULONGLONG>(LeaveMethodAddress), enterLeaveMethodSignatureToken);
}

HRESULT STDMETHODCALLTYPE CorProfiler::JITCompilationFinished(FunctionID functionId, HRESULT hrStatus, BOOL fIsSafeToBlock)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::JITCachedFunctionSearchStarted(FunctionID functionId, BOOL *pbUseCachedFunction)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::JITCachedFunctionSearchFinished(FunctionID functionId, COR_PRF_JIT_CACHE result)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::JITFunctionPitched(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::JITInlining(FunctionID callerId, FunctionID calleeId, BOOL *pfShouldInline)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ThreadCreated(ThreadID threadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ThreadDestroyed(ThreadID threadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ThreadAssignedToOSThread(ThreadID managedThreadId, DWORD osThreadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingClientInvocationStarted()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingClientSendingMessage(GUID *pCookie, BOOL fIsAsync)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingClientReceivingReply(GUID *pCookie, BOOL fIsAsync)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingClientInvocationFinished()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingServerReceivingMessage(GUID *pCookie, BOOL fIsAsync)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingServerInvocationStarted()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingServerInvocationReturned()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RemotingServerSendingReply(GUID *pCookie, BOOL fIsAsync)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::UnmanagedToManagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ManagedToUnmanagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeSuspendStarted(COR_PRF_SUSPEND_REASON suspendReason)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeSuspendFinished()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeSuspendAborted()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeResumeStarted()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeResumeFinished()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeThreadSuspended(ThreadID threadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RuntimeThreadResumed(ThreadID threadId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::MovedReferences(ULONG cMovedObjectIDRanges, ObjectID oldObjectIDRangeStart[], ObjectID newObjectIDRangeStart[], ULONG cObjectIDRangeLength[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ObjectAllocated(ObjectID objectId, ClassID classId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ObjectsAllocatedByClass(ULONG cClassCount, ClassID classIds[], ULONG cObjects[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ObjectReferences(ObjectID objectId, ClassID classId, ULONG cObjectRefs, ObjectID objectRefIds[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RootReferences(ULONG cRootRefs, ObjectID rootRefIds[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionThrown(ObjectID thrownObjectId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchFunctionEnter(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchFunctionLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchFilterEnter(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchFilterLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionSearchCatcherFound(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionOSHandlerEnter(UINT_PTR __unused)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionOSHandlerLeave(UINT_PTR __unused)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionUnwindFunctionEnter(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionUnwindFunctionLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionUnwindFinallyEnter(FunctionID functionId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionUnwindFinallyLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionCatcherEnter(FunctionID functionId, ObjectID objectId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionCatcherLeave()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::COMClassicVTableCreated(ClassID wrappedClassId, REFGUID implementedIID, void *pVTable, ULONG cSlots)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::COMClassicVTableDestroyed(ClassID wrappedClassId, REFGUID implementedIID, void *pVTable)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionCLRCatcherFound()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ExceptionCLRCatcherExecute()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ThreadNameChanged(ThreadID threadId, ULONG cchName, WCHAR name[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::GarbageCollectionStarted(int cGenerations, BOOL generationCollected[], COR_PRF_GC_REASON reason)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::SurvivingReferences(ULONG cSurvivingObjectIDRanges, ObjectID objectIDRangeStart[], ULONG cObjectIDRangeLength[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::GarbageCollectionFinished()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::FinalizeableObjectQueued(DWORD finalizerFlags, ObjectID objectID)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::RootReferences2(ULONG cRootRefs, ObjectID rootRefIds[], COR_PRF_GC_ROOT_KIND rootKinds[], COR_PRF_GC_ROOT_FLAGS rootFlags[], UINT_PTR rootIds[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::HandleCreated(GCHandleID handleId, ObjectID initialObjectId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::HandleDestroyed(GCHandleID handleId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::InitializeForAttach(IUnknown *pCorProfilerInfoUnk, void *pvClientData, UINT cbClientData)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ProfilerAttachComplete()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ProfilerDetachSucceeded()
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ReJITCompilationStarted(FunctionID functionId, ReJITID rejitId, BOOL fIsSafeToBlock)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::GetReJITParameters(ModuleID moduleId, mdMethodDef methodId, ICorProfilerFunctionControl *pFunctionControl)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ReJITCompilationFinished(FunctionID functionId, ReJITID rejitId, HRESULT hrStatus, BOOL fIsSafeToBlock)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ReJITError(ModuleID moduleId, mdMethodDef methodId, FunctionID functionId, HRESULT hrStatus)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::MovedReferences2(ULONG cMovedObjectIDRanges, ObjectID oldObjectIDRangeStart[], ObjectID newObjectIDRangeStart[], SIZE_T cObjectIDRangeLength[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::SurvivingReferences2(ULONG cSurvivingObjectIDRanges, ObjectID objectIDRangeStart[], SIZE_T cObjectIDRangeLength[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ConditionalWeakTableElementReferences(ULONG cRootRefs, ObjectID keyRefIds[], ObjectID valueRefIds[], GCHandleID rootIds[])
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::GetAssemblyReferences(const WCHAR *wszAssemblyPath, ICorProfilerAssemblyReferenceProvider *pAsmRefProvider)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::ModuleInMemorySymbolsUpdated(ModuleID moduleId)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::DynamicMethodJITCompilationStarted(FunctionID functionId, BOOL fIsSafeToBlock, LPCBYTE ilHeader, ULONG cbILHeader)
{
    //printf("\r\nDynamic Function JIT Compilation Started. %" UINT_PTR_FORMAT "", (UINT64)functionId);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CorProfiler::DynamicMethodJITCompilationFinished(FunctionID functionId, HRESULT hrStatus, BOOL fIsSafeToBlock)
{
    //printf("\r\nDynamic Function JIT Compilation Finished. %" UINT_PTR_FORMAT "", (UINT64)functionId);
    return S_OK;
}

std::string CorProfiler::GetTypeName(mdTypeDef type, ModuleID module) const {
    CComPtr<IMetaDataImport> spMetadata;
    if (SUCCEEDED(corProfilerInfo->GetModuleMetaData(module, ofRead, IID_IMetaDataImport, reinterpret_cast<IUnknown**>(&spMetadata)))) {
        WCHAR name[256];
        ULONG nameSize = 256;
        DWORD flags;
        mdTypeDef baseType;
        if (SUCCEEDED(spMetadata->GetTypeDefProps(type, name, 256, &nameSize, &flags, &baseType))) {
            return UnicodeToAnsi(name);
        }
    }
    return "";
}

std::string CorProfiler::GetMethodName(FunctionID function) const {
    ModuleID module;
    mdToken token;
    mdTypeDef type;
    ClassID classId;
    auto functionResult = corProfilerInfo->GetFunctionInfo(function, &classId, &module, &token);
    if (FAILED(functionResult))
        return "Unknown function";

    CComPtr<IMetaDataImport> spMetadata;
    auto result = corProfilerInfo->GetModuleMetaData(module, CorOpenFlags::ofRead, IID_IMetaDataImport, reinterpret_cast<IUnknown**>(&spMetadata));
    if (FAILED(result)) {
        return "Unknown Module: " + std::to_string(module) + " -- " + std::to_string(result);
    }
    PCCOR_SIGNATURE sig;
    ULONG blobSize, size, attributes;
    WCHAR name[256];
    DWORD flags;
    ULONG codeRva;
    if (FAILED(spMetadata->GetMethodProps(token, &type, name, 256, &size, &attributes, &sig, &blobSize, &codeRva, &flags)))
        return "Unknown Method Props";

    return GetTypeName(type, module) + "::" + UnicodeToAnsi(name);
}
