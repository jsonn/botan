/**
* Runtime CPU detection
* (C) 2009 Jack Lloyd
*
* Distributed under the terms of the Botan license
*/

#include <botan/cpuid.h>
#include <botan/types.h>
#include <botan/loadstor.h>
#include <botan/mem_ops.h>

#if defined(BOTAN_TARGET_ARCH_IS_IA32) || defined(BOTAN_TARGET_ARCH_IS_AMD64)

#if defined(BOTAN_BUILD_COMPILER_IS_MSVC)

  #include <intrin.h>
  #define CALL_CPUID(type, out) do { __cpuid((int*)out, type); } while(0)

#elif defined(BOTAN_BUILD_COMPILER_IS_INTEL)

  #include <ia32intrin.h>
  #define CALL_CPUID(type, out) do { __cpuid(out, type); } while(0);

#elif defined(BOTAN_BUILD_COMPILER_IS_GCC)

  #include <cpuid.h>
  #define CALL_CPUID(type, out) \
    do { __get_cpuid(type, out, out+1, out+2, out+3); } while(0);

#endif

#endif

#ifndef CALL_CPUID
  // In all other cases, just zeroize the supposed cpuid output
  #define CALL_CPUID(type, out) \
    do { out[0] = out[1] = out[2] = out[3] = 0; } while(0);
#endif

namespace Botan {

namespace {

u32bit get_x86_cache_line_size()
   {
   const u32bit INTEL_CPUID[3] = { 0x756E6547, 0x6C65746E, 0x49656E69 };
   const u32bit AMD_CPUID[3] = { 0x68747541, 0x444D4163, 0x69746E65 };

   u32bit cpuid[4] = { 0 };
   CALL_CPUID(0, cpuid);

   if(same_mem(cpuid + 1, INTEL_CPUID, 3))
      {
      CALL_CPUID(1, cpuid);
      return 8 * get_byte(2, cpuid[1]);
      }
   else if(same_mem(cpuid + 1, AMD_CPUID, 3))
      {
      CALL_CPUID(0x80000005, cpuid);
      return get_byte(3, cpuid[2]);
      }
   else
      return 32; // default cache line guess
   }

}

/*
* Call the x86 CPUID instruction and return the contents of ecx and
* edx, which contain the feature masks.
*/
u64bit CPUID::x86_processor_flags()
   {
   static u64bit proc_flags = 0;

   if(proc_flags)
      return proc_flags;

   u32bit cpuid[4] = { 0 };
   CALL_CPUID(1, cpuid);

   // Set the FPU bit on to force caching in proc_flags
   proc_flags = ((u64bit)cpuid[2] << 32) | cpuid[3] | 1;

   return proc_flags;
   }

u32bit CPUID::cache_line_size()
   {
   static u32bit cl_size = 0;

   if(cl_size)
      return cl_size;

   cl_size = get_x86_cache_line_size();

   return cl_size;
   }

bool CPUID::has_altivec()
   {
   static bool first_time = true;
   static bool altivec_capable = false;

   if(first_time)
      {
#if defined(BOTAN_TARGET_ARCH_IS_PPC) || defined(BOTAN_TARGET_ARCH_IS_PPC64)

      /*
      PVR identifiers for various AltiVec enabled CPUs. Taken from
      PearPC and Linux sources, mostly.
      */
      const u16bit PVR_G4_7400  = 0x000C;
      const u16bit PVR_G5_970   = 0x0039;
      const u16bit PVR_G5_970FX = 0x003C;
      const u16bit PVR_G5_970MP = 0x0044;
      const u16bit PVR_G5_970GX = 0x0045;
      const u16bit PVR_POWER6   = 0x003E;
      const u16bit PVR_CELL_PPU = 0x0070;

      // Motorola produced G4s with PVR 0x800[0123C] (at least)
      const u16bit PVR_G4_74xx_24  = 0x800;

      /*
      On PowerPC, MSR 287 is PVR, the Processor Version Number

      Normally it is only accessible to ring 0, but Linux and NetBSD
      (at least) will trap and emulate it for us. This is roughly 20x
      saner than every other approach I've seen for AltiVec detection
      (all of which are entirely OS specific, to boot).

      Apparently OS X doesn't support this, but then again OS X
      doesn't really support PPC anymore, so I'm not worrying about it.

      For OSes that aren't (known to) support the emulation, skip the
      call, leaving pvr as 0 which will cause all subsequent model
      number checks to fail (and we'll assume no AltiVec)
      */

#if defined(BOTAN_TARGET_OS_IS_LINUX) || defined(BOTAN_TARGET_OS_IS_NETBSD)
  #define BOTAN_TARGET_OS_SUPPORTS_MFSPR_EMUL
#endif

      u32bit pvr = 0;

#if defined(BOTAN_TARGET_OS_SUPPORTS_MFSPR_EMUL)
      asm volatile("mfspr %0, 287" : "=r" (pvr));
#endif
      // Top 16 bit suffice to identify model
      pvr >>= 16;

      altivec_capable |= (pvr == PVR_G4_7400);
      altivec_capable |= ((pvr >> 8) == PVR_G4_74xx_24);
      altivec_capable |= (pvr == PVR_G5_970);
      altivec_capable |= (pvr == PVR_G5_970FX);
      altivec_capable |= (pvr == PVR_G5_970MP);
      altivec_capable |= (pvr == PVR_G5_970GX);
      altivec_capable |= (pvr == PVR_CELL_PPU);
#endif

      first_time = false;
      }

   return altivec_capable;
   }

}