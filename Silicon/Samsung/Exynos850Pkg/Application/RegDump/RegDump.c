/** @file
  Read-only register dump for the Samsung Galaxy A14 4G (SM-A145F, Exynos850).

  Port of the U-Boot "regdump" bootmenu entry (0004-a145f-regdump). Addresses
  come from the stock live DTB. Only MmioRead32 is used: it cannot change
  silicon state.

  Usage (UEFI Shell):
    RegDump.efi                 dump all blocks
    RegDump.efi chipid pmu      dump only the named blocks
    RegDump.efi -h              help
    RegDump.efi > fs0:\regdump.txt     save the output to the USB stick

  Notes
  - Each block's header is printed BEFORE it is read. If a block's clock/power
    domain is gated, the bus access can hang the phone; the last header on
    screen then tells you which block it was (reboot and skip it by name).
  - This is NOT the eFUSE/OTP controller. On Exynos850 that is TrustZone-only
    and not mapped to the non-secure world.

  2026 update: index 5 of the CHIPID block (offset 0x14, i.e. B->Base+0x14 =
  0x10000014) hit a Synchronous External Abort (ESR 0x96000010, FAR
  0x10000014) that crashed the whole firmware - RegDump.efi was launched
  from the UiApp boot menu via SetupBrowser, so the abort took DxeCore down
  with it and the only way out was a hard reset. CHIPID's real register
  file on this SoC does not cover every word of the 0x100-byte range its
  DT reg property reserves; past some point reads fall into a decode hole
  that the internal bus answers with an error response instead of 0, and
  an unhandled SEA at EL1 is fatal by default.

  Fix: each read now goes through SafeMmioRead32(), which installs a
  temporary EFI_CPU_ARCH_PROTOCOL synchronous-exception handler before
  reading and uses SetJump/LongJump to unwind back out of the read if that
  handler fires, instead of letting the abort propagate. A faulting word is
  printed as "FAULT" and the dump moves on to the next word/block rather
  than taking the firmware down. This makes it safe to leave the original
  (generous) per-block word counts in place - they're useful for seeing
  what, if anything, lives past the documented registers - without betting
  the whole UEFI session on every one of those words actually being wired
  up.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/Cpu.h>
#include <Protocol/ShellParameters.h>

STATIC BASE_LIBRARY_JUMP_BUFFER  mFaultJumpBuffer;
STATIC volatile BOOLEAN          mInGuardedRead;
STATIC EFI_CPU_ARCH_PROTOCOL     *mCpu;

/**
  Registered as the AArch64 synchronous-exception handler only while a
  SafeMmioRead32() is in flight. If we are in one (mInGuardedRead), unwind
  straight back to the SetJump() in SafeMmioRead32() instead of letting the
  abort fall through to the platform's default handler (which dumps the
  exception and hangs). If we somehow got called while NOT in a guarded
  read, this isn't our fault to handle - fall through by doing nothing, so
  whatever the normal/default handler chain is still gets to run.
**/
STATIC
VOID
EFIAPI
GuardedReadExceptionHandler (
  IN EFI_EXCEPTION_TYPE  ExceptionType,
  IN OUT EFI_SYSTEM_CONTEXT  SystemContext
  )
{
  if (!mInGuardedRead) {
    return;
  }

  mInGuardedRead = FALSE;
  LongJump (&mFaultJumpBuffer, 1);
}

/**
  MmioRead32(), but a Synchronous External Abort on the read is caught
  instead of crashing the firmware.

  @param[in]  Address  The MMIO address to read.
  @param[out] Value    Set to the read value on success. Left unchanged on
                        fault.

  @retval TRUE   The read completed normally; *Value is valid.
  @retval FALSE  The read faulted (SEA/data abort); *Value was not touched.
**/
STATIC
BOOLEAN
SafeMmioRead32 (
  IN  UINTN   Address,
  OUT UINT32  *Value
  )
{
  if ((mCpu == NULL) || (SetJump (&mFaultJumpBuffer) != 0)) {
    //
    // Either we have no EFI_CPU_ARCH_PROTOCOL (so we cannot install a
    // handler at all - treat every read as a potential fault and refuse
    // to risk it), or SetJump() just returned here *for the second time*,
    // via LongJump() out of the exception handler above: the read faulted.
    //
    mInGuardedRead = FALSE;
    return FALSE;
  }

  mInGuardedRead = TRUE;
  *Value         = MmioRead32 (Address);
  mInGuardedRead = FALSE;
  return TRUE;
}

typedef struct {
  CONST CHAR16  *Key;
  CONST CHAR16  *Title;
  CONST CHAR16  *Note;
  UINT32        Base;
  UINT32        Words;
} REG_BLOCK;

STATIC CONST REG_BLOCK  mBlocks[] = {
  { L"chipid", L"CHIPID",
    L"product/package ID and revision (samsung,s5e3830-chipid)",
    0x10000000, 0x40 },
  { L"pmu",    L"PMU_ALIVE (first 0x200 bytes)",
    L"reset-reason / status registers; the rest of the 64 KiB block is power-domain control",
    0x11860000, 0x80 },
  { L"tmu",    L"TMU (shared by the three thermal probes)",
    L"includes TRIMINFO; DTB has samsung,tmu_efuse_value = 0x37 for all three probes",
    0x10070000, 0x80 },
  { L"adc",    L"ADC (exynos-adc-v3)",
    L"controller registers only, nothing is sampled",
    0x11C40000, 0x40 },
};

STATIC
VOID
DumpBlock (
  IN CONST REG_BLOCK  *B
  )
{
  UINT32  Index;
  UINT32  Value;
  UINTN   FaultCount;

  //
  // Header first: if a read below hangs the bus outright (as opposed to
  // faulting cleanly), this is the last thing seen.
  //
  Print (L"===== %s @ 0x%08X =====\n", B->Title, B->Base);
  Print (L"  (%s)\n", B->Note);

  FaultCount = 0;

  for (Index = 0; Index < B->Words; Index++) {
    if ((Index % 4) == 0) {
      Print (L"%08X:", B->Base + (Index * 4));
    }

    if (SafeMmioRead32 ((UINTN)B->Base + (Index * 4), &Value)) {
      Print (L" %08X", Value);
    } else {
      Print (L"    FAULT");
      FaultCount++;
    }

    if ((Index % 4) == 3) {
      Print (L"\n");
    }
  }

  if ((B->Words % 4) != 0) {
    Print (L"\n");
  }

  if (FaultCount != 0) {
    Print (L"  (%d word%a caused a bus/external abort - unmapped or unpowered SFR, skipped)\n",
           FaultCount,
           (FaultCount == 1) ? "" : "s"
           );
  }
}

STATIC
VOID
UnregisterGuard (
  VOID
  )
{
  if (mCpu != NULL) {
    mCpu->RegisterInterruptHandler (mCpu, EXCEPT_AARCH64_SYNCHRONOUS_EXCEPTIONS, NULL);
    mCpu = NULL;
  }
}

STATIC
VOID
Usage (
  VOID
  )
{
  UINTN  I;

  Print (L"RegDump - read-only SM-A145F register dump\n");
  Print (L"  RegDump.efi [all | block ...]\n  blocks:");
  for (I = 0; I < ARRAY_SIZE (mBlocks); I++) {
    Print (L" %s", mBlocks[I].Key);
  }

  Print (L"\n  Save output:  RegDump.efi > fs0:\\regdump.txt\n");
}

EFI_STATUS
EFIAPI
RegDumpMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_SHELL_PARAMETERS_PROTOCOL  *Params;
  EFI_STATUS                     Status;
  BOOLEAN                        Selected[ARRAY_SIZE (mBlocks)];
  BOOLEAN                        Any;
  UINTN                          Arg;
  UINTN                          I;

  mCpu = NULL;
  Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **)&mCpu);
  if (EFI_ERROR (Status) || (mCpu == NULL)) {
    Print (L"warning: no EFI_CPU_ARCH_PROTOCOL - cannot guard against bus faults, refusing all reads\n");
    mCpu = NULL;
  } else {
    Status = mCpu->RegisterInterruptHandler (
                      mCpu,
                      EXCEPT_AARCH64_SYNCHRONOUS_EXCEPTIONS,
                      GuardedReadExceptionHandler
                      );
    if (EFI_ERROR (Status)) {
      Print (L"warning: could not install exception guard (%r) - refusing all reads\n", Status);
      mCpu = NULL;
    }
  }

  Params = NULL;
  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiShellParametersProtocolGuid,
                  (VOID **)&Params
                  );

  ZeroMem (Selected, sizeof (Selected));
  Any = FALSE;

  if (!EFI_ERROR (Status) && (Params != NULL)) {
    for (Arg = 1; Arg < Params->Argc; Arg++) {
      CONST CHAR16  *A = Params->Argv[Arg];

      if ((StrCmp (A, L"-h") == 0) || (StrCmp (A, L"--help") == 0) || (StrCmp (A, L"-?") == 0)) {
        Usage ();
        UnregisterGuard ();
        return EFI_SUCCESS;
      }

      if (StrCmp (A, L"all") == 0) {
        for (I = 0; I < ARRAY_SIZE (mBlocks); I++) {
          Selected[I] = TRUE;
        }

        Any = TRUE;
        continue;
      }

      for (I = 0; I < ARRAY_SIZE (mBlocks); I++) {
        if (StrCmp (A, mBlocks[I].Key) == 0) {
          Selected[I] = TRUE;
          Any         = TRUE;
          break;
        }
      }

      if (I == ARRAY_SIZE (mBlocks)) {
        Print (L"unknown block '%s'\n", A);
        Usage ();
        UnregisterGuard ();
        return EFI_INVALID_PARAMETER;
      }
    }
  }

  if (!Any) {
    for (I = 0; I < ARRAY_SIZE (mBlocks); I++) {
      Selected[I] = TRUE;
    }
  }

  for (I = 0; I < ARRAY_SIZE (mBlocks); I++) {
    if (Selected[I]) {
      DumpBlock (&mBlocks[I]);
    }
  }

  Print (L"done. True eFUSE/OTP controller is TrustZone-only, not shown.\n");
  UnregisterGuard ();
  return EFI_SUCCESS;
}
