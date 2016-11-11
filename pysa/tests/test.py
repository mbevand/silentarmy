"""Simple test case that attempts to find solutions for a few actual
zcash blocks from the main chain. We have selected blocks from the
main chain that have last 12 bytes of nonce 0's to fulfill the
implementation constraint
"""
import pysa
from pysa.tests.zcashblocks import blocks
import binascii
import sys





def run_test(solver, validator):
    pass_count = 0
    print('---Running test for solver: {}---'.format(solver))
    for b_idx, b in enumerate(blocks):
        header = b[:140]
        block_test_passed = False
        print('Solving block {0}, header:{1}'.format(b_idx,
                                                     binascii.hexlify(header)))
        # send the 140 byte header to the solver
        solution_count = solver.find_solutions(header)
        print('Found {} solutions'.format(solution_count))
        # Check all solutions against the test block data
        for s_idx in range(solution_count):
            solution = solver.get_solution(s_idx)
            if validator is not None:
                solution_list = validator.minimal_to_list(solution)
                is_valid = validator.validate_solution(header, solution)
                sys.stdout.write('Comparing [validate={0}, size:{4}] solution {1}: {2}...{3} '\
                                 'with test data '.format(
                                     is_valid,
                                     s_idx, binascii.hexlify(solution[:16]),
                                     binascii.hexlify(solution[-16:]),
                                     len(set(solution_list))                        ))
            if solution == blocks[b_idx][143:143+1344]:
                print('MATCH')
                block_test_passed = True
                pass_count += 1
            else:
                print('NO MATCH')
        if not block_test_passed:
            print('--TEST FAILED for block {}--'.format(b_idx))

    print('Test results: passed {0} of {1} blocks'.format(pass_count, len(blocks)))

if __name__ == '__main__':
    validator = None
    try:
        import morpavsolver
        validator = morpavsolver.Solver()
    except ImportError:
        print('Running test without validator')
    s = pysa.Solver((0, 0), verbose=True)

    run_test(s, validator)
    # Test the validator, too
    if validator is not None:
        run_test(validator, validator)
