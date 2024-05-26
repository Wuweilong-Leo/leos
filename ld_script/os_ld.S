MEMORY
{
	MBR_MEM(rwx): ORIGIN = 0x00007C00, LENGTH = 512
    LOADER_MEM(rwx): ORIGIN = 0x00001000, LENGTH = 0x6000
    L1M(rwx) : ORIGIN = 0x00010000, LENGTH = 0x40000
    L2M(rwx) : ORIGIN = 0x00050000, LENGTH = 0xB0000
}

ENTRY(main)

SECTIONS
{
    .L1M_LOADER :
    {   
        . = ALIGN(4);
        *(.os.loader.text)
        *(.os.loader.data)
        *(.os.loader.bss)
        . = ALIGN(4);
        _os_boot_sp_start = .;
        . += 0x4000;
        _os_boot_sp_end = .;
    } > LOADER_MEM

    .L1M_MBR : 
    {
        _os_mbr_start = .;
        *(.os.mbr.text)
        _os_mbr_end = _os_mbr_start + 508;
        . = _os_mbr_end;
        LONG(0xaa550000)
    } > MBR_MEM

    .L1M_BSS : 
    {
        . = ALIGN(4);
        *(.os.l1.bss)
    } > L1M

    .L1M_DATA : 
    {
        . = ALIGN(4);
        *(.os.l1.data)
    } > L1M

    .L1M_TEXT : 
    {
        . = ALIGN(4);
        *(.os.l1.text)
    } > L1M

    .L2M_BSS : 
    {
        . = ALIGN(4);
        *(.os.l2.bss)
    } > L2M

    .L2M_DATA : 
    {
        . = ALIGN(4);
        *(.os.l2.data)
    } > L2M

    .L2M_TEXT : 
    {
        . = ALIGN(4);
        *(.os.l2.text)
    } > L2M
}