// (C) 2018 Michael Toutonghi
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/*
This provides the PoW hash function for Verus, enabling CPU mining.
*/

#include "haraka.h"
#include "haraka_portable.h"

uint64_t verusclhash_port(void * random, const unsigned char buf[64], uint64_t keyMask);

