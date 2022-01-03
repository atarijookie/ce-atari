import string, random
import time
from setproctitle import setproctitle
from fuse83 import Fuse83

setproctitle("fuse83_test")

fu = Fuse83("")

# first shortening of filename
out = fu.get_short_filename_with_ext("long filename.extension")
assert out == "LONG_FI1.EXT"
assert len(fu.long_to_short) == 1
assert len(fu.long_to_short_exts) == 1

# shortening of same filename shouldn't add items to dicts
out = fu.get_short_filename_with_ext("long filename.extension")
assert out == "LONG_FI1.EXT"
assert len(fu.long_to_short) == 1           # same as before
assert len(fu.long_to_short_exts) == 1      # same as before

# other file adds 1 to long_to_short, but reuse extension
out = fu.get_short_filename_with_ext("long filename other.extension")
assert out == "LONG_FI2.EXT"
assert len(fu.long_to_short) == 2           # 1 item added here
assert len(fu.long_to_short_exts) == 1      # same as before

# same filename as 1st, but other extension
out = fu.get_short_filename_with_ext("long filename.extension other")
assert out == "LONG_FI1.EX1"
assert len(fu.long_to_short) == 3           # 1 full filename with extension added
assert len(fu.long_to_short_exts) == 2      # 1 extension item added

# should not add anthing
out = fu.get_short_filename_with_ext("long filename.extension other")
assert out == "LONG_FI1.EX1"
assert len(fu.long_to_short) == 3           # same as before
assert len(fu.long_to_short_exts) == 2      # same as before

# test full_path - root
out = fu._full_path("/")
assert out == ""

# test full_path - one filename
out = fu._full_path("/LONG_FI1.EXT")
assert out == "long filename.extension"

# test full_path - one filename nested under other filename
out = fu._full_path("/LONG_FI1.EXT/LONG_FI1.EX1")
assert out == "long filename.extension/long filename.extension other"

print("all OK")

# generate lot of random file shorteners
count = 10000
start = time.time()
print("Generating and shortening {} filenames.".format(count))

while count > 0:
    count -= 1

    rand_filename = ''.join(random.choice(string.ascii_lowercase) for x in range(32))
    rand_ext = ''.join(random.choice(string.ascii_lowercase) for x in range(5))
    filename = rand_filename + "." + rand_ext

    fu.get_short_filename_with_ext(filename)

end = time.time()
duration = end - start

print("Done, took {:.1f} s. Endless loop follows, Ctrl+C to quit. (check memory usage)".format(duration))

import time
while True:
    time.sleep(1)
