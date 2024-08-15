raw = ["5546","95900"]
degrees = int(raw[0]) // 100 * 1000000  # the ddd
print(degrees)
minutes = int(raw[0]) % 100  # the mm.
print(minutes)
minutes += int(f"{raw[1][:4]:0<4}") / 10000
print(int(f"{raw[1][:4]:0<4}"), minutes)
minutes = int(minutes / 60 * 1000000)
print(minutes)
print(degrees+minutes)