# md5rush-simd

A backend of md5rush implementation using GCC vector extensions.

## Arguments

None for now.

See also: chrt(1), nice(1), taskset(1)

## IO format

Each task consists of 26 unsigned integers,
separated by white spaces (whatever `std::istream` accepts.)

* First 4 integers are initial MD5 state: a, b, c, d.
* The following 4 integers are mask.
* The following 16 integers are padded message.
* The following integer is the 0-based position in message to be mutated.
  If out of range, no message will be tried.
* The last integer is the number of messages to try.
  If not a multiple of natural vector size, more messages may be tried.

If it can find a message such that `md5next(state, message) & mask == 0`,
output 1 and the mutated value in the message.
Otherwise, output two 0.
