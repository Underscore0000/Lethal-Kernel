#!/usr/bin/env python3
import math, struct, sys, os
from pathlib import Path

SECTOR=512; TOTAL=32768; RESERVED=1; FATS=2; ROOT_ENTRIES=512; SPC=1; ROOTSEC=32

spf=1
while True:
    clusters=(TOTAL-RESERVED-FATS*spf-ROOTSEC)//SPC
    need=math.ceil((clusters+2)*2/SECTOR)
    if need==spf: break
    spf=need

DATA=RESERVED+FATS*spf+ROOTSEC

def p16(b,o,v):struct.pack_into('<H',b,o,v)
def p32(b,o,v):struct.pack_into('<I',b,o,v)

def n83(n):
    x=n.upper().split('.',1)
    return (x[0][:8].ljust(8)+(x[1][:3] if len(x)>1 else '').ljust(3)).encode()

def find_free_root_entry(img):
    rb=(RESERVED+FATS*spf)*SECTOR
    for off in range(0, ROOT_ENTRIES*32, 32):
        if img[rb+off] == 0 or img[rb+off] == 0xE5:
            return rb+off
    return -1

def main(out, *files):
    out_dir = os.path.dirname(out)
    if out_dir and not os.path.exists(out_dir):
        os.makedirs(out_dir)
    
    if os.path.exists(out):
        print(f"Overwriting existing image: {out}")
    
    if not files:
        print("ERROR: at least one file required")
        sys.exit(1)
    
    file_data = []
    total_size = 0
    for file_path in files:
        try:
            data = Path(file_path).read_bytes()
            name = Path(file_path).name.upper()
            file_data.append((name, data))
            total_size += len(data)
            print(f"  Adding {name} ({len(data)} bytes)")
        except FileNotFoundError:
            print(f"WARNING: {file_path} not found, skipping")
    
    if not file_data:
        print("ERROR: no valid files to add")
        sys.exit(1)
    
    img = bytearray(TOTAL * SECTOR)
    b = memoryview(img)[:SECTOR]
    
    b[:3]=b'\xEB\x3C\x90'
    b[3:11]=b'LethalOS'
    p16(b,11,512)
    b[13]=SPC
    p16(b,14,1)
    b[16]=FATS
    p16(b,17,ROOT_ENTRIES)
    p16(b,19,TOTAL)
    b[21]=0xF8
    p16(b,22,spf)
    p16(b,24,32)
    p16(b,26,64)
    b[38]=0x29
    p32(b,39,0x4C4B3130)
    b[43:54]=b'LETHAL OS  '
    b[54:62]=b'FAT16   '
    b[510:512]=b'\x55\xAA'
    
    for fi in range(FATS):
        base=(RESERVED+fi*spf)*SECTOR
        p16(img,base,0xFFF8)
        p16(img,base+2,0xFFFF)
    
    total_clusters = 0
    for name, data in file_data:
        need = max(1, math.ceil(len(data) / SECTOR))
        total_clusters += need
    
    if total_clusters > clusters - 1:
        print(f"ERROR: files too large ({total_clusters} clusters > {clusters-1})")
        sys.exit(1)
    
    cluster = 2
    
    for name, data in file_data:
        need = max(1, math.ceil(len(data) / SECTOR))
        first_cluster = cluster
        

        for c in range(need):
            for fi in range(FATS):
                offset = (RESERVED + fi * spf) * SECTOR + cluster * 2
                if c == need - 1:
                    p16(img, offset, 0xFFFF)
                else:
                    p16(img, offset, cluster + 1)
            cluster += 1
        

        db = (DATA + (first_cluster - 2) * SPC) * SECTOR
        img[db:db+len(data)] = data
        
        entry_pos = find_free_root_entry(img)
        if entry_pos == -1:
            print(f"ERROR: root directory full, cannot add {name}")
            sys.exit(1)
        
        e = memoryview(img)[entry_pos:entry_pos+32]
        e[:11] = n83(name)
        e[11] = 0x20
        p16(e, 26, first_cluster)
        p32(e, 28, len(data))
    
    Path(out).write_bytes(img)
    print(f'FAT16 image: {len(img)//(1024*1024)} MiB, {len(file_data)} files, {total_clusters} clusters')
    print(f'  Created: {out}')

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: mkfat16.py <image_path> <file1> [file2 ...]")
        sys.exit(1)
    main(sys.argv[1], *sys.argv[2:])