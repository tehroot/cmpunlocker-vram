// Decompilation: fwsec_170hx_imem.bin  (falcon:LE:32:v5)
// ===== FwSecEntry @ imem:00000000 =====

/* WARNING: Control flow encountered bad instruction data */

void FwSecEntry(void)

{
  undefined4 unaff_r5;
  
  uDmem00000000 = unaff_r5;
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000000a8 @ imem:000000a8 =====

void FUN_imem_000000a8(uint param_1)

{
  uint unaff_r1;
  
  func_0x00000360(0x1454,unaff_r1 & 0xffff | param_1);
  return;
}



// ===== FUN_imem_000002af @ imem:000002af =====

void FUN_imem_000002af(void)

{
  uint uVar1;
  
  do {
    uVar1 = io_read(0x1c000);
  } while ((uVar1 >> 0xc & 3) == 1);
  return;
}



// ===== FUN_imem_00000386 @ imem:00000386 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00000386(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000039c @ imem:0000039c =====

void FUN_imem_0000039c(int param_1,int param_2,uint param_3)

{
  uint in_r9;
  undefined4 in_r15;
  
  do {
    in_r15 = CONCAT31((int3)((uint)in_r15 >> 8),*(undefined1 *)(param_2 + in_r9));
    *(undefined4 *)(param_1 + in_r9) = in_r15;
    in_r9 = in_r9 + 1;
  } while (in_r9 < param_3);
  return;
}



// ===== FUN_imem_000003b0 @ imem:000003b0 =====

void FUN_imem_000003b0(undefined4 param_1,undefined4 param_2,int param_3)

{
  undefined4 *in_r9;
  
  do {
    *in_r9 = param_2;
    param_3 = param_3 + -1;
    in_r9 = (undefined4 *)((int)in_r9 + 1);
  } while (param_3 != 0);
  return;
}



// ===== FUN_imem_00000400 @ imem:00000400 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00000400(void)

{
  undefined4 unaff_r3;
  undefined4 *unaff_r4;
  undefined4 unaff_r8;
  
  todo(unaff_r4,unaff_r3);
  *unaff_r4 = unaff_r8;
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000006bf @ imem:000006bf =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000006bf(void)

{
  undefined1 in_r13b;
  
  todo(in_r13b,0x29);
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00001868 @ imem:00001868 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00001868(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00001e48 @ imem:00001e48 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00001e48(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000020fe @ imem:000020fe =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000020fe(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000021b3 @ imem:000021b3 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000021b3(void)

{
  undefined4 in_r13;
  int in_r15;
  
  *(undefined4 *)(in_r15 + 0x16a) = in_r13;
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00002ff0 @ imem:00002ff0 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00002ff0(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00003e25 @ imem:00003e25 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00003e25(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00004522 @ imem:00004522 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00004522(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00004b59 @ imem:00004b59 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00004b59(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00004fad @ imem:00004fad =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00004fad(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00005656 @ imem:00005656 =====

/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_imem_00005656(undefined4 param_1,undefined4 param_2)

{
                    /* WARNING: Could not recover jumptable at 0x00005660. Too many branches */
                    /* WARNING: Treating indirect jump as call */
  (*_DAT_dmem_00003371)(param_1,param_2,_DAT_dmem_00003371);
  return;
}



// ===== FUN_imem_00005c4b @ imem:00005c4b =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00005c4b(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00005e98 @ imem:00005e98 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00005e98(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00006c0e @ imem:00006c0e =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00006c0e(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000073c4 @ imem:000073c4 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000073c4(undefined4 param_1,undefined4 param_2,undefined1 param_3,undefined1 param_4)

{
  undefined4 in_r15;
  
  todo(param_4,0x28);
  todo(param_3,0xe5);
  todo(in_r15,7,0x17);
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000074c1 @ imem:000074c1 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000074c1(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00007ab7 @ imem:00007ab7 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00007ab7(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00007ee9 @ imem:00007ee9 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00007ee9(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00008149 @ imem:00008149 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00008149(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000834b @ imem:0000834b =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000834b(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00008b35 @ imem:00008b35 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00008b35(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00008b74 @ imem:00008b74 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00008b74(void)

{
  bool in_OF;
  
  if (!in_OF) {
                    /* WARNING: Bad instruction - Truncating control flow here */
    halt_baddata();
  }
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00008f76 @ imem:00008f76 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00008f76(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00009730 @ imem:00009730 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00009730(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000a53a @ imem:0000a53a =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000a53a(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000aab3 @ imem:0000aab3 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000aab3(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000ac54 @ imem:0000ac54 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000ac54(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000b555 @ imem:0000b555 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000b555(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



