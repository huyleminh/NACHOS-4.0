// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synchconsole.h"
#include "main.h"
#include "syscall.h"
#include "ksyscall.h"

#define MAX_CHAR 2

char *User2System(int virtAddr, int limit)
{
	int oneChar;
	char *kernelBuffer = NULL;
	kernelBuffer = new char[limit + 1];
	if (kernelBuffer == NULL)
		return kernelBuffer;

	memset(kernelBuffer, 0, limit + 1);

	for (int i = 0; i < limit; i++)
	{
		kernel->machine->ReadMem(virtAddr + i, 1, &oneChar);
		kernelBuffer[i] = (char)oneChar;
		if (oneChar == 0)
			break;
	}
	return kernelBuffer;
}

int System2User(int virtAddr, int length, char *buffer)
{
	if (length < 0)
		return -1;
	if (length == 0)
		return 0;
	int i = 0;
	int oneChar = 0;
	do
	{
		oneChar = (int)buffer[i];
		kernel->machine->WriteMem(virtAddr + i, 1, oneChar);
		i++;
	} while (i < length && oneChar != 0);
	return i;
}
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions
//	is in machine.h.
//----------------------------------------------------------------------

void ExceptionHandler(ExceptionType which)
{
	int type = kernel->machine->ReadRegister(2);

	DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");

	switch (which)
	{
	case NoException:
	{
		return;
	}
	case PageFaultException:
	{
		DEBUG(dbgFile, "\nNo valid translation found");
		SysHalt();
		break;
	}
	case ReadOnlyException:
	{
		DEBUG(dbgFile, "\nWrite attemped to page marked read-only");
		SysHalt();
		break;
	}
	case BusErrorException:
	{
		DEBUG(dbgFile, "\nTranslation resulted invalid physical address");
		SysHalt();
		break;
	}
	case AddressErrorException:
	{
		DEBUG(dbgFile, "\nUnaligned reference or one that was beyond the end of the address space");
		SysHalt();
		break;
	}
	case OverflowException:
	{
		DEBUG(dbgFile, "\nInteger overflow in add or sub");
		SysHalt();
		break;
	}
	case IllegalInstrException:
	{
		DEBUG(dbgFile, "\nUnimplemented or reserved instr");
		SysHalt();
		break;
	}
	case NumExceptionTypes:
	{
		DEBUG(dbgFile, "\nNumber exception types");
		SysHalt();
		break;
	}
	case SyscallException:
		switch (type)
		{
		case SC_Halt:
		{
			DEBUG(dbgSys, "Shutdown, initiated by user program.\n");
			SysHalt();
			ASSERTNOTREACHED();
			break;
		}
		case SC_Add:
		{
			DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5) << "\n");
			/* Process SysAdd Systemcall*/
			int result;
			result = SysAdd(/* int op1 */ (int)kernel->machine->ReadRegister(4),
							(int)kernel->machine->ReadRegister(5)); /* int op2 */
			DEBUG(dbgSys, "Add returning with " << result << "\n");
			/* Prepare Result */
			kernel->machine->WriteRegister(2, (int)result);
			/* Modify return point */
			{
				/* set previous programm counter (debugging only)*/
				kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

				/* set programm counter to next instruction (all Instructions are 4 byte wide)*/
				kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

				/* set next programm counter for brach execution */
				kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
			}
			cout << "Result is " << result << endl;
			return;
			ASSERTNOTREACHED();
			break;
		}
		case SC_Exit:
		{
			DEBUG(dbgAddr, "Program exit\n");
			int re = kernel->machine->ReadRegister(4);
			cout << "Return value" << re << endl;
			kernel->currentThread->Finish();
			break;
		}
		case SC_Close:
		{

			//Input: ID cua file (OpenFileID)
			//Output: 0: thanh cong, -1: that bai
			int id = kernel->machine->ReadRegister(4);
			if (id >= 0 && id <= 14)
			{
				//FileSystem *fileSystem = new FileSystem();
				if (kernel->fileSystem->Open((char *)id))
				{
					delete kernel->fileSystem->Open((char *)id);
					//kernel->fileSystem->Open((char*)id) = NULL;
					kernel->machine->WriteRegister(2, 0);
					break;
				}
			}
			kernel->machine->WriteRegister(2, -1);
			break;
		}
		case SC_Create:
		{
			int virtAddr;
			char *filename;
			DEBUG(dbgFile, "\nSC_Create call...");
			DEBUG(dbgFile, "\nReading virtual address of filename");
			virtAddr = kernel->machine->ReadRegister(4);
			DEBUG(dbgFile, "\nReading filename");
			filename = User2System(virtAddr, 32 + 1); //max length = 32
			if (strlen(filename) == 0)
			{
				DEBUG(dbgFile, "\nFile name is not valid");
				kernel->machine->WriteRegister(2, -1);
				break;
			}
			if (filename == NULL)
			{
				DEBUG(dbgFile, "\nNot enough memory in system");
				kernel->machine->WriteRegister(2, -1);
				delete filename;
				break;
			}
			DEBUG(dbgFile, "\nFinish reading filename.");
			if (!kernel->fileSystem->Create(filename, 0))
			{
				kernel->machine->WriteRegister(2, -1);
				delete filename;
				break;
			}
			kernel->machine->WriteRegister(2, 0);
			delete filename;
			break;
		}

		case SC_ReadChar:
		{
			char *buffers = new char[MAX_CHAR + 1];

			int i = 0;
			while (true)
			{
				char inputCh = kernel->synchConsoleIn->GetChar();
				buffers[i++] = inputCh;
				if (inputCh == '\n')
					break;
				if (i == MAX_CHAR)
					break;
			}

			//Put the first character into register
			kernel->machine->WriteRegister(2, (int)buffers[0]);

			{
				/* set previous programm counter (debugging only)*/
				kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

				/* set programm counter to next instruction (all Instructions are 4 byte wide)*/
				kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

				/* set next programm counter for brach execution */
				kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
			}
			delete buffers;
			return;
		}

		case SC_PrintChar:
		{
			char ch = (char)kernel->machine->ReadRegister(4);
			kernel->synchConsoleOut->PutChar(ch);

			{
				/* set previous programm counter (debugging only)*/
				kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

				/* set programm counter to next instruction (all Instructions are 4 byte wide)*/
				kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

				/* set next programm counter for brach execution */
				kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
			}
			return;
		}

		default:
			cerr << "Unexpected system call " << type << "\n";
			break;
		}
		break;
	default:
		cerr << "Unexpected user mode exception" << (int)which << "\n";
		break;
	}
	ASSERTNOTREACHED();
}
