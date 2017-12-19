#!/usr/bin/env python3
import argparse
import contextlib
import ctypes
import datetime
import hashlib
import itertools
import md5
import selectors
import signal
import struct
import subprocess

class WorkPattern:
    """A pattern of work to be finished by slave (without mask and count)"""
    def __init__(self, pattern: bytes, length: int, offset: int):
        if offset % 4:
            raise ValueError('offset not multiple of 4')
        self.pattern = pattern
        self.length = length
        self.offset = offset
    def __repr__(self):
        return '%s(pattern=%r, length=%r, offset=%r)' % \
                (self.__class__.__name__, self.pattern, self.length, self.offset)

    def format_work(self, mask):
        """With mask, get the 26 integer slave understands"""
        return md5.prefix_state(self.pattern[:-64]) + mask + \
                struct.unpack('<16I', self.pattern[-64:]) + \
                (self.offset // 4, 2 ** 32)

    def to_treasure(self, index):
        return self.pattern[:self.offset] + \
               struct.pack('<I', int(index)) + \
               self.pattern[self.offset + 4 : self.length]

def nzero_mask(nzero: int):
    """Get mask from number of zeroes wanted"""
    return struct.unpack('<4I', bytes.fromhex(('f' * nzero).ljust(32, '0')))

def round_up(num: int, base: int):
    """Round `num` to multiple of `base`"""
    return (num + base - 1) // base * base

def list_work_patterns(prefix: bytes):
    """Generate possible work patterns with the given prefix"""
    for length in itertools.count(len(prefix)):
        pattern = bytearray(md5.pad(prefix.ljust(length)))
        p1 = len(prefix)
        p2 = max(round_up(len(prefix), 4), len(pattern) - 64)
        p3 = p2 + 4
        p4 = length
        if p3 > p4:
            continue

        pattern[p2:p3] = b'\0\0\0\0'
        for x in range(256 ** (p2 - p1)):
            pattern[p1:p2] = x.to_bytes(p2 - p1, byteorder='little')
            for y in range(256 ** (p4 - p3)):
                pattern[p3:p4] = y.to_bytes(p4 - p3, byteorder='little')
                yield WorkPattern(bytes(pattern), length, p2)

class Slave:
    """Data class about slave."""
    def __init__(self, stdin, stdout):
        self.stdin = stdin
        self.stdout = stdout
        self.pattern_queue = []
    def register_to(self, selector):
        """Register this slave to the selector"""
        selector.register(self.stdout, selectors.EVENT_READ, data=self)
    def read_result(self):
        """Read one result from the slave"""
        result = self.stdout.readline()
        success, index = result.split(b' ')
        pattern = self.pattern_queue.pop(0)
        if int(success):
            return pattern, int(index)
        else:
            return pattern, None
    def write_work(self, pattern, mask):
        """Write a work to the slave"""
        work = pattern.format_work(mask)
        self.pattern_queue.append(pattern)
        self.stdin.write(b' '.join(b'%d' % x for x in work) + b'\n')
        self.stdin.flush()

class SlaveFactory:
    """Factory creating slaves and killing them cleanly"""
    def __init__(self, commands):
        self.commands = commands

    @contextlib.contextmanager
    def create_slaves(self):
        """Create slaves; this is a context manager"""
        processes = []
        slaves = []
        with contextlib.ExitStack() as stack:
            stack.callback(self.cleanup_processes, processes)

            for command in self.commands:
                process = subprocess.Popen(
                    command,
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=True,
                    preexec_fn=lambda: ctypes.CDLL("libc.so.6").prctl(1, signal.SIGTERM))
                processes.append(process)

                slaves.append(Slave(process.stdin, process.stdout))

            yield slaves

    @staticmethod
    def cleanup_processes(processes):
        """Clean up a list of processes"""
        for process in processes:
            process.stdin.close()
            process.stdout.close()
        for process in processes:
            process.terminate()
        for process in processes:
            process.wait()

def find_treasure(generator, mask, slave_factory):
    """Find first treasure"""
    with selectors.DefaultSelector() as selector, \
            slave_factory.create_slaves() as slaves:
        for slave in slaves:
            slave.register_to(selector)
            slave.write_work(next(generator), mask)

        index = None
        while index is None:
            for key, _ in selector.select():
                slave = key.data
                pattern, index = slave.read_result()
                if index is not None:
                    return pattern.to_treasure(index)
                slave.write_work(next(generator), mask)

def main():
    parser = argparse.ArgumentParser(fromfile_prefix_chars='@')
    parser.add_argument('-z', '--zeroes', type=int, required=True,
                        help='number of zeroes to look for (mandantory)')
    parser.add_argument('-p', '--prefix-file', type=argparse.FileType('rb'),
                        help='read prefix from PREFIX_FILE')
    parser.add_argument('-o', '--output-file', type=argparse.FileType('wb'),
                        help='write result to OUTPUT_FILE')
    parser.add_argument('-c', '--command', required=True, action='append',
                        dest='commands',
                        help='command to launch slave')

    args = parser.parse_args()

    prefix = b''
    if args.prefix_file is not None:
        prefix = args.prefix_file.read()
    generator = list_work_patterns(prefix)

    mask = nzero_mask(args.zeroes)
    commands = args.commands


    start_time = datetime.datetime.now()
    treasure = find_treasure(generator, mask, SlaveFactory(commands))
    end_time = datetime.datetime.now()
    time_used = end_time - start_time

    print('Treasure found!')
    print('Treasure (repr):', repr(treasure))
    print('Treasure (hex):', treasure.hex())
    print('Hash:', hashlib.md5(treasure).hexdigest())
    print('Time used:', time_used)
    if args.output_file:
        args.output_file.write(treasure)
        print('Treasure saved to', args.output_file.name)

if __name__ == '__main__':
    main()
