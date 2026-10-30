"""Validate the checksum and appended SHA-256 of an ESP application image."""
from pathlib import Path
import hashlib
import json
import struct
import sys
p=Path(sys.argv[1]);b=p.read_bytes()
assert b[0]==0xE9 and b[1]<=16, 'Invalid ESP image header'
assert struct.unpack_from('<H',b,12)[0]==9, 'Expected ESP32-S3 image'
position=24;checksum=0xEF;segments=[]
for _ in range(b[1]):
    address,length=struct.unpack_from('<II',b,position);position+=8
    assert position+length<=len(b), 'Truncated segment'
    for byte in b[position:position+length]: checksum ^= byte
    segments.append({'address':hex(address),'size':length})
    position+=length
checksum_at=position+(15-position%16)%16
assert b[checksum_at]==checksum, 'Checksum mismatch'
assert b[23]==1, 'Expected appended image SHA-256'
assert hashlib.sha256(b[:checksum_at+1]).digest()==b[checksum_at+1:checksum_at+33], 'Image SHA-256 mismatch'
assert struct.unpack_from('<I',b,32)[0]==0xABCD5432, 'Expected application descriptor, not bootloader'
print(json.dumps({'file':str(p),'size':len(b),'sha256':hashlib.sha256(b).hexdigest(),
                  'chip':'ESP32-S3','checksum_valid':True,'image_sha256_valid':True,'segments':segments},indent=2))
