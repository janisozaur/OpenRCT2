#pragma region Copyright (c) 2014-2016 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include "addresses.h"

#if defined(__GNUC__)
	#ifdef __clang__
		#define DISABLE_OPT __attribute__((noinline,optnone))
	#else
		#define DISABLE_OPT __attribute__((noinline,optimize("O0")))
	#endif // __clang__
#else
#define DISABLE_OPT
#endif // defined(__GNUC__)

int DISABLE_OPT RCT2_CALLPROC_X(int address, int _eax, int _ebx, int _ecx, int _edx, int _esi, int _edi, int _ebp)
{
	log_warning("enter");
	int result = 0;
//#if defined(PLATFORM_X86) && !defined(NO_RCT2)
	#ifdef _MSC_VER
	__asm {
		push ebp
		push address
		mov eax, _eax
		mov ebx, _ebx
		mov ecx, _ecx
		mov edx, _edx
		mov esi, _esi
		mov edi, _edi
		mov ebp, _ebp
		call [esp]
		lahf
		pop ebp
		pop ebp
		/* Load result with flags */
		mov result, eax
	}
	#else
	char *stack = malloc(100) + 100;
	// ensure stack is a 32 bit address
	if ((uintptr_t)stack != ((uintptr_t)stack & 0xFFFFFFFF)) {
		log_error("stack is not 32 bit");
		return 0;
	}
	__asm__ volatile ( "\
		\n\
		push %%rbx \n\
		push %%rbp \n\
		push %%r8  \n\
		push %%r9  \n\
		push %%r10  \n\
		push %%r11 \n\
		push %[address] 	\n\
		mov %[address], %%r9d \n\
		mov %[stack], %%r8 \n\
		mov %%r9d, (%%r8) \n\
		mov %[ebp], %%r9d	\n\
		mov %%rsp, -8(%%r8) \n\
		mov %[eax], %%eax 	\n\
		mov %[ebx], %%ebx 	\n\
		mov %[ecx], %%ecx 	\n\
		mov %[edx], %%edx 	\n\
		mov %[esi], %%esi 	\n\
		mov %[edi], %%edi 	\n\
		mov %%r9d, %%ebp	\n\
		mov %%rsp, %%r10 \n\
		mov %%r8, %%rsp \n\
		subq $8, %%rsp		\n\
		call .+5 \n\
		xx: \n\
		movl    $0x23, 4(%%rsp) \n\
		addl    $rt - xx, (%%rsp) \n\
		retf \n\
		rt: \n\
		// set up DS \n\
		// push %%eax \n\
		.byte 0x50 \n\
		// mov $0x2b, %%al \n\
		.byte 0xb0, 0x2b \n\
		// mov %%al, %%ds \n\
		.byte 0x8e, 0xd8 \n\
		// pop %%eax \n\
		.byte 0x58 \n\
		// now call the function \n\
		//call *(%%esp) 	\n\
		.byte 0xff, 0x54, 0x24, 0x08 \n\
		// reset DS \n\
		// xor %%eax, %%eax \n\
		.byte 0x31, 0xc0 \n\
		// mov %%eax, %%ds \n\
		.byte 0x8e, 0xd8 \n\
		/* call  .+5 */ \n\
		.byte 0xE8, 0, 0, 0, 0 \n\
		/* movl    $0x330033, 4(%%esp) */ \n\
		.byte 0xc7, 0x44, 0x24, 0x04, 0x33, 0, 0x33, 0 \n\
		/* add   dword ptr [esp], 13 */ \n\
		.byte 0x83, 4, 0x24, 0xd \n\
		/* retf */ \n\
		.byte 0xCB \n\
		mov %%r10, %%rsp \n\
		pop %%r11 \n\
		pop %%r10 \n\
		pop %%r9  \n\
		pop %%r8  \n\
		lahf \n\
		add $8, %%rsp 	\n\
		pop %%rbp \n\
		pop %%rbx \n\
		/* Load result with flags */ \n\
		mov %%eax, %[result] \n\
		" : [address] "+m" (address), [eax] "+m" (_eax), [ebx] "+m" (_ebx), [ecx] "+m" (_ecx), [edx] "+m" (_edx), [esi] "+m" (_esi), [edi] "+m" (_edi), [ebp] "+m" (_ebp), [result] "+m" (result), [stack] "+m" (stack)
		:
		: "eax","ecx","edx","esi","edi","memory"
	);
	#endif
//#endif // PLATFORM_X86
	log_warning("exit");
	// lahf only modifies ah, zero out the rest
	return result & 0xFF00;
}

int DISABLE_OPT RCT2_CALLFUNC_X(int address, int *_eax, int *_ebx, int *_ecx, int *_edx, int *_esi, int *_edi, int *_ebp)
{
	int result = 0;
#if defined(PLATFORM_X86) && !defined(NO_RCT2)
	#ifdef _MSC_VER
	__asm {
		// Store C's base pointer
		push ebp
		push ebx
		// Store address to call
		push address

		// Set all registers to the input values
		mov eax, [_eax]
		mov eax, [eax]
		mov ebx, [_ebx]
		mov ebx, [ebx]
		mov ecx, [_ecx]
		mov ecx, [ecx]
		mov edx, [_edx]
		mov edx, [edx]
		mov esi, [_esi]
		mov esi, [esi]
		mov edi, [_edi]
		mov edi, [edi]
		mov ebp, [_ebp]
		mov ebp, [ebp]

		// Call function
		call [esp]

		// Store output eax
		push eax
		push ebp
		push ebx
		mov ebp, [esp + 20]
		mov ebx, [esp + 16]

		// Get resulting ecx, edx, esi, edi registers

		mov eax, [_edi]
		mov [eax], edi
		mov eax, [_esi]
		mov [eax], esi
		mov eax, [_edx]
		mov [eax], edx
		mov eax, [_ecx]
		mov [eax], ecx

		// Pop ebx reg into ecx
		pop ecx
		mov eax, [_ebx]
		mov[eax], ecx

		// Pop ebp reg into ecx
		pop ecx
		mov eax, [_ebp]
		mov[eax], ecx

		pop eax
		// Get resulting eax register
		mov ecx, [_eax]
		mov [ecx], eax

		// Save flags as return in eax
		lahf
		// Pop address
		pop ebp

		pop ebx
		pop ebp
		/* Load result with flags */
		mov result, eax
	}
	#else
	__asm__ volatile ( "\
		\n\
		/* Store C's base pointer*/     \n\
		push %%ebp        \n\
		push %%ebx        \n\
		\n\
		/* Store %[address] to call*/   \n\
		push %[address]         \n\
		\n\
		/* Set all registers to the input values*/      \n\
		mov %[_eax], %%eax      \n\
		mov (%%eax), %%eax  \n\
		mov %[_ebx], %%ebx      \n\
		mov (%%ebx), %%ebx  \n\
		mov %[_ecx], %%ecx      \n\
		mov (%%ecx), %%ecx  \n\
		mov %[_edx], %%edx      \n\
		mov (%%edx), %%edx  \n\
		mov %[_esi], %%esi      \n\
		mov (%%esi), %%esi  \n\
		mov %[_edi], %%edi      \n\
		mov (%%edi), %%edi  \n\
		mov %[_ebp], %%ebp      \n\
		mov (%%ebp), %%ebp  \n\
		\n\
		/* Call function*/      \n\
		call *(%%esp)      \n\
		\n\
		/* Store output eax */ \n\
		push %%eax \n\
		push %%ebp \n\
		push %%ebx \n\
		mov 20(%%esp), %%ebp \n\
		mov 16(%%esp), %%ebx \n\
		/* Get resulting ecx, edx, esi, edi registers*/       \n\
		mov %[_edi], %%eax      \n\
		mov %%edi, (%%eax)  \n\
		mov %[_esi], %%eax      \n\
		mov %%esi, (%%eax)  \n\
		mov %[_edx], %%eax      \n\
		mov %%edx, (%%eax)  \n\
		mov %[_ecx], %%eax      \n\
		mov %%ecx, (%%eax)  \n\
		/* Pop ebx reg into ecx*/ \n\
		pop %%ecx		\n\
		mov %[_ebx], %%eax \n\
		mov %%ecx, (%%eax) \n\
		\n\
		/* Pop ebp reg into ecx */\n\
		pop %%ecx \n\
		mov %[_ebp], %%eax \n\
		mov %%ecx, (%%eax) \n\
		\n\
		pop %%eax \n\
		/* Get resulting eax register*/ \n\
		mov %[_eax], %%ecx \n\
		mov %%eax, (%%ecx) \n\
		\n\
		/* Save flags as return in eax*/  \n\
		lahf \n\
		/* Pop address*/ \n\
		pop %%ebp \n\
		\n\
		pop %%ebx \n\
		pop %%ebp \n\
		/* Load result with flags */ \n\
		mov %%eax, %[result] \n\
		" : [address] "+m" (address), [_eax] "+m" (_eax), [_ebx] "+m" (_ebx), [_ecx] "+m" (_ecx), [_edx] "+m" (_edx), [_esi] "+m" (_esi), [_edi] "+m" (_edi), [_ebp] "+m" (_ebp), [result] "+m" (result)

		:
		: "eax","ecx","edx","esi","edi","memory"
	);
	#endif
#endif // PLATFORM_X86
	// lahf only modifies ah, zero out the rest
	return result & 0xFF00;
}
