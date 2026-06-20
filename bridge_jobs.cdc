# bridge_jobs.cdc -- operational bridge-coordinate jobs.
# The C bridge runtime consumes these witnesses and checks the expected
# coordinate against bridge64.cdc.

witness bridge-job-council-channel invariant=dyadic-triadic-closure job=bridge-coordinate window=council trits=+0-+0- expect-dyadic=101101 expect-triadic=231 claim="council channel projects committed occupancy through bridge64"
witness bridge-job-trace-window invariant=dyadic-triadic-closure job=bridge-coordinate window=trace trits=++00-- expect-dyadic=110011 expect-triadic=303 claim="trace window projects committed occupancy through bridge64"

expect witness bridge-job-council-channel
expect witness bridge-job-trace-window
