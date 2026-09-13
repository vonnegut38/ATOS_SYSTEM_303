import subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).parents[1]
class RomToolsTest(unittest.TestCase):
    def test_split_join_roundtrip(self):
        with tempfile.TemporaryDirectory() as d:
            d=Path(d); src=d/'rom.bin'; hi=d/'hi.bin'; lo=d/'lo.bin'; out=d/'out.bin'
            src.write_bytes(bytes(range(256))*4)
            subprocess.run([sys.executable,ROOT/'tools/splitrom.py','split',src,hi,lo],check=True)
            self.assertEqual(hi.read_bytes()[:3],bytes((0,2,4)))
            self.assertEqual(lo.read_bytes()[:3],bytes((1,3,5)))
            subprocess.run([sys.executable,ROOT/'tools/splitrom.py','join',hi,lo,out],check=True)
            self.assertEqual(src.read_bytes(),out.read_bytes())
    def test_padding(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'r'; p.write_bytes(b'abc')
            subprocess.run([sys.executable,ROOT/'tools/padrom.py',p,'8'],check=True)
            self.assertEqual(p.read_bytes(),b'abc'+b'\xff'*5)
if __name__=='__main__': unittest.main()
