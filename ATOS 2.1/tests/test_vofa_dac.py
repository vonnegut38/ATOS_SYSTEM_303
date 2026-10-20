"""RAMDAC family detection and the palette scaling that follows it.

The Nova drivers ship one build per DAC family (T0/T1/T3 = 8-bit, T4/T6 =
15-bit, T8 = 24-bit) because the palette path differs.  We probe 0x3C6 and fall
back to the CIC10487 (15-bit) part the VoFa cards carry, so both halves of that
rule are asserted here against the real driver code.
"""
import pathlib, subprocess, tempfile, textwrap, unittest


class VofaDacTest(unittest.TestCase):
    def test_detect_families_and_scale_palette(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        src = textwrap.dedent('''
        #include <stdint.h>
        #include "vofa_et4000.h"
        #include "vofa_et4000.c"

        static uint8_t regs[0x400];
        static uint8_t pal[8];   /* 0x3C9 is a data port: R,G,B in sequence */
        static int pal_n;
        static int behaviour;   /* 0 = true 8-bit mask, 1 = writes ignored, 2 = nibble kept */

        static int rd(uint32_t a, uint8_t *v)
        {
            uint8_t r = regs[a & 0x3ffu];
            if ((a & 0x3ffu) == 0x3c6u) {
                if (behaviour == 1) r = 0xf0u;
                else if (behaviour == 2) r = (uint8_t)(r & 0xf0u);
            }
            *v = r;
            return 1;
        }
        static int wr(uint32_t a, uint32_t v)
        {
            if ((a & 0x3ffu) == 0x3c6u && behaviour == 1) return 1;
            if ((a & 0x3ffu) == 0x3c9u && pal_n < 8) pal[pal_n] = (uint8_t)v, pal_n++;
            regs[a & 0x3ffu] = (uint8_t)v;
            return 1;
        }
        static void dly(void) {}

        int main(void)
        {
            /* No tick and no bulk fill callback: the DAC path needs neither. */
            vofa_io_t io = {rd, wr, dly, 0, 0, 0};

            /* A true 8-bit pixel mask stores 00 and FF. */
            behaviour = 0;
            vofa_et4000_dac_detect(&io, VOFA_DAC_UNKNOWN);
            if (vofa_et4000_dac() != VOFA_DAC_8BIT) return 1;

            /* Writes aliased away: inconclusive, so the CIC10487 default wins. */
            behaviour = 1;
            vofa_et4000_dac_detect(&io, VOFA_DAC_UNKNOWN);
            if (vofa_et4000_dac() != VOFA_DAC_15BIT) return 2;

            /* Only part of the pattern kept: also the 15-bit default. */
            behaviour = 2;
            vofa_et4000_dac_detect(&io, VOFA_DAC_UNKNOWN);
            if (vofa_et4000_dac() != VOFA_DAC_15BIT) return 3;

            /* An explicit override always wins. */
            vofa_et4000_dac_detect(&io, VOFA_DAC_24BIT);
            if (vofa_et4000_dac() != VOFA_DAC_24BIT) return 4;

            /* Palette values arrive as 6 bits per gun and are scaled per family. */
            behaviour = 0;
            vofa_et4000_dac_detect(&io, VOFA_DAC_8BIT);
            pal_n = 0;
            (void)vofa_et4000_set_palette(&io, 1u, 0x3fu, 0x20u, 0x00u);
            if (pal[0] != 0x3fu || pal[1] != 0x20u || pal[2] != 0x00u) return 5;
            vofa_et4000_dac_detect(&io, VOFA_DAC_15BIT);
            pal_n = 0;
            (void)vofa_et4000_set_palette(&io, 1u, 0x3fu, 0x20u, 0x00u);
            if (pal[0] != 0x1fu || pal[1] != 0x10u) return 6;   /* 5 bits/gun */
            vofa_et4000_dac_detect(&io, VOFA_DAC_24BIT);
            pal_n = 0;
            (void)vofa_et4000_set_palette(&io, 1u, 0x3fu, 0x20u, 0x00u);
            if (pal[0] != 0xffu || pal[1] != 0x82u) return 7;   /* 8 bits/gun */
            if (regs[0x3c8] != 1u) return 8;                    /* palette index */
            return 0;
        }
        ''')
        with tempfile.TemporaryDirectory() as temp:
            path = pathlib.Path(temp)
            (path / 't.c').write_text(src)
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-function',
                            '-I' + str(root / 'include'),
                            '-I' + str(root / 'system'),
                            str(path / 't.c'), '-o', str(path / 't')], check=True)
            subprocess.run([str(path / 't')], check=True)


if __name__ == '__main__':
    unittest.main()
