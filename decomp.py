def reg(num):
    return "V" + format(num, "X")


def decomp_inst(inst):
    first_nibble = (inst & 0xF000) >> 12
    last_byte = inst & 0x00FF
    last_three_nibles = inst & 0x0FFF
    second_nibel = (inst & 0x0F00) >> 8
    second_nibel_register = reg(second_nibel)
    third_nibel = inst & 0x00F0
    third_nibel_register = reg(third_nibel)
    last_nibble = inst & 0x000F
    if first_nibble == 0:
        if inst == 0x00E0:
            return "dcl"
        if inst == 0x00EE:
            return "ret"
        else:
            return f"unsuported {last_three_nibles}"
    elif first_nibble == 1:
        return f"jmp {last_three_nibles}"
    elif first_nibble == 2:
        return f"cll {last_three_nibles}"
    elif first_nibble == 3:
        return f"je  {second_nibel_register} {last_byte}"
    elif first_nibble == 4:
        return f"jne {second_nibel_register} {last_byte}"
    elif first_nibble == 5:
        return f"je  {second_nibel_register} {last_byte}"
    elif first_nibble == 6:
        return f"mov {second_nibel_register} {last_byte}"
    elif first_nibble == 7:
        return f"add {second_nibel_register} {last_byte}"
    elif first_nibble == 8:
        if last_nibble == 0:
            return f"mov {second_nibel_register} {third_nibel_register}"
        elif last_nibble == 1:
            return f"or {second_nibel_register} {third_nibel_register}"
        elif last_nibble == 2:
            return f"and {second_nibel_register} {third_nibel_register}"
        elif last_nibble == 3:
            return f"xor {second_nibel_register} {third_nibel_register}"
        elif last_nibble == 4:
            return f"add {second_nibel_register} {third_nibel_register}"
        elif last_nibble == 5:
            return f"sub {second_nibel_register} {third_nibel_register}"
        elif last_nibble == 6:
            return f"sft {second_nibel_register}"
    elif first_nibble == 9:
        return f"jne {second_nibel_register} {third_nibel_register}"
    elif first_nibble == 10:
        return f"mov I {last_three_nibles}"
    elif first_nibble == 11:
        return f"jwo {last_three_nibles}"
    elif first_nibble == 12:
        return f"rnd {second_nibel_register} {last_three_nibles}"
    elif first_nibble == 13:
        return f"dsp {second_nibel_register} {third_nibel_register} {last_nibble}"
    elif first_nibble == 14:
        if last_byte == 0xA1:
            return f"je  KEY {second_nibel_register}"
        if last_byte == 0x07:
            return f"jne KEY {second_nibel_register}"
    elif first_nibble == 15:
        if last_byte == 0x07:
            return f"mov {second_nibel_register} TIMER"
        if last_byte == 0x0A:
            return f"awk {second_nibel_register}"
        if last_byte == 0x15:
            return f"mov TIMER {second_nibel_register}"
        if last_byte == 0x18:
            return f"mov SOUND {second_nibel_register}"
        if last_byte == 0x1E:
            return f"add I {second_nibel_register}"
        if last_byte == 0x29:
            return f"msp {second_nibel_register}"
        if last_byte == 0x33:
            return f"bcd {second_nibel_register}"
        if last_byte == 0x55:
            return f"pat {second_nibel_register}"
        if last_byte == 0x65:
            return f"lat {second_nibel_register}"


with open("output.ch8", "rb") as f:
    rom = f.read()


def byte2art(val):
    return format(val, "0>8b").replace("0", " ").replace("1", "█")


for i in range(0, len(rom), 2):
    inst = (rom[i] << 8) | rom[i + 1]  # Combine two bytes into a 16-bit instruction

    print(f"{byte2art(inst << 2)}\n{byte2art(inst)}")
    # print(
    #    f"{str(0x200 + i).zfill(4)} | {short2art(inst)} {format(inst, 'x').zfill(4)}: {decomp_inst(inst)}"
    # )
