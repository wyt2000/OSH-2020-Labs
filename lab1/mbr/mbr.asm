[BITS 16]                               ; 16 bits program\r
[ORG 0x7C00]                            ; starts from 0x7c00, where MBR lies in memory

sti
mov ah,0x00
mov al,0x03
int 0x10								; call BIOS interrupt procedure, clear the screen


print_str:
 	mov word [0x046C],0					; clear the timer
test_timer:
	cmp word [0x046C],18				; when timer gets 18, print the mess
	jle	test_timer
	mov si, OSH							; si points to string OSH
start:	
    lodsb                               ; load char to al
    cmp al, 0                           ; is it the end of the string?
    je return                           ; if true, then return to the first character
    mov ah, 0x0e                        ; if false, then set AH = 0x0e 
    int 0x10                            ; call BIOS interrupt procedure, print a char to screen
    jmp start                           ; loop over to print all chars

return:
    jmp print_str

OSH db `Hello, OSH 2020 Lab1!\n\r`, 0       ; our string, null-terminated

TIMES 510 - ($ - $$) db 0               ; the size of MBR is 512 bytes, fill remaining bytes to 0
DW 0xAA55                               ; magic number, mark it as a valid bootloader to BIOS 
