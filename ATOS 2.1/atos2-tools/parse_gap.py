import re
from pathlib import Path
NAMES = ["0 timer/switch","1 input poll","2 ksessions","3 consumer loop","4 notify",
         "5 dispatch ready","6 dispatch output","7 ksessions2","8 kconsole_flush",
         "9 wm_frame_tick","10 wm_applet_bar","11 ksessions_paint","12 kvideo_flush",
         "13","14","15"]
text = Path("/Users/xxx/Documents/Deepseek_test/ATOS 2.1/work/hatari-gap.log.out").read_text(errors="replace")
lines = text.splitlines()
hexline = re.compile(r"^[0-9A-Fa-f]{8}:")
blocks = []
i = 0
while i < len(lines):
    if "memdump 0x9a64e 128" in lines[i]:
        data = []
        j = i + 1
        while j < len(lines) and hexline.match(lines[j]):
            hexpart = lines[j].split(":", 1)[1][:48]
            for tok in hexpart.split():
                if len(tok) == 2 and all(c in "0123456789abcdefABCDEF" for c in tok):
                    data.append(int(tok, 16))
            j += 1
        blocks.append(data)
        i = j
    else:
        i += 1
for bi, data in enumerate(blocks):
    words = [int.from_bytes(bytes(data[k:k+4]), "big") for k in range(0, len(data) - 3, 4)]
    pending = words[0:16]
    overrun = words[16:32]
    print(f"block {bi}: bytes={len(data)} words={len(words)}")
    print(f"  pending {pending}")
    print(f"  overrun {overrun}")
    for k in range(16):
        if pending[k] or overrun[k]:
            print(f"    id {k:2d} {NAMES[k]:20s} overrun={overrun[k]:4d} pending={pending[k]:4d}")
