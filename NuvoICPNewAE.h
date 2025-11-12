#pragma once

#include <asf.h>

bool NuvoICP_Protocol_Command(void);
		#if defined(INCLUDE_FROM_NUVOICP_C)
			void NuvoICP_Reentry(void);
			void NuvoICP_Reentry_glitch(void);
            static void NuvoICP_EnterProgMode(void);
			static void NuvoICP_LeaveProgMode(void);
			static uint16_t NuvoICP_GetParam(uint8_t cmd, uint8_t *buf);
			static void NuvoICP_Mass_Erase(void);
			static bool NuvoICP_WriteMemory(uint8_t * buf, uint8_t * status_payload);
			static void NuvoICP_ReadMemory(uint8_t * buf);
			static void NuvoICP_Page_Erase(uint8_t * buf);
		#endif
