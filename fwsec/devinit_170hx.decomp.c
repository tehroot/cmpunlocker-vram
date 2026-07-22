// Decompilation: devinit_170hx_imem.bin  (falcon:LE:32:v5)
// ===== FwSecEntry @ imem:00000000 =====

/* WARNING: This function may have set the stack pointer */

void FwSecEntry(void)

{
  uDmemfffffffc = 0x3c;
  FUN_imem_000072b1();
  do {
    exit();
  } while( true );
}



// ===== FUN_imem_00000046 @ imem:00000046 =====

void FUN_imem_00000046(void)

{
  return;
}



// ===== FUN_imem_00000715 @ imem:00000715 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00000715(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00000939 @ imem:00000939 =====

void FUN_imem_00000939(void)

{
  code *UNRECOVERED_JUMPTABLE;
  
                    /* WARNING: Could not recover jumptable at 0x00000939. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (*UNRECOVERED_JUMPTABLE)();
  return;
}



// ===== thunk_FUN_imem_00000939 @ imem:00000941 =====

void thunk_FUN_imem_00000939(void)

{
  code *UNRECOVERED_JUMPTABLE;
  
                    /* WARNING: Could not recover jumptable at 0x00000939. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (*UNRECOVERED_JUMPTABLE)();
  return;
}



// ===== FUN_imem_0000097b @ imem:0000097b =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000097b(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000009e4 @ imem:000009e4 =====

uint FUN_imem_000009e4(uint param_1)

{
  uint in_r9;
  
  return param_1 | in_r9;
}



// ===== FUN_imem_00000a3a @ imem:00000a3a =====

int FUN_imem_00000a3a(int param_1)

{
  uint unaff_r0;
  uint unaff_r1;
  int unaff_r2;
  uint unaff_r3;
  uint unaff_r4;
  uint unaff_r5;
  uint *puVar1;
  
  do {
    if ((int)(unaff_r1 - param_1) < unaff_r2) {
      unaff_r2 = unaff_r1 - param_1;
    }
    do {
      if (unaff_r0 == unaff_r3) {
        return unaff_r2;
      }
      puVar1 = (uint *)(unaff_r0 | unaff_r5);
      unaff_r0 = unaff_r0 + 4;
      unaff_r1 = *puVar1 & unaff_r4;
    } while (unaff_r1 == 0);
    param_1 = func_0x000009b9();
  } while( true );
}



// ===== FUN_imem_00000b68 @ imem:00000b68 =====

void FUN_imem_00000b68(void)

{
  int *in_r9;
  int in_r15;
  
  if (in_r15 != *in_r9) {
    FUN_imem_000072af();
  }
  return;
}



// ===== FUN_imem_00000bda @ imem:00000bda =====

undefined4 FUN_imem_00000bda(byte param_1)

{
  undefined4 uVar1;
  
  uVar1 = 0xffffffff;
  if (param_1 < 10) {
    uVar1 = CONCAT31(0xffffff,*(undefined1 *)(param_1 + 0x6c));
  }
  return uVar1;
}



// ===== FUN_imem_00000bfa @ imem:00000bfa =====

void FUN_imem_00000bfa(void)

{
  uint in_r9;
  
  func_0x00000baa(*(undefined1 *)((in_r9 & 0xff) + 0x76));
  return;
}



// ===== FUN_imem_00000e0e @ imem:00000e0e =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00000e0e(void)

{
  int iStackX_2c;
  
  func_0x00000cde();
  if (iStackX_2c != _DAT_dmem_00000714) {
    FUN_imem_00000e0e(0x32);
    return;
  }
  return;
}



// ===== FUN_imem_00000e15 @ imem:00000e15 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00000e15(void)

{
  int iStackX_2c;
  
  func_0x00000cde();
  if (iStackX_2c != _DAT_dmem_00000714) {
    FUN_imem_00000e0e();
    return;
  }
  return;
}



// ===== FUN_imem_00000e6f @ imem:00000e6f =====

bool FUN_imem_00000e6f(void)

{
  int unaff_r0;
  uint unaff_r1;
  undefined1 unaff_r2b;
  uint *in_r9;
  uint uVar1;
  uint in_r14;
  int in_r15;
  
  *in_r9 = unaff_r0 << 0xb | unaff_r1;
  *(undefined4 *)(in_r15 << 5 | in_r14) = 0x8000001b;
  uVar1 = FUN_imem_00000bda(unaff_r2b);
  func_0x00000cde(unaff_r2b);
  return (uVar1 & 0x60000000) == 0;
}



// ===== FUN_imem_00001000 @ imem:00001000 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00001000(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int in_r9;
  int iStackX_c;
  
  *(undefined4 *)(in_r9 + 10) = param_3;
  *(undefined4 *)(in_r9 + 0xb) = param_4;
  iStackX_c = _DAT_dmem_00000714;
  FUN_imem_00000e6f(param_1,param_2,(undefined4 *)(in_r9 + 10),2,1);
  if (iStackX_c != _DAT_dmem_00000714) {
    FUN_imem_000072af();
  }
  return;
}



// ===== FUN_imem_0000104c @ imem:0000104c =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000104c(void)

{
  char unaff_r8b;
  undefined4 in_r9;
  undefined4 *in_r15;
  
  *in_r15 = in_r9;
  if (unaff_r8b == -0xe) {
                    /* WARNING: Bad instruction - Truncating control flow here */
    halt_baddata();
  }
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00001062 @ imem:00001062 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00001062(void)

{
  char unaff_r8b;
  undefined4 in_r9;
  undefined4 *in_r14;
  
  *in_r14 = in_r9;
  if ((short)in_r9 == 0) {
    func_0x00000b85();
    return;
  }
  if (unaff_r8b == -0xe) {
                    /* WARNING: Bad instruction - Truncating control flow here */
    halt_baddata();
  }
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00001523 @ imem:00001523 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00001523(void)

{
  int *in_r9;
  
  if (*in_r9 != _DAT_dmem_00000714) {
    FUN_imem_000072af();
  }
  return;
}



// ===== FUN_imem_00001748 @ imem:00001748 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00001748(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000018ee @ imem:000018ee =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_000018ee(void)

{
  undefined4 *in_r15;
  int iStackX_4;
  
  *in_r15 = 0x1d000000;
  FUN_imem_00001748();
  if (iStackX_4 != _DAT_dmem_00000714) {
    FUN_imem_000072af();
  }
  return;
}



// ===== FUN_imem_00001d49 @ imem:00001d49 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00001d49(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00001d5b @ imem:00001d5b =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00001d5b(uint *param_1,undefined4 param_2,int param_3,int param_4,uint param_5)

{
  uint uVar1;
  int in_r9;
  uint in_r15;
  
  while (param_5 < in_r15) {
    uVar1 = param_5 & 0x1f;
    param_5 = param_5 + 1;
    if ((*(byte *)(in_r9 + param_4 + 1) & 1) == 0) {
      *param_1 = *param_1 | param_3 << uVar1;
    }
    param_4 = 0xf0;
    in_r9 = param_5 * 7;
  }
  *(uint *)(_DAT_dmem_000051c8 + 0xc) = *(uint *)(_DAT_dmem_000051c8 + 0xc) | 0x8000000;
  return;
}



// ===== FUN_imem_00001e1a @ imem:00001e1a =====

void FUN_imem_00001e1a(void)

{
  uint in_r9;
  uint *in_r14;
  
  *in_r14 = in_r9 & 0xff00ffff | 0x80000;
  return;
}



// ===== FUN_imem_00001f77 @ imem:00001f77 =====

/* WARNING: Control flow encountered bad instruction data */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00001f77(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000020a1 @ imem:000020a1 =====

/* WARNING: Control flow encountered bad instruction data */

undefined4 FUN_imem_000020a1(int param_1,undefined4 param_2,undefined4 param_3,char param_4)

{
  int *unaff_r0;
  int in_r9;
  
  if (0x400 < (uint)(in_r9 + param_1)) {
    return 4;
  }
  if (param_4 != '\0') {
    *unaff_r0 = param_1 + 0x51d8;
                    /* WARNING: Bad instruction - Truncating control flow here */
    halt_baddata();
  }
  FUN_imem_00007573(param_2,param_1 + 0x51d8,0x2a);
  return 0x1f;
}



// ===== FUN_imem_000023b4 @ imem:000023b4 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000023b4(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000023bb @ imem:000023bb =====

/* WARNING: Instruction at (imem,0x00002392) overlaps instruction at (imem,0x00002391)
    */
/* WARNING: Control flow encountered bad instruction data */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_000023bb(uint param_1)

{
  uint *unaff_r2;
  int unaff_r3;
  uint *unaff_r4;
  int unaff_r5;
  int iVar1;
  char cVar2;
  char cVar3;
  undefined4 uVar4;
  int iStackX_34;
  
  for (; unaff_r3 != unaff_r5; unaff_r3 = unaff_r3 + -1) {
    param_1 = *unaff_r4;
    if ((param_1 & 1) == 0) {
      cVar2 = FUN_imem_000020a1();
      cVar3 = FUN_imem_000020a1(unaff_r4[1]);
      uVar4 = FUN_imem_000020a1(unaff_r4[2]);
      cVar3 = (char)uVar4 + cVar3;
      param_1 = CONCAT31((int3)((uint)uVar4 >> 8),cVar3);
      if ((char)(cVar2 + cVar3) == '\0') {
        iVar1 = unaff_r3 * 0xc;
        *unaff_r2 = (uint)*(ushort *)(iVar1 + 0xc38);
        *(uint *)((int)unaff_r2 + 2) = (uint)*(ushort *)(iVar1 + 0xc3c);
        unaff_r2[1] = (uint)*(ushort *)(iVar1 + 0xc38);
        uVar4 = CONCAT22((short)((uint)(iVar1 + 0xba0) >> 0x10),*(undefined2 *)(iVar1 + 0xc3c));
        *(undefined4 *)((int)unaff_r2 + 6) = uVar4;
        todo(uVar4,0xb6);
                    /* WARNING: Bad instruction - Truncating control flow here */
        halt_baddata();
      }
    }
    unaff_r4 = unaff_r4 + -3;
  }
  if (iStackX_34 != _DAT_dmem_00000714) {
    FUN_imem_000023b4(param_1 & 0xffffff00);
    return;
  }
  return;
}



// ===== FUN_imem_0000274f @ imem:0000274f =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000274f(undefined4 param_1,uint *param_2,undefined4 param_3,undefined4 param_4,
                      undefined1 param_5)

{
  int in_r9;
  uint in_r15;
  int iStackX_18;
  
  if ((*(uint *)(in_r9 + 0xc) & in_r15) != 0) {
    param_5 = 9;
    *param_2 = *param_2 | 0xf;
  }
  if (iStackX_18 != _DAT_dmem_00000714) {
    FUN_imem_000072af(param_5);
  }
  return;
}



// ===== FUN_imem_00002814 @ imem:00002814 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00002814(void)

{
  undefined1 unaff_r8b;
  
  todo(unaff_r8b,0xf8);
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000029c0 @ imem:000029c0 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_000029c0(char param_1)

{
  undefined4 *unaff_r0;
  undefined4 *unaff_r1;
  undefined4 *unaff_r2;
  undefined2 uVar1;
  int iStackX_14;
  
  uVar1 = todo((short)unaff_r2,0);
  if (param_1 == '\x1f') {
    *unaff_r1 = unaff_r0[1];
    *unaff_r2 = *unaff_r0;
  }
  if (iStackX_14 != _DAT_dmem_00000714) {
    FUN_imem_000072af(param_1,uVar1,0x7e);
  }
  return;
}



// ===== FUN_imem_000029f0 @ imem:000029f0 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 FUN_imem_000029f0(undefined4 *param_1)

{
  uint uVar1;
  
  uVar1 = _DAT_dmem_14001478;
  *param_1 = 0;
  if ((uVar1 & 1) != 0) {
    *param_1 = 1;
  }
  return 0x1f;
}



// ===== FUN_imem_00002a7e @ imem:00002a7e =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00002a7e(void)

{
  uint *unaff_r0;
  int in_r9;
  int iStackX_c;
  
  if (*(char *)(in_r9 + 9) != '\0') {
    *unaff_r0 = *unaff_r0 | 0x20;
  }
  if (iStackX_c != _DAT_dmem_00000714) {
    FUN_imem_000072af(0x1f);
  }
  return;
}



// ===== FUN_imem_00002bb5 @ imem:00002bb5 =====

undefined4 FUN_imem_00002bb5(undefined4 param_1,undefined4 param_2,undefined4 *param_3)

{
  undefined4 *in_r9;
  
  *param_3 = *in_r9;
  param_3[1] = in_r9[1];
  return 0x1f;
}



// ===== FUN_imem_00002c19 @ imem:00002c19 =====

/* WARNING: Removing unreachable block (imem,0x00002c05) */
/* WARNING: Removing unreachable block (imem,0x00002c33) */
/* WARNING: Removing unreachable block (imem,0x00002c0b) */

undefined4 FUN_imem_00002c19(int param_1,undefined4 *param_2)

{
  if (param_1 != 5) {
    return 6;
  }
  *param_2 = 2;
  return 0x1f;
}



// ===== FUN_imem_00002dd4 @ imem:00002dd4 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00002dd4(void)

{
  int *unaff_r5;
  int in_r13;
  uint in_r15;
  int iStackX_2c;
  
  *unaff_r5 = (in_r15 >> 8 & 0x7f) + (in_r15 >> 0x10 & 0x7f) + (in_r15 & 0xff) + in_r13;
  if (iStackX_2c != _DAT_dmem_00000714) {
    FUN_imem_000072af();
  }
  return;
}



// ===== FUN_imem_0000302f @ imem:0000302f =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000302f(void)

{
  uint *unaff_r7;
  undefined4 *unaff_r8;
  undefined4 in_r9;
  uint in_r13;
  uint in_r14;
  uint *puStackX_24;
  int iStackX_3c;
  
  *unaff_r8 = in_r9;
  *puStackX_24 = in_r14 >> 6;
  *unaff_r7 = in_r13 & 0xff000008;
  if (iStackX_3c != _DAT_dmem_00000714) {
    FUN_imem_000072af();
  }
  return;
}



// ===== FUN_imem_00003293 @ imem:00003293 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00003293(void)

{
  int *unaff_r0;
  uint *unaff_r1;
  int in_r9;
  char cVar1;
  int iStackX_c;
  
  *unaff_r0 = in_r9;
  cVar1 = FUN_imem_0000302f();
  if (cVar1 == '\x1f') {
    *unaff_r1 = (uint)(*unaff_r0 == 2);
  }
  if (iStackX_c != _DAT_dmem_00000714) {
    FUN_imem_000072af();
  }
  return;
}



// ===== FUN_imem_000034b6 @ imem:000034b6 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000034b6(void)

{
  undefined4 unaff_r0;
  
  todo(unaff_r0,0x32);
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000039cf @ imem:000039cf =====

void FUN_imem_000039cf(undefined4 param_1,undefined1 param_2)

{
  undefined1 unaff_r2b;
  
  do {
    param_2 = todo(param_2,unaff_r2b);
    param_1 = FUN_imem_000034b6(param_1,param_2);
  } while( true );
}



// ===== FUN_imem_000039e9 @ imem:000039e9 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_000039e9(undefined4 param_1,undefined1 param_2)

{
  undefined1 unaff_r2b;
  undefined4 *unaff_r3;
  undefined4 uVar1;
  
  io_read(unaff_r3);
  uVar1 = 0x1f;
  *unaff_r3 = *(undefined4 *)(_DAT_dmem_000051c8 + 0x80);
  do {
    param_2 = todo(param_2,unaff_r2b);
    uVar1 = FUN_imem_000034b6(uVar1,param_2);
  } while( true );
}



// ===== thunk_FUN_imem_000039cf @ imem:000039f9 =====

void thunk_FUN_imem_000039cf(undefined4 param_1,undefined1 param_2)

{
  undefined1 unaff_r2b;
  
  do {
    param_2 = todo(param_2,unaff_r2b);
    param_1 = FUN_imem_000034b6(param_1,param_2);
  } while( true );
}



// ===== FUN_imem_00003a41 @ imem:00003a41 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00003a41(undefined4 param_1,undefined4 *param_2)

{
  undefined4 unaff_r8;
  
  *param_2 = unaff_r8;
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00003d1e @ imem:00003d1e =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00003d1e(void)

{
  int unaff_r5;
  
  io_read(unaff_r5 + 0x264);
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00003d20 @ imem:00003d20 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00003d20(void)

{
  int unaff_r5;
  
  io_read(unaff_r5 + 0x264);
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000042e1 @ imem:000042e1 =====

/* WARNING: Removing unreachable block (imem,0x000042dd) */
/* WARNING: Removing unreachable block (imem,0x000042ef) */

undefined4 FUN_imem_000042e1(void)

{
  int in_r9;
  undefined4 uVar1;
  
  uVar1 = 4;
  if (in_r9 == 0xa5) {
    uVar1 = 8;
  }
  return uVar1;
}



// ===== FUN_imem_00004441 @ imem:00004441 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00004441(undefined4 param_1,undefined4 param_2)

{
  int unaff_r0;
  int *piVar1;
  undefined4 *unaff_r1;
  int in_r9;
  int iVar2;
  int iStackX_c;
  
  piVar1 = (int *)(unaff_r0 + 8);
  *piVar1 = in_r9;
  iVar2 = func_0x00004372(param_1,param_2,piVar1);
  if (iVar2 == 0) {
    *unaff_r1 = CONCAT22((short)((uint)*piVar1 >> 0x10),*(undefined2 *)(*piVar1 + 4));
  }
  if (iStackX_c != _DAT_dmem_00000714) {
    FUN_imem_000072af();
  }
  return;
}



// ===== FUN_imem_00004486 @ imem:00004486 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00004486(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00005204 @ imem:00005204 =====

undefined4 FUN_imem_00005204(uint *param_1,uint param_2,uint *param_3)

{
  uint in_r9;
  uint uVar1;
  uint uVar2;
  
  do {
    uVar1 = *param_1;
    in_r9 = in_r9 + 1;
    param_1 = param_1 + 1;
    if (uVar1 != 0) {
      uVar2 = 0;
      do {
        uVar2 = uVar2 + 1;
        uVar1 = uVar1 & uVar1 - 1;
      } while (uVar1 != 0);
      uVar2 = uVar2 & 1;
      *param_3 = uVar2;
      do {
        *param_3 = uVar2;
        uVar2 = 0;
      } while( true );
    }
  } while (in_r9 < param_2);
  return 0;
}



// ===== FUN_imem_00005209 @ imem:00005209 =====

undefined4
FUN_imem_00005209(uint *param_1,uint param_2,uint *param_3,undefined4 param_4,uint param_5)

{
  uint in_r9;
  uint uVar1;
  
  while( true ) {
    param_1 = param_1 + 1;
    if (param_5 != 0) {
      uVar1 = 0;
      do {
        uVar1 = uVar1 + 1;
        param_5 = param_5 & param_5 - 1;
      } while (param_5 != 0);
      uVar1 = uVar1 & 1;
      *param_3 = uVar1;
      do {
        *param_3 = uVar1;
        uVar1 = 0;
      } while( true );
    }
    if (param_2 <= in_r9) break;
    param_5 = *param_1;
    in_r9 = in_r9 + 1;
  }
  return 0;
}



// ===== FUN_imem_00005655 @ imem:00005655 =====

void FUN_imem_00005655(void)

{
  int *unaff_r0;
  uint unaff_r2;
  short sVar1;
  int iVar2;
  uint uVar3;
  int in_r9;
  
  while( true ) {
    unaff_r0[4] = in_r9;
    iVar2 = 0x10 << (unaff_r2 & 0x1f);
    unaff_r0[4] = iVar2 + 4;
    uVar3 = iVar2 - 0x55d4;
    if (uVar3 >> 9 == 0x7fffd5) break;
    in_r9 = 0;
  }
  sVar1 = (short)unaff_r0[5] + -1;
  unaff_r0[5] = CONCAT22((ushort)(uVar3 >> 0x19),sVar1);
  if (sVar1 == 0) {
    *(int *)(*unaff_r0 + 4) = unaff_r0[1];
    *(int *)unaff_r0[1] = *unaff_r0;
  }
  return;
}



// ===== FUN_imem_0000565b @ imem:0000565b =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000565b(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00005661 @ imem:00005661 =====

void FUN_imem_00005661(void)

{
  int *unaff_r0;
  short sVar1;
  undefined4 in_r9;
  
  sVar1 = (short)unaff_r0[5] + -1;
  unaff_r0[5] = CONCAT22((short)((uint)in_r9 >> 0x10),sVar1);
  if (sVar1 == 0) {
    *(int *)(*unaff_r0 + 4) = unaff_r0[1];
    *(int *)unaff_r0[1] = *unaff_r0;
  }
  return;
}



// ===== FUN_imem_0000567b @ imem:0000567b =====

void FUN_imem_0000567b(void)

{
  undefined4 unaff_r0;
  undefined4 *in_r9;
  
  *in_r9 = unaff_r0;
  return;
}



// ===== FUN_imem_0000567f @ imem:0000567f =====

void FUN_imem_0000567f(void)

{
  FUN_imem_00007589();
  return;
}



// ===== FUN_imem_00005ab0 @ imem:00005ab0 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00005ab0(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000635e @ imem:0000635e =====

/* WARNING: Instruction at (imem,0x0000635f) overlaps instruction at (imem,0x0000635e)
    */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000635e(void)

{
  undefined4 *puVar1;
  undefined4 *puVar2;
  char unaff_r1b;
  undefined4 unaff_r2;
  uint uVar3;
  char cVar4;
  undefined4 *puVar5;
  undefined4 in_r15;
  
  uVar3 = 0;
  puVar1 = (undefined4 *)0xffffff98;
  while( true ) {
    puVar1[2] = uVar3 >> 0xe;
    unaff_r1b = unaff_r1b + '\x01';
    puVar2 = puVar1 + 3;
    if (unaff_r1b == '\x06') break;
    while( true ) {
      puVar5 = (undefined4 *)0xf;
      cVar4 = func_0x00005e94(puVar2,0xe,0,0x52,0xf,unaff_r1b);
      if (cVar4 != '\0') break;
      *puVar2 = unaff_r2;
      puVar1[4] = unaff_r2;
      puVar1[5] = unaff_r2;
      unaff_r2 = 0xbadfb;
      *puVar5 = in_r15;
    }
    uVar3 = puVar1[5];
    puVar1 = puVar2;
  }
  _DAT_dmem_00000780 = 1;
  _DAT_dmem_140880b8 = _DAT_dmem_140880b8 & 0xffffff00;
  return;
}



// ===== FUN_imem_00006600 @ imem:00006600 =====

/* WARNING: Control flow encountered bad instruction data */
/* WARNING: Instruction at (imem,0x000065e5) overlaps instruction at (imem,0x000065e4)
    */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00006600(undefined4 param_1,byte *param_2)

{
  uint *puVar1;
  uint *unaff_r0;
  short sVar2;
  short sVar3;
  undefined2 uVar4;
  uint unaff_r2;
  int unaff_r3;
  byte *unaff_r4;
  int unaff_r5;
  uint unaff_r6;
  char cVar5;
  undefined2 unaff_r7h;
  int iVar6;
  byte bVar7;
  undefined4 uVar8;
  uint *puVar9;
  byte *pbVar10;
  byte *pbStackX_24;
  int iStackX_28;
  undefined4 uStackX_2c;
  int iStackX_30;
  int iStackX_34;
  undefined4 uStackX_38;
  uint *puStackX_3c;
  int iStackX_44;
  int iStackX_88;
  
  do {
    puVar9 = unaff_r0 + 0x18;
    pbVar10 = param_2 + -0xa4;
LAB_imem_000065e4:
    do {
      iVar6 = io_read(unaff_r3 + (int)puVar9 * 4);
      if ((char)puVar9 != '\0') {
        sVar2 = 0;
        uVar8 = 0x2cd;
        if (iVar6 != -1) {
LAB_imem_00006614:
          if (iStackX_88 != _DAT_dmem_00000714) {
            FUN_imem_000072af(uVar8);
          }
          return;
        }
        puVar9 = (uint *)(pbVar10 + -0x3c);
        unaff_r2 = unaff_r2 & 0xffff0000;
        unaff_r3 = 8;
        todo(unaff_r7h,0x73);
        _DAT_dmem_0000011a = 0xffffffb2;
        unaff_r0 = (uint *)0x4045bc09;
        pbVar10 = &stack0x00000048;
LAB_imem_000065c2:
        if (0xffff < (unaff_r2 & 0xffff) + iStackX_30) {
                    /* WARNING: Bad instruction - Truncating control flow here */
          halt_baddata();
        }
        uVar4 = (undefined2)(unaff_r2 >> 0x10);
        sVar3 = (short)unaff_r2 + uStackX_2c._2_2_;
        unaff_r2 = CONCAT22(uVar4,sVar3);
        sVar2 = sVar2 + -1;
        if (sVar2 == 0) {
          puVar9 = (uint *)CONCAT31(0xff,uDmemffffffff);
        }
        else {
          cVar5 = (char)unaff_r7h;
          if ((cVar5 == '\0') || ((uint *)((int)unaff_r0 + iStackX_44) <= puVar9)) {
            if (uStackX_38._3_1_ < 0x17) {
              if (iStackX_34 != 0) goto LAB_imem_000065a1;
              if (iStackX_28 == 0) {
                if ((unaff_r6 & 0x1000) == 0) goto LAB_imem_00006611;
                while (cVar5 != '\0') {
                  bVar7 = *unaff_r4;
                  puVar1 = unaff_r0 + 1;
                  unaff_r4 = unaff_r4 + 1;
                  *unaff_r0 = bVar7 & 0xf;
                  unaff_r0 = unaff_r0 + 2;
                  *puVar1 = (bVar7 & 0x70) >> 4;
                }
              }
              else if (cVar5 != '\0') {
                bVar7 = *unaff_r4;
                unaff_r4 = unaff_r4 + 1;
                todo();
                *unaff_r0 = CONCAT31((int3)((uint)iStackX_28 >> 8),bVar7);
                unaff_r0 = unaff_r0 + 1;
              }
              if (sVar3 == -1) {
                    /* WARNING: Bad instruction - Truncating control flow here */
                halt_baddata();
              }
              unaff_r2 = CONCAT22(uVar4,sVar3 + 1);
              puVar1 = unaff_r0;
              unaff_r0 = (uint *)0xffffffb2;
              param_2 = (byte *)0xffffd1b0;
              goto code_i0x000065b6;
            }
LAB_imem_00006611:
            uVar8 = 0x2d0;
            goto LAB_imem_00006614;
          }
        }
        goto LAB_imem_000065e4;
      }
    } while ((char)unaff_r3 != '\b');
    if (puStackX_3c != (uint *)0x0) {
      *puStackX_3c = unaff_r2;
    }
  } while( true );
LAB_imem_000065a1:
  if (cVar5 != '\0') {
    *unaff_r0 = 0;
    puVar1 = (uint *)((int)unaff_r0 + iStackX_44);
    param_2 = unaff_r4;
    pbStackX_24 = pbVar10;
code_i0x000065b6:
    FUN_imem_00007573(unaff_r0);
    unaff_r4 = unaff_r4 + unaff_r5;
    unaff_r0 = puVar1;
    pbVar10 = pbStackX_24;
  }
  goto LAB_imem_000065c2;
}



// ===== FUN_imem_00006751 @ imem:00006751 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00006751(void)

{
  char in_r9b;
  
  if (in_r9b == '\t') {
    _DAT_dmem_14001418 = _DAT_dmem_14001418 & 0xff00ffff | 0x340000;
    FUN_imem_00006600();
    _DAT_dmem_14001418 = _DAT_dmem_14001418 & 0xff00ffff | 0x350000;
  }
  return;
}



// ===== FUN_imem_000068fa @ imem:000068fa =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_000068fa(void)

{
  int *unaff_r0;
  int iStackX_1c;
  
  if (*unaff_r0 != 0) {
    FUN_imem_00006751();
  }
  if (iStackX_1c != _DAT_dmem_00000714) {
    FUN_imem_000072af(0);
  }
  return;
}



// ===== FUN_imem_0000692e @ imem:0000692e =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000692e(int param_1,undefined4 param_2,undefined4 param_3)

{
  int *in_r9;
  
  *in_r9 = param_1;
  _DAT_dmem_00000808 = param_3;
  _DAT_dmem_00000804 = param_2;
  _DAT_dmem_00000818 = param_3;
  _DAT_dmem_00000814 = param_2;
  if (param_1 == 1) {
    _DAT_dmem_0000080c = 4;
    _DAT_dmem_00000810 = 0;
    return;
  }
  if (param_1 == 2) {
    _DAT_dmem_0000080c = 2;
    _DAT_dmem_00000810 = 2;
  }
  return;
}



// ===== FUN_imem_00006a85 @ imem:00006a85 =====

/* WARNING: Control flow encountered bad instruction data */

undefined4 FUN_imem_00006a85(void)

{
  int unaff_r2;
  int in_r14;
  bool in_CF;
  bool in_ZF;
  
  if (!in_CF && !in_ZF) {
                    /* WARNING: Bad instruction - Truncating control flow here */
    halt_baddata();
  }
  *(uint *)(unaff_r2 + 1) =
       CONCAT31((int3)((uint)(in_r14 + 0x901) >> 8),*(undefined1 *)(in_r14 + 0x901));
  *(uint *)(unaff_r2 + 2) =
       CONCAT31((int3)((uint)(in_r14 + 0x902) >> 8),*(undefined1 *)(in_r14 + 0x902));
  *(uint *)(unaff_r2 + 3) =
       CONCAT31((int3)((uint)(in_r14 + 0x903) >> 8),*(undefined1 *)(in_r14 + 0x903));
  return 0;
}



// ===== FUN_imem_00006ae1 @ imem:00006ae1 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00006ae1(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00006ae9 @ imem:00006ae9 =====

void FUN_imem_00006ae9(void)

{
  undefined4 unaff_r0;
  uint unaff_r1;
  uint *unaff_r3;
  undefined4 *unaff_r4;
  int unaff_r5;
  int iVar1;
  
  iVar1 = FUN_imem_0000692e();
  if (iVar1 == 0) {
    *unaff_r4 = unaff_r0;
    *unaff_r3 = (uint)*(byte *)(unaff_r5 + (unaff_r1 & 3));
    FUN_imem_00006ae1();
    return;
  }
  return;
}



// ===== FUN_imem_00006fbb @ imem:00006fbb =====

/* WARNING: Control flow encountered bad instruction data */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00006fbb(int param_1)

{
  int *unaff_r0;
  ushort *unaff_r1;
  int *unaff_r7;
  int iStackX_28;
  
  if (param_1 == 0) {
    *unaff_r7 = (uint)*unaff_r1 + *unaff_r0;
                    /* WARNING: Bad instruction - Truncating control flow here */
    halt_baddata();
  }
  if (iStackX_28 != _DAT_dmem_00000714) {
    FUN_imem_000072af();
  }
  return;
}



// ===== FUN_imem_000070ae @ imem:000070ae =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_000070ae(void)

{
  byte unaff_r0b;
  uint unaff_r1;
  ushort *unaff_r2;
  uint *unaff_r3;
  uint *unaff_r4;
  int *unaff_r5;
  int *unaff_r6;
  uint unaff_r7;
  byte *unaff_r8;
  int iVar1;
  uint in_r13;
  uint in_r14;
  uint in_r15;
  int iStackX_34;
  
  do {
    while (*unaff_r2 == unaff_r7) {
      *unaff_r5 = in_r14 + 0x10;
      *unaff_r6 = in_r15 - 0x10;
    }
    *unaff_r3 = in_r13;
    unaff_r0b = unaff_r0b + 1;
    if (*unaff_r8 <= unaff_r0b) {
      FUN_imem_000070ae(0x2cb);
      return;
    }
    iVar1 = FUN_imem_00006ae9();
    if ((iVar1 != 0) || (iVar1 = FUN_imem_00006ae9(), iVar1 != 0)) goto LAB_imem_000070de;
    in_r14 = *unaff_r3;
  } while ((in_r14 <= unaff_r1) &&
          ((in_r15 = *unaff_r4, in_r15 <= unaff_r1 && (in_r13 = in_r15 + in_r14, in_r13 <= unaff_r1)
           )));
  iVar1 = 0x2c8;
LAB_imem_000070de:
  if (iStackX_34 != _DAT_dmem_00000714) {
    FUN_imem_000072af(iVar1);
  }
  return;
}



// ===== FUN_imem_000070c8 @ imem:000070c8 =====

/* WARNING: Instruction at (imem,0x000070ae) overlaps instruction at (imem,0x000070ad)
    */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_000070c8(void)

{
  byte unaff_r0b;
  uint unaff_r1;
  ushort *unaff_r2;
  uint *unaff_r3;
  uint *unaff_r4;
  int *unaff_r5;
  int *unaff_r6;
  uint unaff_r7;
  byte *unaff_r8;
  int iVar1;
  uint in_r13;
  uint uVar2;
  uint uVar3;
  int iStackX_34;
  
  while( true ) {
    *unaff_r3 = in_r13;
    unaff_r0b = unaff_r0b + 1;
    if (*unaff_r8 <= unaff_r0b) {
      FUN_imem_000070ae(0x2cb);
      return;
    }
    iVar1 = FUN_imem_00006ae9();
    if (iVar1 != 0) break;
    iVar1 = FUN_imem_00006ae9();
    if (iVar1 != 0) break;
    uVar2 = *unaff_r3;
    if (((unaff_r1 < uVar2) || (uVar3 = *unaff_r4, unaff_r1 < uVar3)) ||
       (in_r13 = uVar3 + uVar2, unaff_r1 < in_r13)) {
      iVar1 = 0x2c8;
      break;
    }
    while (*unaff_r2 == unaff_r7) {
      *unaff_r5 = uVar2 + 0x10;
      *unaff_r6 = uVar3 - 0x10;
    }
  }
  if (iStackX_34 != _DAT_dmem_00000714) {
    FUN_imem_000072af(iVar1);
  }
  return;
}



// ===== FUN_imem_0000710f @ imem:0000710f =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000710f(void)

{
  undefined4 in_r14;
  undefined4 in_r15;
  
  todo(in_r15,in_r14);
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000071d8 @ imem:000071d8 =====

void FUN_imem_000071d8(uint param_1)

{
  uint in_r9;
  uint *in_r14;
  
  *in_r14 = param_1 >> 0xc & 0xfff | in_r9 & 0xfffff000;
  return;
}



// ===== FUN_imem_0000722d @ imem:0000722d =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000722d(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000072ad @ imem:000072ad =====

/* WARNING: Removing unreachable block (imem,0x000072be) */
/* WARNING: Removing unreachable block (imem,0x000072ca) */
/* WARNING: Removing unreachable block (imem,0x000072d2) */
/* WARNING: Removing unreachable block (imem,0x000072da) */
/* WARNING: Removing unreachable block (imem,0x000072dd) */

undefined4 FUN_imem_000072ad(void)

{
  undefined4 uVar1;
  
  uVar1 = FUN_imem_000072ad(0x2c6);
  return uVar1;
}



// ===== FUN_imem_000072af @ imem:000072af =====

/* WARNING: Removing unreachable block (imem,0x000072be) */
/* WARNING: Removing unreachable block (imem,0x000072ca) */
/* WARNING: Removing unreachable block (imem,0x000072d2) */
/* WARNING: Removing unreachable block (imem,0x000072da) */
/* WARNING: Removing unreachable block (imem,0x000072dd) */

undefined4 FUN_imem_000072af(void)

{
  undefined4 uVar1;
  
  uVar1 = FUN_imem_000072ad(0x2c6);
  return uVar1;
}



// ===== FUN_imem_000072b1 @ imem:000072b1 =====

undefined4 FUN_imem_000072b1(undefined4 param_1)

{
  int unaff_r0;
  ushort in_r9h;
  undefined4 uVar1;
  
  if (in_r9h < 0x28) {
    uVar1 = FUN_imem_000072ad(0x2c6);
    return uVar1;
  }
  if (((0x100000 < *(uint *)(unaff_r0 + 8)) || (0x100000 < *(uint *)(unaff_r0 + 0xc))) ||
     (0x100000 < *(uint *)(unaff_r0 + 0xc) + *(uint *)(unaff_r0 + 8))) {
    param_1 = 0x2c7;
  }
  return param_1;
}



// ===== FUN_imem_000073ab @ imem:000073ab =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_000073ab(void)

{
  int unaff_r0;
  uint *in_r14;
  uint in_r15;
  
  if (in_r15 >> 0x10 == 0) {
    *in_r14 = unaff_r0 << 0x10 | in_r15 & 0xffff;
  }
  _DAT_dmem_14001418 = _DAT_dmem_14001418 & 0xffff0fff | 0x5000;
  FUN_imem_000073ab();
  return;
}



// ===== FUN_imem_0000740c @ imem:0000740c =====

void FUN_imem_0000740c(void)

{
  do {
    exit();
  } while( true );
}



// ===== FUN_imem_00007494 @ imem:00007494 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00007494(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000754b @ imem:0000754b =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000754b(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000756a @ imem:0000756a =====

void FUN_imem_0000756a(void)

{
  undefined4 *in_r14;
  
  *in_r14 = 0x59ffd94;
  func_0x000073a7();
  FUN_imem_000073ab();
  return;
}



// ===== FUN_imem_00007573 @ imem:00007573 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00007573(void)

{
  short unaff_r0h;
  
  if (unaff_r0h != 0x7e) {
    do {
    } while (unaff_r0h == 0xf8);
    FUN_imem_0000756a();
    return;
  }
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00007589 @ imem:00007589 =====

void FUN_imem_00007589(void)

{
  code *UNRECOVERED_JUMPTABLE;
  
                    /* WARNING: Could not recover jumptable at 0x0000758b. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (*UNRECOVERED_JUMPTABLE)();
  return;
}



// ===== FUN_imem_000075a1 @ imem:000075a1 =====

void FUN_imem_000075a1(void)

{
  return;
}



// ===== FUN_imem_00007654 @ imem:00007654 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00007654(void)

{
  undefined1 in_r15b;
  
  todo(in_r15b,0xfa);
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



