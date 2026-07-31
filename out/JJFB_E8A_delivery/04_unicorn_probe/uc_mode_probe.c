#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unicorn/unicorn.h>
static void on_code(uc_engine *uc, uint64_t address, uint32_t size, void *user) {
  uint32_t cpsr=0; uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
  printf("pc=0x%llX size=%u T=%d\n", (unsigned long long)address, size, (cpsr>>5)&1);
  if (address >= 0x306320) uc_emu_stop(uc);
}
int main(void) {
  uc_engine *uc; uc_hook hh; uint32_t cpsr, sp=0x27FE00, lr=0x100000, r0=0, r9=0x2B1858;
  uint8_t code[0x200]; FILE *f=fopen("jjfb_extracted/robotol.ext","rb");
  uint32_t base=0x2D8DF4, map=0x306000; fseek(f, 0x306000-base, SEEK_SET); fread(code,1,0x200,f); fclose(f);
  uc_open(UC_ARCH_ARM, UC_MODE_ARM, &uc);
  uc_mem_map(uc, map, 0x2000, UC_PROT_ALL); uc_mem_write(uc, map, code, 0x200);
  uc_mem_map(uc, 0x27F000, 0x2000, UC_PROT_ALL); uc_mem_map(uc, 0x2B1000, 0x2000, UC_PROT_ALL);
  cpsr = 1u<<5; uc_reg_write(uc, UC_ARM_REG_CPSR, &cpsr);
  uc_reg_write(uc, UC_ARM_REG_SP, &sp); uc_reg_write(uc, UC_ARM_REG_LR, &lr);
  uc_reg_write(uc, UC_ARM_REG_R0, &r0); uc_reg_write(uc, UC_ARM_REG_R9, &r9);
  uc_hook_add(uc, &hh, UC_HOOK_CODE, (void*)on_code, NULL, map, map+0x1fff);
  printf("=== A: emu_start even PC + CPSR.T=1 (current run_entry_ex) ===\n");
  { uc_err e=uc_emu_start(uc, 0x30630C, 0x306350, 0, 8); uint32_t pc=0,c=0; uc_reg_read(uc,UC_ARM_REG_PC,&pc); uc_reg_read(uc,UC_ARM_REG_CPSR,&c); printf("err=%u pc=0x%X T=%d\n",(unsigned)e,pc,(c>>5)&1); }
  /* reset T and try odd */
  cpsr = 1u<<5; uc_reg_write(uc, UC_ARM_REG_CPSR, &cpsr);
  printf("=== B: emu_start odd PC 0x30630D + CPSR.T=1 ===\n");
  { uc_err e=uc_emu_start(uc, 0x30630D, 0x306350, 0, 8); uint32_t pc=0,c=0; uc_reg_read(uc,UC_ARM_REG_PC,&pc); uc_reg_read(uc,UC_ARM_REG_CPSR,&c); printf("err=%u pc=0x%X T=%d\n",(unsigned)e,pc,(c>>5)&1); }
  /* C: odd PC without preset T */
  cpsr = 0; uc_reg_write(uc, UC_ARM_REG_CPSR, &cpsr);
  printf("=== C: emu_start odd PC 0x30630D + CPSR.T=0 ===\n");
  { uc_err e=uc_emu_start(uc, 0x30630D, 0x306350, 0, 8); uint32_t pc=0,c=0; uc_reg_read(uc,UC_ARM_REG_PC,&pc); uc_reg_read(uc,UC_ARM_REG_CPSR,&c); printf("err=%u pc=0x%X T=%d\n",(unsigned)e,pc,(c>>5)&1); }
  uc_close(uc); return 0;
}
