#include "source.hpp"
#include "Utils/InputSys.hpp"
#include "Utils/defs.hpp"

#pragma disable(warning:4099)


#include "Utils/Threading/threading.h"
#include "Utils/Threading/shared_mutex.h"
#include "SDK/displacement.hpp"
#include "SDK/CVariables.hpp"
#include "Utils/lazy_importer.hpp"
#include "Utils/CrashHandler.hpp"
#include "Libraries/minhook-master/include/MinHook.h"
#include <thread>

#include "Features/Rage/TickbaseShift.hpp"

#include <iomanip> 
#include "Utils/syscall.hpp"

static Semaphore dispatchSem;
static SharedMutex smtx;

using ThreadIDFn = int(_cdecl*)();

ThreadIDFn AllocateThreadID;
ThreadIDFn FreeThreadID;

int AllocateThreadIDWrapper() {
	return AllocateThreadID();
}

int FreeThreadIDWrapper() {
	return FreeThreadID();
}

template<typename T, T& Fn>
static void AllThreadsStub(void*) {
	dispatchSem.Post();
	smtx.rlock();
	smtx.runlock();
	Fn();
}


// TODO: Build this into the threading library
template<typename T, T& Fn>
static void DispatchToAllThreads(void* data) {
	smtx.wlock();

	for (size_t i = 0; i < Threading::numThreads; i++)
		Threading::QueueJobRef(AllThreadsStub<T, Fn>, data);

	for (size_t i = 0; i < Threading::numThreads; i++)
		dispatchSem.Wait();

	smtx.wunlock();

	Threading::FinishQueue(false);
}

struct DllArguments {
	HMODULE hModule;
	LPVOID lpReserved;
};

namespace duxe::security {
	uintptr_t* rel32(uintptr_t ptr) {
		auto offset = *(uintptr_t*)(ptr + 0x1);
		return (uintptr_t*)(ptr + 5 + offset);
	}

	void bypass_mmap_detection(void* address, uint32_t region_size) {
		const auto client_dll = (uint32_t)GetModuleHandleA(XorStr("client.dll"));

		const auto valloc_call = client_dll + 0x90DC60;

		using add_allocation_to_list_t = int(__thiscall*)(
			uint32_t list, LPVOID alloc_base, SIZE_T alloc_size, DWORD alloc_type, DWORD alloc_protect, LPVOID ret_alloc_base, DWORD last_error, int return_address, int a8);

		auto add_allocation_to_list = (add_allocation_to_list_t)(rel32(Memory::Scan(XorStr("gameoverlayrenderer.dll"), XorStr("E8 ? ? ? ? 53 FF 15 ? ? ? ? 8B C7"))));

		const auto list = *(uint32_t*)(Memory::Scan(XorStr("gameoverlayrenderer.dll"), XorStr("56 B9 ? ? ? ? E8 ? ? ? ? 84 C0 74 1C")) + 2);

		add_allocation_to_list(list, address, region_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE, address, 0, valloc_call, 0);
	}
}

DWORD WINAPI Entry(DllArguments* pArgs) {
	g_Vars.globals.user_info = *(CVariables::GLOBAL::cheat_header_t*)pArgs->hModule;
	g_Vars.globals.c_login = reinterpret_cast<const char*>(pArgs->hModule);
	g_Vars.globals.hModule = pArgs->hModule;

	auto tier0 = GetModuleHandleA(XorStr("tier0.dll"));

	AllocateThreadID = (ThreadIDFn)GetProcAddress(tier0, XorStr("AllocateThreadID"));
	FreeThreadID = (ThreadIDFn)GetProcAddress(tier0, XorStr("FreeThreadID"));

	Threading::InitThreads();

	DispatchToAllThreads<decltype(AllocateThreadIDWrapper), AllocateThreadIDWrapper>(nullptr);

	// b1g fart.
	static bool bDownloaded = false;
	if (!bDownloaded) {
		// XDDXD
		g_Vars.menu.key.key = VK_INSERT;
		// xd v2 :D
		g_Vars.rage.key.cond = 2;
		bDownloaded = true;
	}

	if (Interfaces::Create(pArgs->lpReserved)) {
		Interfaces::m_pInputSystem->EnableInput(true);

		for (auto& child : g_Vars.m_children) {
			child->Save();

			auto json = child->GetJson();
			g_Vars.m_json_default_cfg[child->GetName()] = (json);
		}
	}
	return FALSE;
}

LONG WINAPI CrashHandlerWrapper(struct _EXCEPTION_POINTERS* exception) {
	auto ret = ICrashHandler::Get()->OnCrashProgramm(exception);
	return ret;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
	if (dwReason == DLL_PROCESS_ATTACH) {
		DllArguments* args = new DllArguments();
		args->hModule = hModule;
		args->lpReserved = lpReserved;

		SetUnhandledExceptionFilter(CrashHandlerWrapper);
		//AddVectoredExceptionHandler( 1, CrashHandlerWrapper );

		HANDLE thread;

		syscall(NtCreateThreadEx)(&thread, THREAD_ALL_ACCESS, nullptr, current_process,
			nullptr, args, THREAD_CREATE_FLAGS_CREATE_SUSPENDED | THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER, NULL, NULL, NULL, nullptr);
		CONTEXT context;
		context.ContextFlags = CONTEXT_FULL;
		syscall(NtGetContextThread)(thread, &context);
		context.Eax = reinterpret_cast<uint32_t>(&Entry);
		syscall(NtSetContextThread)(thread, &context);
		syscall(NtResumeThread)(thread, nullptr);

		return TRUE;
	}

	return FALSE;
}