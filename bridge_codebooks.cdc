# bridge_codebooks.cdc -- higher-arity bridge extension declarations.
# A bridge with arity n=3k maps n dyadic bits to three base-2^k digits.

witness bridge512-n9 invariant=dyadic-triadic-closure arity=9 slots=3 base=8 states=512 claim="2^9 equals 8^3"
witness bridge4096-n12 invariant=dyadic-triadic-closure arity=12 slots=3 base=16 states=4096 claim="2^12 equals 16^3"

expect witness bridge512-n9
expect witness bridge4096-n12
