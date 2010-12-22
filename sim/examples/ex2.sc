# Usage example:
# >../traffic_gen ex2.sc 11 >! ex2.ev
# >../sim ex2.ev 10
#

# create a network consisting of 1000 nodes
# a new node joins the system each 1 sec on the average
#
# 'events num avg wjoin wfail wroute
#  - num: number of events
#  - avg: average time between events (in ms)
#  - wjoin/(wjoin+wfail+wroute): fraction of nodes joinning the network
#  - wfail/(wjoin+wfail+wroute): fraction of node failures
#  - wroute/(wjoin+wfail+wroute): fraction of route requests
# 
events 1000 1000 1 0 0 

# wait 100 sec for the network to stabilize
wait 100000

# generate 500 failures and 500 route requests; the time
# between two events is 0.2 sec
events 1000 200 0 1 1

# wait 1 sec to complete all operations 
wait 1000

# end the simulation
exit
