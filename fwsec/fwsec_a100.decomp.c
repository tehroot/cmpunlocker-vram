// Decompilation: fwsec_a100_imem.bin  (falcon:LE:32:v5)
// ===== FwSecEntry @ imem:00000000 =====

/* WARNING: This function may have set the stack pointer */

void FwSecEntry(void)

{
  uDmemfffffffc = 0x1c;
  FUN_imem_000000a8();
  do {
    exit();
  } while( true );
}



// ===== FUN_imem_0000007e @ imem:0000007e =====

void FUN_imem_0000007e(undefined4 param_1,uint param_2)

{
  uint unaff_r0;
  uint unaff_r1;
  uint uVar1;
  
  func_0x0000035b(param_1,unaff_r0 | param_2);
  uVar1 = func_0x00000336(0x1438);
  func_0x0000035b(0x1438,unaff_r0 | uVar1 & 0xffff);
  uVar1 = func_0x00000336(0x1454);
  func_0x0000035b(0x1454,unaff_r1 & 0xffff | uVar1 & 0xffff0000);
  return;
}



// ===== FUN_imem_000000a8 @ imem:000000a8 =====

void FUN_imem_000000a8(uint param_1)

{
  uint unaff_r1;
  
  func_0x0000035b(0x1454,unaff_r1 & 0xffff | param_1);
  return;
}



// ===== FUN_imem_000002aa @ imem:000002aa =====

void FUN_imem_000002aa(void)

{
  uint uVar1;
  
  do {
    uVar1 = io_read(0x1c000);
  } while ((uVar1 >> 0xc & 3) == 1);
  return;
}



// ===== FUN_imem_00000381 @ imem:00000381 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00000381(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00000397 @ imem:00000397 =====

void FUN_imem_00000397(int param_1,int param_2,uint param_3)

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



// ===== FUN_imem_000003ab @ imem:000003ab =====

void FUN_imem_000003ab(undefined4 param_1,undefined4 param_2,int param_3)

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



// ===== FUN_imem_000005c5 @ imem:000005c5 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000005c5(void)

{
  undefined4 unaff_r7;
  undefined4 *in_r9;
  
  *in_r9 = unaff_r7;
  func_0x007ae172();
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000015b0 @ imem:000015b0 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000015b0(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000254c @ imem:0000254c =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000254c(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000029b1 @ imem:000029b1 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000029b1(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000037f8 @ imem:000037f8 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000037f8(void)

{
  int in_r9;
  undefined4 in_r13;
  int in_r15;
  
  io_write_sync(in_r9 + 0x2ac,in_r13);
  io_write_sync(in_r15 + 0x298,in_r9);
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00003ff1 @ imem:00003ff1 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00003ff1(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00004449 @ imem:00004449 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00004449(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000045c7 @ imem:000045c7 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000045c7(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00004dba @ imem:00004dba =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00004dba(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00005647 @ imem:00005647 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00005647(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000057d0 @ imem:000057d0 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000057d0(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000062f4 @ imem:000062f4 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000062f4(void)

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



// ===== FUN_imem_00007c34 @ imem:00007c34 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00007c34(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00007c7f @ imem:00007c7f =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00007c7f(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00008aee @ imem:00008aee =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00008aee(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_00008c1e @ imem:00008c1e =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_00008c1e(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000092ad @ imem:000092ad =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000092ad(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_000095a7 @ imem:000095a7 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_000095a7(void)

{
  undefined1 unaff_r0b;
  
  todo(unaff_r0b,0xb2);
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



// ===== FUN_imem_0000a963 @ imem:0000a963 =====

/* WARNING: Control flow encountered bad instruction data */

void FUN_imem_0000a963(void)

{
                    /* WARNING: Bad instruction - Truncating control flow here */
  halt_baddata();
}



