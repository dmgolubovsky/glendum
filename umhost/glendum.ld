OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
phys = 0x70000000;

SECTIONS
{
  .text phys : AT(phys) {
    code = .;
    *(.text)
    *(.rodata)
    . = ALIGN(4096);
    textend = .;
  }
  .data : AT(phys + textend)
  {
    data = .;
    *(.data)
    . = ALIGN(4096);
    dataend = .;
  }
  .bss : AT(phys + dataend)
  {
    bss = .;
    *(.bss)
    . = ALIGN(4096);
    end = .;
  }
}

