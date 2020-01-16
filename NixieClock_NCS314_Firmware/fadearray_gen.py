# create a c array in the form 0x00RRGGBB to support the color temp fade
import sys

r_list = list(range(0, 256))

b_list = list(reversed(range(0, 256, 1)))

g_list = list(range(0, 128))
g_list = g_list + list(reversed(range(0, 128)))

print("r_list")
print(r_list)
print("size: {}".format(len(r_list)))

print("g_list")
print(g_list)
print("size: {}".format(len(g_list)))

print("b_list")
print(b_list)
print("size: {}".format(len(b_list)))

rgb_list = [list(a) for a in zip(r_list, g_list, b_list)]

print("rgb_list")
print(rgb_list)
print("size: {}".format(len(rgb_list)))

c_array = []

for val in rgb_list:
	tmp = val[0] << 16
	tmp = tmp | val[1] << 8
	tmp = tmp | val[2]
	c_array.append(tmp)

print("long templed_array[256] = {")

for i, val in enumerate(c_array):
	sys.stdout.write("0x{:08X}, ".format(val))
	if (i + 1) % 8 == 0: 
		sys.stdout.write('\n')

print("};")