// Decompilation: app08_170hx_imem.bin  (falcon:LE:32:v5)
// ===== FwSecEntry @ imem:00000000 =====

/* WARNING: This function may have set the stack pointer */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FwSecEntry(void)

{
  _DAT_dmem_0000fffc = 0x3c;
  FUN_imem_0000cf7c(0);
  do {
    exit();
  } while( true );
}



// ===== FUN_imem_0000009d @ imem:0000009d =====

undefined4 FUN_imem_0000009d(byte param_1)

{
  undefined4 uVar1;
  
  uVar1 = 0xffffffff;
  if (param_1 < 10) {
    uVar1 = CONCAT31(0xffffff,*(undefined1 *)(param_1 + 0x10));
  }
  return uVar1;
}



// ===== FUN_imem_000000bd @ imem:000000bd =====

void FUN_imem_000000bd(void)

{
  uint in_r9;
  
  func_0x0000006d(*(undefined1 *)((in_r9 & 0xff) + 0x1a));
  return;
}



// ===== FUN_imem_000002d1 @ imem:000002d1 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_000002d1(void)

{
  int iStackX_2c;
  
  func_0x000001a1();
  if (iStackX_2c != _DAT_dmem_00001834) {
    FUN_imem_000002d1(0);
    return;
  }
  return;
}



// ===== FUN_imem_000002d8 @ imem:000002d8 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_000002d8(void)

{
  int iStackX_2c;
  
  func_0x000001a1();
  if (iStackX_2c != _DAT_dmem_00001834) {
    FUN_imem_000002d1();
    return;
  }
  return;
}



// ===== FUN_imem_00000332 @ imem:00000332 =====

bool FUN_imem_00000332(void)

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
  uVar1 = FUN_imem_0000009d(unaff_r2b);
  func_0x000001a1(unaff_r2b);
  return (uVar1 & 0x60000000) == 0;
}



// ===== FUN_imem_000004fb @ imem:000004fb =====

void FUN_imem_000004fb(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int *unaff_r1;
  int in_r9;
  int in_r15;
  int iStackX_c;
  
  *(undefined4 *)(in_r9 + 9) = param_3;
  *(undefined4 *)(in_r9 + 0xb) = param_4;
  *(uint *)(in_r9 + 10) = CONCAT22((short)((uint)param_4 >> 0x10),(ushort)param_4 >> 8);
  iStackX_c = in_r15;
  FUN_imem_00000332(param_1,param_2,(undefined4 *)(in_r9 + 9),3,1);
  if (iStackX_c != *unaff_r1) {
    FUN_imem_0000cf7a();
  }
  return;
}



// ===== FUN_imem_00000533 @ imem:00000533 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00000533(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int in_r9;
  int iStackX_c;
  
  *(undefined4 *)(in_r9 + 10) = param_3;
  *(undefined4 *)(in_r9 + 0xb) = param_4;
  iStackX_c = _DAT_dmem_00001834;
  FUN_imem_00000332(param_1,param_2,(undefined4 *)(in_r9 + 10),2,1);
  if (iStackX_c != _DAT_dmem_00001834) {
    FUN_imem_0000cf7a();
  }
  return;
}



// ===== FUN_imem_0000057f @ imem:0000057f =====

/* WARNING: Control flow encountered bad instruction data */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000057f(uint param_1,uint param_2,undefined4 param_3,uint param_4,uint param_5)

{
  uint unaff_r0;
  uint unaff_r1;
  uint unaff_r2;
  uint unaff_r4;
  uint unaff_r5;
  uint unaff_r6;
  uint unaff_r7;
  int unaff_r8;
  undefined4 in_r9;
  undefined4 *in_r15;
  byte bStackX_28;
  byte bStackX_2c;
  byte bStackX_30;
  byte bStackX_34;
  byte bStackX_38;
  byte bStackX_3c;
  
  *in_r15 = in_r9;
  if ((char)unaff_r8 == 'T') {
                    /* WARNING: Bad instruction - Truncating control flow here */
    halt_baddata();
  }
  _DAT_dmem_1410100c =
       (((((uint)in_r15 | param_2) & 0xfff1ffff | param_1) & 0xff8fffff | unaff_r0) & 0xff7fffff |
       unaff_r1) & 0x7dffffff | (unaff_r2 & 1) << 0x19 | unaff_r8 << 0x1f;
  _DAT_dmem_14118fb8 =
       (param_5 & 1) << 0x10 | bStackX_3c & 0x1f | (unaff_r4 & 1) << 0x11 | (unaff_r5 & 1) << 0x12 |
       (unaff_r6 & 1) << 0x13 | (unaff_r7 & 3) << 0x14;
  _DAT_dmem_14118fbc =
       (param_4 & 1) << 0x10 | bStackX_38 & 0x1f | (bStackX_34 & 1) << 0x11 |
       (bStackX_30 & 1) << 0x12 | (bStackX_2c & 1) << 0x13 | (bStackX_28 & 3) << 0x14;
  return;
}



// ===== FUN_imem_000006ce @ imem:000006ce =====

/* WARNING: Control flow encountered bad instruction data */

undefined4 FUN_imem_000006ce(void)

{
  int unaff_r3;
  char *unaff_r7;
  undefined4 *puStackX_28;
  
  do {
    while (*unaff_r7 == '\0') {
      if (puStackX_28 == (undefined4 *)0x0) {
        return 1;
      }
      *puStackX_28 = 0;
    }
  } while (unaff_r3 == 0);
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00000708 @ imem:00000708 =====

uint FUN_imem_00000708(uint param_1)

{
  uint in_r15;
  
  return (((char)param_1 == '\0' ^ 1) << 0x17 | in_r15) >> (0x96 - (param_1 & 0xff) & 0x1f);
}



// ===== FUN_imem_00000881 @ imem:00000881 =====

int FUN_imem_00000881(void)

{
  uint unaff_r2;
  
  return (*(int *)(((unaff_r2 & 0xff) * 6 + 1) * 4 + 0x18b0) +
         *(int *)((unaff_r2 & 0xff) * 0x18 + 0x18b0)) / 2;
}



// ===== FUN_imem_000008bd @ imem:000008bd =====

int FUN_imem_000008bd(void)

{
  uint unaff_r0;
  
  return (*(int *)(((unaff_r0 & 0xff) * 6 + 1) * 4 + 0x18b0) +
         *(int *)((unaff_r0 & 0xff) * 0x18 + 0x18b0)) / 2;
}



// ===== FUN_imem_000009d2 @ imem:000009d2 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_000009d2(void)

{
  int in_r9;
  uint uVar1;
  uint uVar2;
  undefined4 *in_r15;
  
  *in_r15 = *(undefined4 *)(in_r9 + 0xc);
  uVar1 = FUN_imem_000006ce(_DAT_dmem_000019f8);
  uVar2 = FUN_imem_000006ce(_DAT_dmem_000019fc);
  if (uVar1 < _DAT_dmem_00001a08) {
    uVar1 = _DAT_dmem_00001a08;
  }
  if (uVar2 < uVar1) {
    uVar1 = uVar2;
  }
  _DAT_dmem_00001a08 = uVar1;
  return;
}



// ===== FUN_imem_00000bdc @ imem:00000bdc =====

void FUN_imem_00000bdc(undefined4 param_1,int param_2,undefined4 param_3)

{
  undefined4 uVar1;
  int unaff_r0;
  undefined4 unaff_r1;
  undefined4 *unaff_r8;
  uint uVar2;
  int in_r15;
  
  *unaff_r8 = param_3;
  todo(unaff_r1,0);
  uVar2 = in_r15 + param_2;
  func_0x00000587(0x1590,uVar2,param_3,0x34,0);
  uVar1 = uDmem00001a9c;
  if ((unaff_r0 != 0) && ((char)uDmem00001abc != -1)) {
    FUN_imem_00000bdc();
    return;
  }
  FUN_imem_00000fff(uDmem00001a9c,uVar2 & 0xffffff00,DAT_dmem_00001ac0);
  FUN_imem_00000e5f(uVar1);
  return;
}



// ===== FUN_imem_00000bf5 @ imem:00000bf5 =====

void FUN_imem_00000bf5(void)

{
  uint in_r9;
  char in_r15b;
  byte in_CF;
  
  if (in_r15b != (char)((0xffffff60 < in_r9 || CARRY4(in_r9 + 0x9f,(uint)in_CF)) + -1)) {
    FUN_imem_00000bdc(0);
    return;
  }
  FUN_imem_00000fff();
  FUN_imem_00000e5f();
  return;
}



// ===== FUN_imem_00000bfa @ imem:00000bfa =====

void FUN_imem_00000bfa(void)

{
  char in_r15b;
  char in_CF;
  
  if (in_r15b != (char)(in_CF + -1)) {
    FUN_imem_00000bdc(0);
    return;
  }
  FUN_imem_00000fff();
  FUN_imem_00000e5f();
  return;
}



// ===== FUN_imem_00000ca0 @ imem:00000ca0 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00000ca0(uint param_1)

{
  uint unaff_r1;
  char unaff_r2b;
  
  if (unaff_r2b != '\0') {
    _DAT_dmem_14118bc0 = param_1 & 0xfff;
    _DAT_dmem_14118bc4 = _DAT_dmem_14118bc0;
  }
  _DAT_dmem_14118f30 = unaff_r1 & 0xfff;
  _DAT_dmem_14118f34 = param_1 | 0x80000000;
  return;
}



// ===== FUN_imem_00000cc7 @ imem:00000cc7 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00000cc7(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00000e54 @ imem:00000e54 =====

/* WARNING: Instruction at (imem,0x00000e05) overlaps instruction at (imem,0x00000e03)
    */
/* WARNING: Control flow encountered bad instruction data */

ushort FUN_imem_00000e54(uint param_1)

{
  ushort unaff_r0h;
  int unaff_r1;
  ushort unaff_r2h;
  ushort unaff_r3h;
  int unaff_r5;
  undefined1 unaff_r6b;
  char unaff_r7b;
  byte unaff_r8b;
  ushort uVar1;
  int iVar2;
  undefined1 in_CF;
  undefined1 in_ZF;
  ushort uStackX_28;
  uint uStackX_2c;
  uint uStackX_30;
  uint uStackX_34;
  
code_i0x00000e54:
  if ((bool)in_CF || (bool)in_ZF) {
    unaff_r3h = unaff_r0h;
  }
LAB_imem_00000e5d:
  if ((uStackX_2c <= param_1) && (param_1 = FUN_imem_0000d32e(unaff_r3h,unaff_r6b), param_1 == 0)) {
    return unaff_r3h;
  }
  do {
    do {
      if (unaff_r1 < 0) {
        uVar1 = FUN_imem_00000e54(param_1);
        return uVar1;
      }
      unaff_r0h = unaff_r0h + 1;
      if (unaff_r0h == unaff_r2h) {
        return unaff_r3h;
      }
    } while (((byte)unaff_r0h & unaff_r8b) != unaff_r8b);
    while ((unaff_r0h & 0xff & uStackX_28) == 0) {
      if (unaff_r7b == '\0') {
        iVar2 = FUN_imem_0000d369((int)(short)unaff_r0h);
        param_1 = iVar2 + unaff_r5;
        if (param_1 < uStackX_30) goto LAB_imem_00000e5d;
        in_ZF = param_1 == uStackX_34;
        in_CF = param_1 < uStackX_34;
        goto code_i0x00000e54;
      }
      if (unaff_r1 < 1) {
        FUN_imem_0000d369((int)(short)unaff_r0h);
        FUN_imem_0000d2ed();
                    /* WARNING: Bad instruction - Truncating control flow here */
        halt_baddata();
      }
      FUN_imem_0000d369((int)(short)unaff_r0h);
      param_1 = 0xffffffc4;
    }
  } while( true );
}



// ===== FUN_imem_00000e5f @ imem:00000e5f =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00000e5f(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00000edb @ imem:00000edb =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00000edb(undefined4 param_1,int param_2)

{
  uint *unaff_r0;
  uint uVar1;
  uint *unaff_r1;
  int unaff_r2;
  undefined2 uVar2;
  uint uVar3;
  int in_r15;
  
  uVar2 = FUN_imem_00000cc7(param_1,(param_2 + unaff_r2) - in_r15);
  uVar1 = *unaff_r1;
  uVar3 = FUN_imem_0000d369(uVar2,*unaff_r0 & 0xfff);
  _DAT_dmem_14118bd8 = uVar3 / (uVar1 & 0xfff) & 0xfff;
  return;
}



// ===== FUN_imem_00000fff @ imem:00000fff =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00000fff(void)

{
  undefined4 unaff_r6;
  uint in_r9;
  uint in_r15;
  
  *(undefined4 *)(in_r9 | in_r15) = unaff_r6;
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000011ce @ imem:000011ce =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000011ce(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000014ec @ imem:000014ec =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000014ec(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00001506 @ imem:00001506 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00001506(byte *param_1,undefined4 param_2,int param_3,uint param_4)

{
  ushort *unaff_r0;
  int unaff_r1;
  uint unaff_r2;
  int unaff_r3;
  uint *unaff_r4;
  ushort unaff_r5h;
  uint *puVar1;
  ushort uVar2;
  
  while( true ) {
    param_3 = param_3 + 1;
    unaff_r0 = unaff_r0 + 1;
    if (param_3 == 4) break;
    uVar2 = *unaff_r0;
    if ((uVar2 != unaff_r5h) && ((unaff_r3 << (*param_1 & 0x1f) & *unaff_r4) == 0)) {
      puVar1 = (uint *)((((uint)*param_1 * 0x400 + unaff_r1) * 4 + 0x24046d + param_3) * 4 |
                       unaff_r2);
      *puVar1 = (uint)uVar2;
      *puVar1 = uVar2 | param_4;
    }
  }
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00001524 @ imem:00001524 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00001524(void)

{
  int in_r15;
  
  if (in_r15 != _DAT_dmem_00001834) {
    FUN_imem_0000cf7a();
  }
  return;
}



// ===== FUN_imem_00002fc5 @ imem:00002fc5 =====

void FUN_imem_00002fc5(uint param_1,undefined4 param_2,undefined4 param_3,uint param_4)

{
  uint *unaff_r0;
  uint in_r9;
  
  *unaff_r0 = (in_r9 | param_4) & 0xfffb7fff | param_1;
  return;
}



// ===== FUN_imem_00002fd9 @ imem:00002fd9 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00002fd9(void)

{
  uint *in_r15;
  
  *in_r15 = *in_r15 & 0xf9ffffff;
  _DAT_dmem_140204e8 = _DAT_dmem_140204e8 & 0xf9ffffff;
  return;
}



// ===== FUN_imem_0000494e @ imem:0000494e =====

void FUN_imem_0000494e(void)

{
  int unaff_r0;
  int *unaff_r1;
  int *unaff_r2;
  undefined4 in_r9;
  
  *(undefined4 *)(unaff_r0 + 0x82) = in_r9;
  *(undefined4 *)(unaff_r0 + 0xbb) = in_r9;
  *(undefined4 *)(unaff_r0 + 0xbc) = in_r9;
  *(undefined4 *)(unaff_r0 + 0xbd) = in_r9;
  *(undefined4 *)(unaff_r0 + 0xbe) = in_r9;
  *(undefined4 *)(unaff_r0 + 0xbf) = in_r9;
  *(undefined4 *)(unaff_r0 + 0xc0) = in_r9;
  *(undefined4 *)(unaff_r0 + 0xc1) = in_r9;
  *(undefined4 *)(unaff_r0 + 0xc2) = in_r9;
  FUN_imem_00008fdd();
  if (*unaff_r1 != *unaff_r2) {
    FUN_imem_0000cf7a();
  }
  return;
}



// ===== FUN_imem_00004d52 @ imem:00004d52 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00004d52(void)

{
  undefined4 *in_r13;
  
  *in_r13 = *in_r13;
  _DAT_dmem_1417e21c = _DAT_dmem_1417e21c & 0xfffffffe;
  _DAT_dmem_1482380c = 0x888888;
  _DAT_dmem_14823810 = 0x2aaaaa;
  _DAT_dmem_1482382c = 10;
  return;
}



// ===== FUN_imem_00004da8 @ imem:00004da8 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00004da8(void)

{
  uint in_r9;
  uint *in_r14;
  uint *in_r15;
  
  *in_r14 = in_r9 | 0x200;
  *in_r15 = *in_r15 | 3;
  uDmem1417e318 = uDmem1417e318 | 1;
  in_r14[-0x3e0] = in_r14[-0x3e0] | 1;
  return;
}



// ===== FUN_imem_00004db0 @ imem:00004db0 =====

void FUN_imem_00004db0(undefined4 param_1,undefined4 param_2,undefined4 *param_3,uint *param_4,
                      undefined4 *param_5)

{
  undefined4 in_r9;
  uint *in_r15;
  
  *param_5 = in_r9;
  *in_r15 = *in_r15 | 3;
  *param_4 = *param_4 | 1;
  *param_3 = *param_3;
  param_5[-0x3e0] = param_5[-0x3e0] | 1;
  return;
}



// ===== FUN_imem_00004de3 @ imem:00004de3 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00004de3(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00007eea @ imem:00007eea =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00007eea(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00008034 @ imem:00008034 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00008034(void)

{
  undefined1 unaff_r1b;
  
  todo(unaff_r1b,0xbf);
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000080cd @ imem:000080cd =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000080cd(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00008107 @ imem:00008107 =====

void FUN_imem_00008107(void)

{
  uint in_r9;
  uint uVar1;
  uint in_r14;
  uint *in_r15;
  
  uVar1 = ~in_r9 & 0x1f;
  *in_r15 = ~((0xffffffffU >> ((in_r9 & 0x1f) + uVar1 & 0x1f)) << uVar1) & in_r14 | 1 << uVar1;
  FUN_imem_00008e0a(0x2033c,1,0);
  return;
}



// ===== FUN_imem_0000819e @ imem:0000819e =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000819e(void)

{
  uint *unaff_r0;
  uint unaff_r1;
  undefined4 in_r9;
  int in_r13;
  
  _DAT_dmem_00000014 = in_r9;
  *unaff_r0 = *(byte *)(in_r13 + 0x39) | 0xa00 | *unaff_r0 & unaff_r1;
  FUN_imem_000080cd(2);
  return;
}



// ===== FUN_imem_00008257 @ imem:00008257 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00008257(void)

{
  uint unaff_r2;
  byte *in_r15;
  
  _DAT_dmem_14020418 = (in_r15[1] & 0xf) << 8 | (uint)*in_r15 | _DAT_dmem_14020418 & unaff_r2;
  FUN_imem_000080cd(1);
  return;
}



// ===== FUN_imem_00008359 @ imem:00008359 =====

void FUN_imem_00008359(uint param_1,uint param_2,undefined4 param_3,int param_4,uint param_5)

{
  uint *unaff_r1;
  uint in_r9;
  uint in_r15;
  
  *unaff_r1 = ((in_r9 | ((param_5 | param_2) & 0xfff) << 0xc) & in_r15 | param_4 << 0x18) &
              0xfffffff | param_1;
  FUN_imem_000080cd(param_1 & 0xffffff00);
  return;
}



// ===== FUN_imem_0000839c @ imem:0000839c =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000839c(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00008674 @ imem:00008674 =====

void FUN_imem_00008674(undefined4 param_1,int param_2,uint param_3,undefined4 param_4,uint param_5)

{
  uint *unaff_r0;
  uint in_r9;
  
  *unaff_r0 = (in_r9 & 0x1f) << 0x18 | param_3 & 0x1ffe00ff | param_2 << 0x1d |
              (param_5 >> 5 & 0x1ff) << 8;
  return;
}



// ===== FUN_imem_000087e9 @ imem:000087e9 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_000087e9(undefined4 param_1,undefined4 param_2,int param_3)

{
  _DAT_dmem_140207a4 =
       *(byte *)(param_3 + 0xd4) & 0x3f | (*(byte *)(param_3 + 0xd5) & 1) << 0x1c |
       (*(byte *)(param_3 + 0xd6) & 1) << 0x1d | (uint)*(byte *)(param_3 + 0xd7) << 0x1f;
  return;
}



// ===== FUN_imem_00008d9e @ imem:00008d9e =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00008d9e(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00008e0a @ imem:00008e0a =====

void FUN_imem_00008e0a(undefined4 param_1,uint param_2,undefined4 param_3,uint param_4,uint *param_5
                      )

{
  uint unaff_r0;
  uint in_r9;
  uint in_r15;
  
  *param_5 = (in_r9 & 0xf | in_r15 & unaff_r0) & param_2 | param_4;
  return;
}



// ===== FUN_imem_00008e4f @ imem:00008e4f =====

/* WARNING: Instruction at (imem,0x00008e52) overlaps instruction at (imem,0x00008e51)
    */

void FUN_imem_00008e4f(undefined4 param_1)

{
  do {
    param_1 = FUN_imem_0000d369(param_1,100000);
  } while( true );
}



// ===== FUN_imem_00008e96 @ imem:00008e96 =====

bool FUN_imem_00008e96(undefined4 param_1,undefined1 param_2,undefined2 param_3)

{
  char cVar1;
  
  FUN_imem_0000d369(param_3,param_2);
  cVar1 = func_0x00000587();
  return cVar1 != '\0';
}



// ===== FUN_imem_00008ee8 @ imem:00008ee8 =====

void FUN_imem_00008ee8(void)

{
  FUN_imem_00008e4f();
  return;
}



// ===== FUN_imem_00008f3a @ imem:00008f3a =====

void FUN_imem_00008f3a(void)

{
  FUN_imem_00008e4f();
  return;
}



// ===== FUN_imem_00008f68 @ imem:00008f68 =====

void FUN_imem_00008f68(void)

{
  return;
}



// ===== FUN_imem_00008f8c @ imem:00008f8c =====

void FUN_imem_00008f8c(void)

{
  FUN_imem_00008e4f();
  return;
}



// ===== FUN_imem_00008fb9 @ imem:00008fb9 =====

void FUN_imem_00008fb9(undefined1 param_1,int param_2,undefined4 param_3)

{
  int in_r9;
  
  FUN_imem_00008e4f((short)*(undefined4 *)(param_2 + 4) + (short)*(undefined4 *)(in_r9 + 0x1c),
                    param_1,*(uint *)(param_2 + 0xc) & 0xffff,0x17cc,param_3);
  return;
}



// ===== FUN_imem_00008fdd @ imem:00008fdd =====

void FUN_imem_00008fdd(void)

{
  FUN_imem_00008e4f();
  return;
}



// ===== FUN_imem_000093a9 @ imem:000093a9 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000093a9(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00009649 @ imem:00009649 =====

void FUN_imem_00009649(void)

{
                    /* WARNING: Subroutine does not return */
  FUN_imem_000004fb();
}



// ===== FUN_imem_000096c4 @ imem:000096c4 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000096c4(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000a8f3 @ imem:0000a8f3 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000a8f3(void)

{
  int iStackX_18;
  
  FUN_imem_00009649();
  func_0x00000158();
  func_0x00000577();
  if (iStackX_18 != _DAT_dmem_00001834) {
    FUN_imem_0000cf7a();
  }
  return;
}



// ===== FUN_imem_0000a96e @ imem:0000a96e =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000a96e(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000b89c @ imem:0000b89c =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000b89c(void)

{
  int iStackX_18;
  
  FUN_imem_0000a8f3();
  func_0x00000158();
  func_0x00000577();
  if (iStackX_18 != _DAT_dmem_00001834) {
    FUN_imem_0000cf7a();
  }
  return;
}



// ===== FUN_imem_0000b90d @ imem:0000b90d =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000b90d(void)

{
  uint in_r9;
  int iStackX_10;
  
  if ((in_r9 & 0x10) == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_imem_000004fb();
  }
  if (iStackX_10 != _DAT_dmem_00001834) {
    FUN_imem_0000cf7a();
  }
  return;
}



// ===== FUN_imem_0000be14 @ imem:0000be14 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000be14(void)

{
  int iStackX_1c;
  
  FUN_imem_0000b89c();
  func_0x00000158();
  func_0x00000577();
  if (iStackX_1c != _DAT_dmem_00001834) {
    FUN_imem_0000cf7a();
  }
  return;
}



// ===== FUN_imem_0000be85 @ imem:0000be85 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000be85(void)

{
  uint in_r9;
  int iStackX_10;
  
  if ((in_r9 & 0x10) == 0) {
                    /* WARNING: Subroutine does not return */
    FUN_imem_000004fb();
  }
  if (iStackX_10 != _DAT_dmem_00001834) {
    FUN_imem_0000cf7a();
  }
  return;
}



// ===== FUN_imem_0000c38b @ imem:0000c38b =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000c38b(void)

{
  int iStackX_1c;
  
  FUN_imem_0000be14();
  func_0x00000158();
  func_0x00000577();
  if (iStackX_1c != _DAT_dmem_00001834) {
    FUN_imem_0000cf7a();
  }
  return;
}



// ===== FUN_imem_0000c4ca @ imem:0000c4ca =====

void FUN_imem_0000c4ca(void)

{
  func_0x000004bd();
  func_0x00000158();
  func_0x00000577();
  return;
}



// ===== FUN_imem_0000c5c2 @ imem:0000c5c2 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000c5c2(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000ca7e @ imem:0000ca7e =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000ca7e(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000ca9b @ imem:0000ca9b =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000ca9b(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000cae2 @ imem:0000cae2 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000cae2(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000cb00 @ imem:0000cb00 =====

void FUN_imem_0000cb00(undefined4 param_1,undefined4 param_2)

{
  FUN_imem_0000ca9b(param_1,param_2,0x470);
  return;
}



// ===== FUN_imem_0000cf7a @ imem:0000cf7a =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000cf7a(void)

{
  uint unaff_r1;
  undefined4 *in_r9;
  
  *in_r9 = 300;
  uDmem141c509c = uDmem141c509c | unaff_r1;
  _DAT_dmem_141c5068 = 500000000;
  _DAT_dmem_14118f78 = _DAT_dmem_14118f78 & 0xbfffffff;
  return;
}



// ===== FUN_imem_0000cf7c @ imem:0000cf7c =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000cf7c(void)

{
  uint unaff_r1;
  undefined4 *in_r9;
  uint *in_r14;
  
  *in_r9 = 300;
  *in_r14 = *in_r14 | unaff_r1;
  _DAT_dmem_141c5068 = 500000000;
  _DAT_dmem_14118f78 = _DAT_dmem_14118f78 & 0xbfffffff;
  return;
}



// ===== FUN_imem_0000d064 @ imem:0000d064 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_0000d064(void)

{
  uint in_r9;
  undefined4 *in_r13;
  uint *in_r14;
  
  _DAT_dmem_ffffffb3 = 0xffffffb3;
  *in_r13 = in_r14;
  *in_r14 = in_r9 & 0xffff | 0xffb30000;
  *in_r14 = *in_r14 & 0xfffffff0 | 7;
  FUN_imem_0000d068(0xffffffb3);
  return;
}



// ===== FUN_imem_0000d068 @ imem:0000d068 =====

void FUN_imem_0000d068(void)

{
  int unaff_r0;
  uint in_r9;
  undefined4 *in_r13;
  uint *in_r14;
  
  *in_r13 = in_r14;
  *in_r14 = unaff_r0 << 0x10 | in_r9 & 0xffff;
  *in_r14 = *in_r14 & 0xfffffff0 | 7;
  FUN_imem_0000d068(0xffffffb3);
  return;
}



// ===== FUN_imem_0000d0c9 @ imem:0000d0c9 =====

void FUN_imem_0000d0c9(void)

{
  do {
    exit();
  } while( true );
}



// ===== FUN_imem_0000d151 @ imem:0000d151 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000d151(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000d208 @ imem:0000d208 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000d208(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000d21e @ imem:0000d21e =====

void FUN_imem_0000d21e(void)

{
  uint in_r9;
  uint in_r13;
  uint *in_r14;
  int in_r15;
  
  io_read(in_r15 * 5);
  *in_r14 = in_r13 & in_r9 | 0x80000000;
  FUN_imem_0000d064();
  FUN_imem_0000d068();
  return;
}



// ===== FUN_imem_0000d236 @ imem:0000d236 =====

void FUN_imem_0000d236(void)

{
  return;
}



// ===== FUN_imem_0000d2ed @ imem:0000d2ed =====

undefined4
FUN_imem_0000d2ed(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  undefined4 uStackX_2c;
  undefined4 uStackX_30;
  
  uStackX_2c = param_3;
  uStackX_30 = param_4;
  FUN_imem_0000d369();
  FUN_imem_0000d369();
  return uStackX_2c;
}



// ===== FUN_imem_0000d32e @ imem:0000d32e =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000d32e(void)

{
  undefined1 in_r15b;
  
  todo(in_r15b,0xfa);
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000d369 @ imem:0000d369 =====

/* WARNING: Control flow encountered bad instruction data */

uint FUN_imem_0000d369(uint param_1,uint param_2)

{
  int in_r9;
  char in_OF;
  char in_SF;
  
  if (in_OF == in_SF) {
    param_1 = param_1 % param_2;
    if (in_r9 < 0) {
      return -param_1;
    }
  }
  else {
    param_1 = param_1 % -param_2;
    if (in_r9 < 0) {
                    /* WARNING: Bad instruction - Truncating control flow here */
      halt_baddata();
    }
  }
  return param_1;
}



