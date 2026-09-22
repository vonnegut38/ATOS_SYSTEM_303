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
  for local_name,up_name in [('seq','vga_TS_1_4'),('extseq','et4000_TS_6_8'),('crtc','vga_CRTC_0_0x18'),('attr','vga_ATC_0_0x16'),('gdc','vga_GDC_0_8')]:
   self.assertEqual(table(local,local_name),table(upstream,up_name),local_name)
