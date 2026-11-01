#pragma once
#include <stdint.h>

/* bochs/qemu "vbe dispi" linear framebuffer. text mode stays the default; a program
   switches to graphics with fb_acquire() and gets text mode back on release/exit */
struct proc;
int  fb_acquire(void* owner, uint32_t* info /* width,height,pitch,bpp,vaddr */);  /* 0 ok, -1 no device / busy */
void fb_release(void* owner);   /* no-op unless owner holds the framebuffer */
