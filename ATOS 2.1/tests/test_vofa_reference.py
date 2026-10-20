"""Verify hardware tables against the exact licensed upstream snapshot."""
import pathlib,re,unittest
class VofaReference(unittest.TestCase):
 def test_register_tables(self):
  root=pathlib.Path(__file__).resolve().parents[1]
  upstream=(root/'vendor/emutos/nova.c').read_text()
  local=(root/'system/vofa_et4000.c').read_text()
  def table(text,name):
   body=re.search(r'\b'+name+r'\[\]\s*=\s*\{([^}]+)\}',text).group(1)
   return [int(v.strip(),0) for v in body.split(',') if v.strip()]
  for local_name,up_name in [('seq','vga_TS_1_4'),('extseq','et4000_TS_6_8'),('attr','vga_ATC_0_0x16'),('gdc','vga_GDC_0_8')]:
   self.assertEqual(table(local,local_name),table(upstream,up_name),local_name)
  # The 640 CRTC keeps the upstream H timing (00..05) and row offset (13), but
  # uses a deliberate 400-line vertical timing: the ATOS view is a true
  # 640x400, while the original EmuTOS table scans 480 and left a black band.
  local_crtc=table(local,'crtc');up_crtc=table(upstream,'vga_CRTC_0_0x18')
  for i in (0x00,0x01,0x02,0x03,0x04,0x05,0x13):
   self.assertEqual(local_crtc[i],up_crtc[i],'crtc[%02x]'%i)
  self.assertEqual(local_crtc[0x12],0x8f,'640 mode must display 400 lines')
  self.assertTrue(local_crtc[0x07]&0x02 and not (local_crtc[0x07]&0x40),
                  'VDisplayEnd bit8 set, bit9 clear -> 400 lines')
 def test_extra_modes_are_selected_at_runtime(self):
  """VOFA.SYS is loaded without relocation, so the mode table must not store
  pointers to the CRTC arrays: those would keep their link-time (low RAM)
  address and program the card with garbage."""
  root=pathlib.Path(__file__).resolve().parents[1]
  local=(root/'system/vofa_et4000.c').read_text()
  self.assertNotIn('const uint8_t *crtc;',local)
  self.assertIn('crtc_for',local)
  self.assertIn('vofa_et4000_set_mode',local)
  for size in ('crtc_800','crtc_1024','VOFA_MODE_800','VOFA_MODE_1024'):
   self.assertIn(size,local)
