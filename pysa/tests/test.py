"""Simple test case that attempts to find solutions for a few actual
zcash blocks from the main chain. We have selected blocks from the
main chain that have last 12 bytes of nonce 0's to fulfill the
implementation constraint
"""
import pysa
from pysa.tests.zcashblocks import blocks
import binascii
import sys

s = pysa.Solver(verbose=True)

pass_count = 0
for b_idx, b in enumerate(blocks):
    block_test_passed = False
    print('Solving block {0}'.format(b_idx))
    # send the 140 byte header to the solver
    solution_count = s.find_solutions(b[:140])
    print('Found {} solutions'.format(solution_count))
    # Check all solutions against the test block data
    for s_idx in range(solution_count):
        solution = s.get_solution(s_idx)
        sys.stdout.write('Comparing solution {0}: {1}...{2} with test data '.format(
            s_idx, binascii.hexlify(solution[:16]), binascii.hexlify(solution[-16:])))
        if solution == blocks[b_idx][143:143+1344]:
            print('MATCH')
            block_test_passed = True
            pass_count += 1
        else:
            print('NO MATCH')
    if not block_test_passed:
        print('--TEST FAILED for block {}--'.format(b_idx))

print('Test results: passed {0} of {1} blocks'.format(pass_count, len(blocks)))
