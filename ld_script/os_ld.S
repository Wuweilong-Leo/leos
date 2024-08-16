MEMORY
{
    LOADER_MEM(rwx): ORIGIN = 0x00001000, LENGTH = 1024
    KERNEL_STACK_MEM(rwx): ORIGIN = 0x00002000, LENGTH = 0x4000
	MBR_MEM(rwx): ORIGIN = 0x00007C00, LENGTH = 512
/* 编译出来是C000D000起始，实际上加载到D000，然后通过页表映射到实际物理地址 */
    KERNEL_MEM(rwx) : ORIGIN = 0xC000D000, LENGTH = 0x60000
    VEDIO_MEM(rwx): ORIGIN = 0x0000B8000, LENGTH = 0x8000
    PG_TAB_MEM(rwx): ORIGIN = 0x00100000, LENGTH = 0x101000
}

ENTRY(main)

SECTIONS
{
    .LOADER :
    {   
        . = ALIGN(4);
        _os_loader_start = .;
        *(.os.loader.text)
        *(.os.loader.data)
        *(.os.loader.bss)
        . = ALIGN(8);
        _os_gdt_start = .;
        *(.os.gdt.data)
        _os_gdt_end = .;
        _os_loader_end = .;
    } > LOADER_MEM

    .MBR : 
    {
        _os_mbr_start = .;
        *(.os.mbr.text)
        _os_mbr_end = _os_mbr_start + 508;
        . = _os_mbr_end;
        LONG(0xaa550000)
    } > MBR_MEM

    .PG_TAB :
    {
        . = ALIGN(0x1000);
        _os_pg_tab_start = .;
        *(.os.pgt.data)
        _os_pg_tab_end = .;
    } > PG_TAB_MEM

    .KERNEL_STACK :
    {
        . = ALIGN(4);
        _os_kernel_sp_start = .;
        . += 0x4000;
        _os_kernel_sp_end = .;
    } > KERNEL_STACK_MEM

    .KERNEL_TEXT : 
    {
        . = ALIGN(4);
        *(.os.kernel.text)
    } > KERNEL_MEM

    .KENNEL_BSS : 
    {
        . = ALIGN(4);
        _os_kernel_bss_start = .;
        *(.os.kernel.bss)
        _os_kernel_bss_end = .;
    } > KERNEL_MEM

    .KERNEL_DATA : 
    {
        . = ALIGN(4);
        *(.rodata)
        _os_bss_table_start = .;
        LONG(_os_kernel_bss_start)
        LONG(_os_kernel_bss_end)
        _os_bss_table_end = .;
        *(.os.kernel.data)
    } > KERNEL_MEM
}