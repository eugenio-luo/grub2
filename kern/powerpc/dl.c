/* dl.c - arch-dependent part of loadable module support */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002, 2004  Free Software Foundation, Inc.
 *
 *  GRUB is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <grub/dl.h>
#include <grub/elf.h>
#include <grub/misc.h>
#include <grub/err.h>

/* Check if EHDR is a valid ELF header.  */
int
grub_arch_dl_check_header (void *ehdr, unsigned size)
{
  Elf32_Ehdr *e = ehdr;

  /* Check the header size.  */
  if (size < sizeof (Elf32_Ehdr))
    return 0;

  /* Check the magic numbers.  */
  if (!((e->e_ident[EI_MAG0] == ELFMAG0) 
	&& (e->e_ident[EI_MAG1] == ELFMAG1)
	&& (e->e_ident[EI_MAG2] == ELFMAG2) 
	&& (e->e_ident[EI_MAG3] == ELFMAG3)
	&& (e->e_ident[EI_CLASS] == ELFCLASS32) 
	&& (e->e_ident[EI_DATA] == ELFDATA2MSB)
	&& (e->e_ident[EI_VERSION] == EV_CURRENT) 
	&& (e->e_type == ET_REL) && (e->e_machine == EM_PPC) 
	&& (e->e_version == EV_CURRENT)))
    return 0;
  
  /* Make sure that every section is within the core.  */
  if (size < e->e_shoff + e->e_shentsize * e->e_shnum)
    return 0;

  return 1;
}


/* Relocate symbols.  */
grub_err_t
grub_arch_dl_relocate_symbols (grub_dl_t mod, void *ehdr)
{
  Elf32_Ehdr *e = ehdr;
  Elf32_Shdr *s;
  Elf32_Sym *symtab;
  Elf32_Word entsize;
  unsigned i;
  
  /* Find a symbol table.  */
  for (i = 0, s = (Elf32_Shdr *) ((char *) e + e->e_shoff);
       i < e->e_shnum;
       i++, s = (Elf32_Shdr *) ((char *) s + e->e_shentsize))
    if (s->sh_type == SHT_SYMTAB)
      break;

  if (i == e->e_shnum)
    return grub_error (GRUB_ERR_BAD_MODULE, "no symtab found");
  
  symtab = (Elf32_Sym *) ((char *) e + s->sh_offset);
  entsize = s->sh_entsize;
  
  for (i = 0, s = (Elf32_Shdr *) ((char *) e + e->e_shoff);
       i < e->e_shnum;
       i++, s = (Elf32_Shdr *) ((char *) s + e->e_shentsize))
    if (s->sh_type == SHT_RELA)
      {
	grub_dl_segment_t seg;

	/* Find the target segment.  */
	for (seg = mod->segment; seg; seg = seg->next)
	  if (seg->section == s->sh_info)
	    break;

	if (seg)
	  {
	    Elf32_Rela *rel, *max;
	    
	    for (rel = (Elf32_Rela *) ((char *) e + s->sh_offset),
		   max = rel + s->sh_size / s->sh_entsize;
		 rel < max;
		 rel++)
	      {
		Elf32_Word *addr;
		Elf32_Sym *sym;
		grub_uint32_t value;
		
		if (seg->size < rel->r_offset)
		  return grub_error (GRUB_ERR_BAD_MODULE,
				     "reloc offset is out of the segment");
		
		addr = (Elf32_Word *) ((char *) seg->addr + rel->r_offset);
		sym = (Elf32_Sym *) ((char *) symtab
				     + entsize * ELF32_R_SYM (rel->r_info));
		
		/* On the PPC the value does not have an explicit
		   addend, add it.  */
		value = sym->st_value + rel->r_addend;
		switch (ELF32_R_TYPE (rel->r_info))
		  {
		  case R_PPC_ADDR16_LO:
		    *(Elf32_Half *) addr = value;
		    break;
		    
		  case R_PPC_REL24:
		    {
		      Elf32_Sword delta = value - (Elf32_Word) addr;
		      
		      if (delta << 6 >> 6 != delta)
			return grub_error (GRUB_ERR_BAD_MODULE, "Relocation overflow");
		      *addr = (*addr & 0xfc000003) | (delta & 0x3fffffc);
		      break;
		    }
		    
		  case R_PPC_ADDR16_HA:
		    *(Elf32_Half *) addr = (value + 0x8000) >> 16;
		    break;
		    
		  case R_PPC_ADDR32:
		    *addr = value;
		    break;
		  }
	      }
	  }
      }
  
  return GRUB_ERR_NONE;
}
