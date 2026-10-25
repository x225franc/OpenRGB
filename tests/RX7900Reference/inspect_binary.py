"""Read-only PE disassembly around references to ADL diagnostic strings."""
import argparse
import pathlib
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable', type=pathlib.Path)
parser.add_argument('--tools', type=pathlib.Path, required=True)
parser.add_argument('--target-rva', type=lambda value: int(value, 0), action='append', default=[])
parser.add_argument('--function-rva', type=lambda value: int(value, 0))
args = parser.parse_args()
sys.path.insert(0, str(args.tools.resolve()))
import pefile
import capstone

pe = pefile.PE(str(args.executable))
base = pe.OPTIONAL_HEADER.ImageBase
needles = ['ADL_Display_WriteAndReadSMUI2C', 'adli2c.iAddress = 0xb4',
           'i2cData[0]~[24]', 'adli2c.iAction = ADL_DL_I2C_ACTIONREAD']
targets = {}
for rva in args.target_rva:
    targets[base + rva] = f'RVA {rva:x}'
for needle in needles:
    for encoding in ['ascii', 'utf-16le']:
        data = needle.encode(encoding)
        start = 0
        while True:
            offset = pe.__data__.find(data, start)
            if offset < 0:
                break
            targets[base + pe.get_rva_from_offset(offset)] = needle
            start = offset + len(data)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = True
md.skipdata = True
if args.function_rva is not None:
    for entry in pe.DIRECTORY_ENTRY_EXCEPTION:
        if entry.struct.BeginAddress <= args.function_rva < entry.struct.EndAddress:
            start, end = entry.struct.BeginAddress, entry.struct.EndAddress
            print(f'FUNCTION RVA {start:x}..{end:x}')
            for instruction in md.disasm(pe.get_data(start, end-start), base+start):
                print(f'{instruction.address:016x} {instruction.mnemonic:8s} {instruction.op_str}')
            sys.exit(0)
    raise ValueError('No runtime function contains the supplied RVA')
for section in pe.sections:
    if not section.Characteristics & 0x20000000:
        continue
    instructions = list(md.disasm(section.get_data(), base + section.VirtualAddress))
    locations = set()
    for index, instruction in enumerate(instructions):
        if instruction.id == 0:
            continue
        for operand in instruction.operands:
            if operand.type == capstone.x86.X86_OP_IMM and instruction.mnemonic == 'call' and operand.imm in targets:
                print(f'\nCALL {targets[operand.imm]} at {instruction.address:x}')
                locations.update(range(max(0, index-18), min(len(instructions), index+5)))
            if operand.type == capstone.x86.X86_OP_MEM and operand.mem.base == capstone.x86.X86_REG_RIP:
                target = instruction.address + instruction.size + operand.mem.disp
                if target in targets:
                    print(f'\nREFERENCE {targets[target]} at {instruction.address:x}')
                    locations.update(range(max(0, index-12), min(len(instructions), index+18)))
    for index in sorted(locations):
        instruction = instructions[index]
        print(f'{instruction.address:016x} {instruction.mnemonic:8s} {instruction.op_str}')
