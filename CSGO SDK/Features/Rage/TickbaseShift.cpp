#include "TickbaseShift.hpp"
#include "../../source.hpp"
#include "../../SDK/Classes/player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../Miscellaneous/Movement.hpp"
#include "../Game/Prediction.hpp"
#include "../../Libraries/minhook-master/include/MinHook.h"
#include "../../Hooking/Hooked.hpp"
#include "../../SDK/Displacement.hpp"

void* g_pLocal = nullptr;
TickbaseSystem g_TickbaseController;

TickbaseShift_t::TickbaseShift_t(int _cmdnum, int _tickbase) :
	cmdnum(_cmdnum), tickbase(_tickbase)
{
	;
}

#define OFFSET_LASTOUTGOING 0x4CAC
#define OFFSET_CHOKED 0x4CB0
#define OFFSET_TICKBASE 0x3404

bool TickbaseSystem::IsTickcountValid(int nTick) {
	return nTick >= (Interfaces::m_pGlobalVars->tickcount + int(1 / Interfaces::m_pGlobalVars->interval_per_tick) + g_Vars.sv_max_usercmd_future_ticks->GetInt());
}

void WriteUsercmdD(bf_write* buf, CUserCmd* incmd, CUserCmd* outcmd) {
	__asm
	{
		mov     ecx, buf
		mov     edx, incmd
		push    outcmd
		call    Engine::Displacement.Function.m_WriteUsercmd
		add     esp, 4
	}
}

bool Hooked::WriteUsercmdDeltaToBuffer(void* ECX, void* EDX, int nSlot, void* buffer, int o_from, int o_to, bool isnewcommand) {

	if (g_TickbaseController.iCommandsToShift <= 0)
		return oWriteUsercmdDeltaToBuffer;

	if (o_from != -1)
		return true;

	o_from = -1;

	int m_nTickbase = g_TickbaseController.iCommandsToShift;
	g_TickbaseController.iCommandsToShift = 0;

	int* m_pnNewCmds = (int*)((uintptr_t)buffer - 0x2C);
	int* m_pnBackupCmds = (int*)((uintptr_t)buffer - 0x30);

	*m_pnBackupCmds = 0;

	int m_nNewCmds = *m_pnNewCmds;
	int m_nNextCmd = Interfaces::m_pClientState->m_nChokedCommands() + Interfaces::m_pClientState->m_nLastOutgoingCommand() + 1;
	int m_nTotalNewCmds = std::min(m_nNewCmds + abs(m_nTickbase), 16);

	*m_pnNewCmds = m_nTotalNewCmds;

	for (o_to = m_nNextCmd - m_nNewCmds + 1; o_to <= m_nNextCmd; o_to++) {
		if (!oWriteUsercmdDeltaToBuffer)
			return false;

		o_from = o_to;
	}

	CUserCmd* m_pCmd = Interfaces::m_pInput->GetUserCmd(nSlot, o_from);
	if (!m_pCmd)
		return true;

	CUserCmd m_ToCmd = *m_pCmd, m_FromCmd = *m_pCmd;
	m_ToCmd.command_number++;
	m_ToCmd.tick_count += 3 * (int)std::round(1.f / Interfaces::m_pGlobalVars->interval_per_tick);

	for (int i = m_nNewCmds; i <= m_nTotalNewCmds; i++) {
		WriteUsercmdD((bf_write*)buffer, &m_ToCmd, &m_FromCmd);
		m_FromCmd = m_ToCmd;

		m_ToCmd.command_number++;
		m_ToCmd.tick_count++;
	}

	//g_TickbaseController.m_shift_data.m_current_shift = m_nTickbase;
	return true;
}

void InvokeRunSimulation(void* this_, float curtime, int cmdnum, CUserCmd* cmd, size_t local) {
	__asm {
		push local
		push cmd
		push cmdnum

		movss xmm2, curtime
		mov ecx, this_

		call Hooked::RunSimulationDetor.m_pOldFunction
	}
}

void TickbaseSystem::OnRunSimulation(void* this_, int iCommandNumber, CUserCmd* pCmd, size_t local) {
	g_pLocal = (void*)local;

	float curtime;
	__asm
	{
		movss curtime, xmm2
	}

	for (int i = 0; i < (int)g_iTickbaseShifts.size(); i++)
	{
		if ((g_iTickbaseShifts[i].cmdnum < iCommandNumber - s_iNetBackup) ||
			(g_iTickbaseShifts[i].cmdnum > iCommandNumber + s_iNetBackup))
		{
			g_iTickbaseShifts.erase(g_iTickbaseShifts.begin() + i);
			i--;
		}
	}

	int tickbase = -1;
	for (size_t i = 0; i < g_iTickbaseShifts.size(); i++)
	{

		//TO:DO this is completely wrong. needs redone.
		const auto& elem = g_iTickbaseShifts[i];

		if (elem.cmdnum == iCommandNumber)
		{
			tickbase = elem.tickbase;
			break;
		}
	}

	if (tickbase != -1 && local)
	{
		*(int*)(local + OFFSET_TICKBASE) = tickbase;
		curtime = tickbase * s_flTickInterval;
	}
	InvokeRunSimulation(this_, curtime, iCommandNumber, pCmd, local);
}

void TickbaseSystem::OnPredictionUpdate(void* prediction, void*, int startframe, bool validframe, int incoming_acknowledged, int outgoing_command) {
	typedef void(__thiscall* PredictionUpdateFn_t)(void*, int, bool, int, int);
	PredictionUpdateFn_t fn = (PredictionUpdateFn_t)Hooked::PredictionUpdateDetor.m_pOldFunction;
	fn(prediction, startframe, validframe, incoming_acknowledged, outgoing_command);

	if (s_bInMove && g_pLocal) {
		*(int*)((size_t)g_pLocal + OFFSET_TICKBASE) = s_iMoveTickBase;
	}

	if (g_pLocal) {
		for (size_t i = 0; i < g_iTickbaseShifts.size(); i++) {
			const auto& elem = g_iTickbaseShifts[i];

			if (elem.cmdnum == (outgoing_command + 1)) {
				*(int*)((size_t)g_pLocal + OFFSET_TICKBASE) = elem.tickbase;
				break;
			}
		}
	}
}
