#!/usr/bin/env python3
import argparse
import contextlib
import ctypes
import datetime
import hashlib
import itertools
import json
import md5
import selectors
import signal
import struct
import subprocess

class WorkPattern:
    """A pattern of work to be finished by slave (without mask and count)"""
    def __init__(self, pattern: bytes, length: int, offset: int, count: int):
        if offset % 4:
            raise ValueError('offset not multiple of 4')
        self.pattern = pattern
        self.length = length
        self.offset = offset
        self.count = count
    def __repr__(self):
        return '%s(pattern=%r, length=%r, offset=%r)' % \
                (self.__class__.__name__, self.pattern, self.length, self.offset)

    def format_work(self, mask):
        """With mask, get the 26 integers slave understands"""
        return md5.prefix_state(self.pattern[:-64]) + mask + \
                struct.unpack('<16I', self.pattern[-64:]) + \
                (self.offset // 4, self.count)

    def to_treasure(self, value):
        """Get the treasure with 'pattern[index] set to value'"""
        return self.pattern[:self.offset] + \
               struct.pack('<I', value) + \
               self.pattern[self.offset + 4 : self.length]

    def _add_pattern(self, addend):
        value, = struct.unpack('<I', self.pattern[self.offset : self.offset + 4])
        return self.pattern[:self.offset] + \
               struct.pack('<I', value + addend) + \
               self.pattern[self.offset + 4:]

    def split(self, max_count):
        """Split the pattern into two, with the former.count <= max_count"""
        if self.count <= max_count:
            return self, None
        pat1 = WorkPattern(self.pattern, self.length, self.offset, max_count)
        pat2 = WorkPattern(self._add_pattern(max_count), self.length,
                           self.offset, self.count - max_count)
        return pat1, pat2

def nzero_mask(nzero: int):
    """Get mask from number of zeroes wanted"""
    return struct.unpack('<4I', bytes.fromhex(('f' * nzero).ljust(32, '0')))

def round_up(num: int, base: int):
    """Round `num` to multiple of `base`"""
    return (num + base - 1) // base * base

class Slave:
    """Data class about slave."""
    def __init__(self, stdin, stdout, config):
        self.stdin = stdin
        self.stdout = stdout
        self.name = config.get('name')
        self.block_size = config.get('block-size', 2 ** 32)
        self.hashes = 0
        self.last_hashes_update = None
        self.pattern_queue = []
    def register_to(self, selector):
        """Register this slave to the selector"""
        selector.register(self.stdout, selectors.EVENT_READ, data=self)
    def read_result(self):
        """Read one result from the slave"""
        result = self.stdout.readline()
        success, index = result.split(b' ')
        pattern = self.pattern_queue.pop(0)
        index = int(index) if int(success) else None

        if index is None:
            self.hashes += pattern.count
            self.last_hashes_update = datetime.datetime.now()
        return pattern, index
    def write_work(self, pattern, mask):
        """Write a work to the slave"""
        work = pattern.format_work(mask)
        self.pattern_queue.append(pattern)
        self.stdin.write(b' '.join(b'%d' % x for x in work) + b'\n')
        self.stdin.flush()

class SlaveFactory:
    """Factory creating slaves and killing them cleanly"""
    def __init__(self, slave_config):
        self.slave_config = slave_config

    @staticmethod
    def validate_config(slave_config):
        """Raise exception if slave_config is invalid"""
        if not isinstance(slave_config, list):
            raise ValueError('slave_config should be a list of slaves')
        for slave in slave_config:
            if not isinstance(slave, dict):
                raise TypeError('each slave should be a dict')
            if 'name' in slave:
                if not isinstance(slave['name'], str):
                    raise TypeError('name must be a string')
            if 'block-size' in slave:
                if not isinstance(slave['block-size'], int):
                    raise TypeError('block-size must be an integer')
                if not 1 <= slave['block-size'] <= 2 ** 32:
                    raise ValueError('block-size must be between 1 and 2 ** 32')
            if not 'command' in slave:
                raise TypeError('command is required for each slave')
            if not isinstance(slave['command'], str):
                raise TypeError('command must be a string')

            for key in slave:
                if key not in ('name', 'block-size', 'command'):
                    raise ValueError('unknown key ' + key)

    @contextlib.contextmanager
    def create_slaves(self):
        """Create slaves; this is a context manager"""
        processes = []
        slaves = []
        with contextlib.ExitStack() as stack:
            stack.callback(self.cleanup_processes, processes)

            for slave in self.slave_config:
                process = subprocess.Popen(
                    slave['command'],
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=True,
                    preexec_fn=lambda: ctypes.CDLL("libc.so.6").prctl(1, signal.SIGTERM))
                processes.append(process)

                slaves.append(Slave(process.stdin, process.stdout, slave))

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

class PatternGenerator:
    """Generate work patterns"""
    def __init__(self, prefix):
        self._gen = self.list_work_patterns(prefix)
        self._held = None

    def next(self, max_count):
        """Get next work pattern with count <= max_count"""
        if self._held is None:
            self._held = next(self._gen)
        result, self._held = self._held.split(max_count)
        return result

    @staticmethod
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
                    yield WorkPattern(bytes(pattern), length, p2, 2 ** 32)

def estimate_speed(start_time, slaves):
    """Estimated hashes per second"""
    speed = 0
    for slave in slaves:
        if slave.last_hashes_update is not None:
            time = slave.last_hashes_update - start_time
            speed += slave.hashes / time.total_seconds()
    return speed

def find_treasure(generator, mask, slave_factory):
    """Find first treasure"""

    with selectors.DefaultSelector() as selector, \
            slave_factory.create_slaves() as slaves:
        start_time = datetime.datetime.now()

        for slave in slaves:
            slave.register_to(selector)
            slave.write_work(generator.next(slave.block_size), mask)

        print('Estimated speed: None')
        while True:
            for key, _ in selector.select():
                slave = key.data
                pattern, index = slave.read_result()
                if index is not None:
                    return pattern.to_treasure(index)
                slave.write_work(generator.next(slave.block_size), mask)
                estimated_speed = estimate_speed(start_time, slaves)
                print('\033[FEstimated speed: %g hashes/second' % estimated_speed)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('slave_config', type=argparse.FileType('r'),
                        help='JSON file listing all slaves')
    parser.add_argument('-z', '--zeroes', type=int, required=True,
                        help='number of zeroes to look for (mandantory)')
    parser.add_argument('-p', '--prefix-file', type=argparse.FileType('rb'),
                        help='read prefix from PREFIX_FILE')
    parser.add_argument('-o', '--output-file', type=argparse.FileType('wb'),
                        help='write result to OUTPUT_FILE')

    args = parser.parse_args()
    with args.slave_config:
        slave_config = json.load(args.slave_config)
    SlaveFactory.validate_config(slave_config)

    prefix = b''
    if args.prefix_file is not None:
        with args.prefix_file:
            prefix = args.prefix_file.read()

    generator = PatternGenerator(prefix)
    mask = nzero_mask(args.zeroes)
    factory = SlaveFactory(slave_config)

    start_time = datetime.datetime.now()
    treasure = find_treasure(generator, mask, factory)
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
