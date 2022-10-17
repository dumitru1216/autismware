#include "../Hooked.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../Features/Rage/TickbaseShift.hpp"

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

#define nigger 0
bool __fastcall Hooked::WriteUsercmdDeltaToBuffer(void* ECX, void* EDX, int nSlot, void* buffer, int o_from, int o_to, bool isnewcommand) {

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

	// g_tickbase.m_shift_data.m_current_shift = m_nTickbase;
	return true;
}
