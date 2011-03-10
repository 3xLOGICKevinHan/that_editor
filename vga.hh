#ifdef __BORLANDC__
unsigned short* VidMem = (unsigned short *) MK_FP(0xB800, 0x0000);
#else
//unsigned short VidMem[132*43];
#endif

unsigned char VidW=80, VidH=25, VidCellHeight=16;
const unsigned char* VgaFont = 0;
int C64palette = 0, FatMode = 0;

static const unsigned char c64font[8*(256-64)] = {
#include "c64font.inc"
};
static const unsigned char p32font[32*256] = {
#include "8x32.inc"
};
static const unsigned char p19font[19*256] = {
#include "8x19.inc"
};
static const unsigned char p32wfont[32*256] = {
#include "16x32.inc"
};

void VgaGetFont()
{
    if(C64palette) { VgaFont = c64font-8*32; return; }

    unsigned char mode = 1;
    switch(VidCellHeight)
    {
        case 8: mode = 3; break;
        case 14: mode = 2; break;
        case 16: mode = 6; break;
        case 19: case 20: { VgaFont = p19font; return; }
        case 32: { VgaFont = FatMode ? p32wfont : p32font; return; }
        default: mode = 1; break;
    }
    _asm { push es; push bp
           mov ax, 0x1130
           mov bh, mode
           int 0x10
           mov word ptr VgaFont+0, bp
           mov word ptr VgaFont+2, es
           xchg cx,cx
           xchg dx,dx
           pop bp
           pop es }
}

void VgaGetMode()
{
#ifdef __BORLANDC__
    _asm { mov ah, 0x0F; int 0x10; mov VidW, ah }
    _asm { mov ax, 0x1130; xor bx,bx; int 0x10; mov VidH, dl; mov VidCellHeight, cl }
    if(VidH == 0) VidH = 25; else VidH += 1;
    _asm { mov ax, 0x1003; xor bx,bx; int 0x10 } // Disable blink-bit
    VgaGetFont();
#endif
    if(C64palette) { VidW -= 4; VidH -= 5; }
    if(FatMode) VidW /= 2;
}

void VgaSetMode(unsigned modeno)
{
    if(modeno < 0x100)
        _asm { mov ax, modeno; int 0x10 }
    else
        _asm { mov bx, modeno; mov ax, 0x4F02; int 0x10 }

    static const unsigned long c64pal[16] =
    {
        0x3E31A2ul, // 0
        0x000000ul,
        0x68A141ul,
        0x7ABFC7ul,
        0xFCFCFCul, // 4
        0x8A46AEul,
        0x905F25ul,
        0x7C70DAul,
        0x3E31A2ul, // 8
        0x7C70DAul,
        0xACEA88ul, 
        0x7ABFC7ul,
        0xBB776Dul, // C
        0x8A46AEul,
        0xD0DC71ul,
        0x7ABFC7ul
    };
    outportb(0x3C8, 0x20);
    for(unsigned a=0; a<16; ++a)
    {
        outportb(0x3C9, c64pal[a] >> 18);
        outportb(0x3C9, (c64pal[a] >> 10) & 0x3F);
        outportb(0x3C9, (c64pal[a] >> 2) & 0x3F);
    }
}

void VgaSetCustomMode(
    unsigned width,
    unsigned height,
    unsigned font_height,
    int is_9pix,
    int is_half/*horizontally doubled*/,
    int is_double/*vertically doubled*/)
{
    if(C64palette)
    {
        width  += 4;
        height += 5;
    }
    if(FatMode)
        width *= 2;

    unsigned hdispend = width;
    unsigned vdispend = height*font_height;
    if(is_double) vdispend *= 2;
    unsigned htotal = width*5/4;
    unsigned vtotal = vdispend+45;

    // Set standard 80x25 mode as a baseline
    // This triggers font reset on JAINPUT.
    *(unsigned char*)MK_FP(0x40,0x87) |= 0x80;
    VgaSetMode(3);
    *(unsigned char*)MK_FP(0x40,0x87) &= ~0x80;

    if(font_height ==14) { _asm { mov ax, 0x1101; mov bl, 0; int 0x10 } }
    if(font_height == 8) {
        _asm { mov ax, 0x1102; mov bl, 0; int 0x10 }
        if(C64palette)
        {
            _asm {
                push es
                push bp
                 mov ax, seg c64font
                 mov es, ax
                 mov bp, offset c64font
                 mov ax, 0x1100
                 mov bx, 0x800
                 mov cx, 256 - 64
                 mov dx, 32      // Contains: 20..DF
                 int 0x10
                pop bp
                pop es
            }
        }
    }
    if(font_height == 32) {
        const unsigned char* font = FatMode ? p32wfont : p32font;
        _asm {
            push es
            push bp
             les bp, font
             mov ax, 0x1100
             mov bx, 0x2000
             mov cx, 256
             mov dx, 0
             int 0x10
            pop bp
            pop es
        }
    }
    if(font_height == 19) {
        _asm {
            push es
            push bp
             mov ax, seg p19font
             mov es, ax
             mov bp, offset p19font
             mov ax, 0x1100
             mov bx, 0x1300
             mov cx, 256
             mov dx, 0
             int 0x10
            pop bp
            pop es
        }
    }

    /* This script is, for the most part, copied from DOSBox. */

    {unsigned char Seq[5] = { 0, !is_9pix + 8*is_half, 3, 0, 6 };
    for(unsigned a=0; a<5; ++a) outport(0x3C4, a | (Seq[a] << 8));}

    // Disable write protection
    outportb(0x3D4, 0x11); outportb(0x3D5, inportb(0x3D5)&0x7F);
    // crtc
    {for(unsigned a=0; a<=0x18; ++a) outport(0x3D4, a | 0x0000);}
    unsigned overflow = 0, max_scanline = 0, ver_overflow = 0, hor_overflow = 0;
    outport(0x3D4, 0x00 | ((htotal-5) << 8));   hor_overflow |= ((htotal-5) >> 8) << 0;
    outport(0x3D4, 0x01 | ((hdispend-1) << 8)); hor_overflow |= ((hdispend-1) >> 8) << 1;
    outport(0x3D4, 0x02 | ((hdispend) << 8));   hor_overflow |= ((hdispend) >> 8) << 2;
    unsigned blank_end = (htotal-2) & 0x7F;
    outport(0x3D4, 0x03 | ((0x80|blank_end) << 8));
    unsigned ret_start = is_half ? hdispend+3 : hdispend+5;
    outport(0x3D4, 0x04 | (ret_start << 8));    hor_overflow |= ((ret_start) >> 8) << 4;
    unsigned ret_end = is_half ? ((htotal-18)&0x1F)|(is_double?0x20:0) : ((htotal-4)&0x1F);
    outport(0x3D4, 0x05 | ((ret_end | ((blank_end & 0x20) << 2)) << 8));
    outport(0x3D4, 0x06 | ((vtotal-2) << 8));
    overflow |= ((vtotal-2) & 0x100) >> 8;
    overflow |= ((vtotal-2) & 0x200) >> 4;
    ver_overflow |= (((vtotal-2) & 0x400) >> 10);
    unsigned vretrace = vdispend + (4800/vdispend + (vdispend==350?24:0));
    outport(0x3D4, 0x10 | (vretrace << 8));
    overflow |= (vretrace & 0x100) >> 6;
    overflow |= (vretrace & 0x200) >> 2;
    ver_overflow |= (vretrace & 0x400) >> 6;
    outport(0x3D4, 0x11 | (((vretrace+2) & 0xF) << 8));
    outport(0x3D4, 0x12 | ((vdispend-1) << 8));
    overflow |= ((vdispend-1) & 0x100) >> 7;
    overflow |= ((vdispend-1) & 0x200) >> 3;
    ver_overflow |= ((vdispend-1) & 0x400) >> 9;
    unsigned vblank_trim = vdispend / 66;
    outport(0x3D4, 0x15 | ((vdispend+vblank_trim) << 8));
    overflow |= ((vdispend+vblank_trim) & 0x100) >> 5;
    max_scanline |= ((vdispend+vblank_trim) & 0x200) >> 4;
    ver_overflow |= ((vdispend+vblank_trim) & 0x400) >> 8;
    outport(0x3D4, 0x16 | ((vtotal-vblank_trim-2) << 8));
    unsigned line_compare = vtotal < 1024 ? 1023 : 2047;
    outport(0x3D4, 0x18 | (line_compare << 8));
    overflow |= (line_compare & 0x100) >> 4;
    max_scanline |= (line_compare & 0x200) >> 3;
    ver_overflow |= (line_compare & 0x400) >> 4;
    if(is_double) max_scanline |= 0x80;
    max_scanline |= font_height-1;
    unsigned underline = vdispend == 350 ? 0x0F : 0x1F;
    outport(0x3D4, 0x09 | (max_scanline << 8));
    outport(0x3D4, 0x14 | (underline << 8));
    outport(0x3D4, 0x07 | (overflow << 8));
    outport(0x3D4, 0x5D | (hor_overflow << 8)); // S3 trio extension
    outport(0x3D4, 0x5E | (ver_overflow << 8)); // S3 trio extension
    unsigned offset = hdispend / 2;
    outport(0x3D4, 0x13 | (offset << 8));
    outport(0x3D4, 0x51 | (((offset & 0x300) >> 4) << 8));
    outport(0x3D4, 0x69 | (0 << 8)); // S3 trio
    outport(0x3D4, 0x5E | (ver_overflow << 8)); // (repeat for some reason)
    unsigned modecontrol = 0xA3;
    outport(0x3D4, 0x17 | (modecontrol << 8));
    // Enable write protection
    outportb(0x3D4, 0x11); outportb(0x3D5, inportb(0x3D5)|0x80);
    outport(0x3D4, 0x67 | (0 << 8)); // S3 trio

    outportb(0x3C2, 0x63/*0x02*/); // misc output

    {unsigned char Gfx[9] = { 0,0,0,0,0,0x10,0x0E,0x0F,0xFF };
    for(unsigned a=0; a<9; ++a) outport(0x3CE, a | (Gfx[a] << 8));}

    {unsigned char Att[0x15] = { 0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,
                                 0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
                                 is_9pix*4, 0, 0x0F, is_9pix*8, 0 };
    if(C64palette)
    {
        for(unsigned a=0; a<0x10; ++a) Att[a] = 0x20+a;
    }

    _asm { mov dx,0x3DA; in al,dx }
    {for(unsigned a=0x0; a<0x15; ++a)
        { if(a == 0x11) continue;
          outportb(0x3C0, a);
          outportb(0x3C0, Att[a]); }} }
    _asm { mov dx,0x3DA; in al,dx }
    outportb(0x3C0, 0x20);

    *(unsigned char*)MK_FP(0x40,0x10) &= ~0x30;
    *(unsigned char*)MK_FP(0x40,0x10) |= 0x20;

    *(unsigned char*)MK_FP(0x40,0x65) = 0x29;
    *(unsigned char*)MK_FP(0x40, 0x4A) = width;
    *(unsigned char*)MK_FP(0x40, 0x84) = height-1;
    *(unsigned char*)MK_FP(0x40, 0x85) = font_height;
    *(unsigned short*)MK_FP(0x40, 0x4C) = width*height*2;

    if(C64palette)
    {
        memset(VidMem, 0x99, width*height*2);
        width -= 4; height -= 5;
    }
    if(FatMode)
        width /= 2;

    VidW = width;
    VidH = height;
    VidCellHeight = font_height;
    VgaGetFont();
}

inline unsigned short* GetVidMem(unsigned x, unsigned y)
{
    if(C64palette)
    {
        return VidMem + (x+2) + (y+2)*(VidW+4);
    }
    if(FatMode)
    {
        return VidMem + (x + y*VidW)*2;
    }
    return VidMem + x + y*VidW;
}