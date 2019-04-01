#include <iostream>
#include <fstream>
#include <cstdlib>
#include <memory>

#include "panda/plugin.h"

static target_ulong range_start;
static target_ulong range_end;

static std::ofstream ofs;

static std::unique_ptr<std::ofstream> output;

enum event_type : int {
  WRITE,
  FLUSH,
  FENCE,
  CHECKPOINT,
};

static void log_output(target_ulong pc, event_type type, target_ulong offset, target_ulong write_size, void* write_data) {
  output->write(reinterpret_cast<char*>(&pc), sizeof(pc));
  output->write(reinterpret_cast<char*>(&type), sizeof(type));
  switch (type) {
  case WRITE: {
    output->write(reinterpret_cast<char*>(&offset), sizeof(offset));
    output->write(reinterpret_cast<char*>(&write_size), sizeof(write_size));
    output->write(reinterpret_cast<char*>(write_data), write_size);
    ofs << "\nWrite ins " << offset << "\n";
    break;
  }
  case FLUSH: {
    output->write(reinterpret_cast<char*>(&offset), sizeof(offset));
    break;
  }
  default:
  case FENCE:
    ofs << "\nFence ins";
    break;
  case CHECKPOINT: ofs << "\nCheckpoint"; break;
  }
}

extern "C" int mem_write_callback(CPUState *env, target_ulong pc, target_ulong pa,
                       target_ulong size, void *buf) {
    if (pa >= range_start && pa < range_end) {
        log_output(pc, WRITE, pa - range_start, size, buf);
    }
    return 0;
}

static int reg_from_rm(uint8_t rm) {
  switch(rm) {
    case 0: return R_EAX;
    case 1: return R_ECX;
    case 2: return R_EDX;
    case 3: return R_EBX;
    case 6: return R_ESI;
    case 7: return R_EDI;
    default: return -1; 
    // NOTE: Doesn't account for exotic prefixes (like using %ds or extended registers like %r8)
    //       Nor does this account for displacement or SIB suffixes

  }
}

static bool try_capture(CPUState *env, target_ulong pc, bool is_translate) {
  auto x86_env = (CPUX86State *)((CPUState*) env->env_ptr);
  uint8_t insn[4];
  int err = panda_virtual_memory_read(env, pc, insn, 4);
  if (err < 0) {
    std::cout << "Error reading insns!" << std::endl;
    return false;
  }

 if (insn[0] >= 0x88 && insn[0] <= 0x8F){
	ofs << "\n" << std::hex << static_cast<int>(insn[1]) << " : Store; ";
	if (insn[1]==0x6 || insn[1]==0x7){
		ofs << std::hex << static_cast<int>(insn[0]) << " : is the start ins";;
		uint8_t rm = insn[1] & 7;
		auto target_reg = reg_from_rm(rm);
         	target_ulong va = x86_env->regs[target_reg];
         	target_ulong pa = panda_virt_to_phys(env, va);
    		//if (pa >= range_start && pa < range_end) {
			ofs << "\n\tPhy addr = " << pa;
		//}
	}
 }

 if (insn[0] >= 0xA0 && insn[0] <= 0xA5){
	ofs << "\n" << std::hex << static_cast<int>(insn[1]) << " : Store-A\n";
 }
 if ( insn[0] == 0x0F)	
  {
	ofs << std::hex << static_cast<int>(insn[1]) <<" : Could be MovNTI";
	uint8_t rm = insn[2] & 7;
	auto target_reg = reg_from_rm(rm);
         target_ulong va = x86_env->regs[target_reg];
         target_ulong pa = panda_virt_to_phys(env, va);
	ofs << "\t Phy addr = " << pa;	
  }
  
  // clflush and fences
  if (insn[0] == 0x0F && insn[1] == 0xAE)
  {
    if (insn[2] >= 0xE8 && insn[2] <= 0xEF) // exclude lfence
      return false;
    if (insn[2] >= 0xF0 /* && insn[2] <= 0xFF*/) { // mfence 0xF0-0xF7, sfence 0xF8-0xFF
      if (!is_translate)
        log_output(pc, FENCE, 0, 0, nullptr);
      return true;
    } 
    uint8_t reg = (insn[2] >> 3) & 7;
    uint8_t rm = insn[2] & 7;
    auto target_reg = reg_from_rm(rm);
    if (reg == 7 && target_reg > -1) // clflush: 0f ae modrm.reg == 7
    {
       if (!is_translate) {
         target_ulong va = x86_env->regs[target_reg];
         target_ulong pa = panda_virt_to_phys(env, va);
         if (pa == -1) {
           std::cout << "[pc 0x" << pc << "] clflush at unmapped address? va: " << va << std::endl;
           return false;
         } else {
           if (pa >= range_start && pa < range_end)
             log_output(pc, FLUSH, pa - range_start, 0, nullptr);
         }
       }
       return true;
    }
  }
  
  // clwb and clflushopt
  if (insn[0] == 0x66 && insn[1] == 0x0F && insn[2] == 0xAE)
  {
    uint8_t reg = (insn[3] >> 3) & 7;
    uint8_t rm = insn[3] & 7;
    auto target_reg = reg_from_rm(rm);
    if ((reg == 6 || reg == 7) && target_reg > -1) // clwb and clflushopt: 66 0f ae modrm.reg == 6 or 7
    {
       if (!is_translate) {
         target_ulong va = x86_env->regs[target_reg];
         target_ulong pa = panda_virt_to_phys(env, va);
         if (pa == -1) {
           std::cout << "[pc 0x" << pc << "] clwb/clflushopt at unmapped address? va: " << va << std::endl;
           return false;
         } else {
           if (pa >= range_start && pa < range_end)
             log_output(pc, FLUSH, pa - range_start, 0, nullptr);
         }
       }
       return true;
    }
  }

  // f2xm1 
  if (insn[0] == 0xd9 && insn[1] == 0xf0) {
    if (!is_translate)
      log_output(pc, CHECKPOINT, 0, 0, nullptr);
    return true;
  }

  return false;
}

extern "C" bool translate_callback(CPUState *env, target_ulong pc) {
  return try_capture(env, pc, false);
}

extern "C" int exec_callback(CPUState *env, target_ulong pc) {
  if (!try_capture(env, pc, true)) {
    std::cout << "WARNING: exec callback running for something we're not interested in!" << std::endl;
  }
  return 0;
}

extern "C" bool init_plugin(void *self) {
    panda_arg_list *args = panda_get_args("writetracker");
    range_start = panda_parse_ulong_opt(args, "start", 0x40000000, "Start address tracking range, default 1G");
    range_end = panda_parse_ulong_opt(args, "end", 0x48000000, "End address (exclusive) of tracking range, default 1G+128MB"); 
    std::cout << "writetracker loading" << std::endl;
    std::cout << "tracking range [" << std::hex << range_start << ", " << std::hex << range_end << ")" << std::endl;

    panda_do_flush_tb();
    // Need this to get EIP with our callbacks
    panda_enable_precise_pc();
    // Enable memory logging
    panda_enable_memcb();
    //panda_do_flush_tb();

    panda_cb pcb {};
    pcb.insn_translate = translate_callback;
    panda_register_callback(self, PANDA_CB_INSN_TRANSLATE, pcb);

    pcb = {};
    pcb.insn_exec = exec_callback;
    panda_register_callback(self, PANDA_CB_INSN_EXEC, pcb);

    pcb = {};
    pcb.phys_mem_after_write = mem_write_callback;
    panda_register_callback(self, PANDA_CB_PHYS_MEM_AFTER_WRITE, pcb);

    output = std::unique_ptr<std::ofstream>(new std::ofstream("wt.out", std::ios::binary));

    ofs.open ("out.log", std::ofstream::out);
    return true;
}

extern "C" void uninit_plugin(void *self) {
    output.reset();
    ofs.close();
    panda_disable_memcb();
    panda_disable_precise_pc();
    panda_do_flush_tb();
    (void) self;
}
